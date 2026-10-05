#!/usr/bin/env python3
"""Hake GeoDesk runtime dependency manifest for an installed package.

Walks the *installed* tree (NSIS install dir, the root filesystem after
`apt install ./hake-geodesk.deb`, or the .app bundle), builds the native
dependency graph from the actual binaries and checks it against
scripts/ci/hake_runtime_deps.json.

  Windows: pure-Python PE import parser (import + delay-import tables)
  Linux:   ldd (resolution) + readelf -d (RPATH/RUNPATH) + dpkg -S (ownership)
  macOS:   otool -l (LC_LOAD_DYLIB / LC_RPATH)

Exit status is non-zero when a required file is missing, a dependency in the
required closure is unresolved, or a dependency resolves to a forbidden
location (build tree, vcpkg, Homebrew, /usr/local, ...).

Must stay compatible with Python 3.9 (macOS /usr/bin/python3).
"""

from __future__ import annotations

import argparse
import datetime
import json
import mmap
import os
import plistlib
import struct
import subprocess
import sys
from collections import deque
from pathlib import Path

HERE = Path(__file__).resolve().parent

REPORT: dict = {
    "tool": "hake_dependency_manifest",
    "platform": None,
    "root": None,
    "generated_utc": None,
    "versions": {},
    "checks": {},
    "modules": {},
    "external": {},
    "packages": {},
    "duplicates": {},
    "errors": [],
    "warnings": [],
}


def log(msg: str) -> None:
    print(msg, flush=True)


def error(msg: str) -> None:
    REPORT["errors"].append(msg)
    log(f"ERROR: {msg}")


def warn(msg: str) -> None:
    REPORT["warnings"].append(msg)
    log(f"WARN: {msg}")


def detect_platform() -> str:
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def load_model(path: Path, platform: str) -> tuple[dict, dict]:
    model = json.loads(path.read_text(encoding="utf-8"))
    return model, model["platforms"][platform]


def bundle_executable(root: Path) -> str | None:
    plist = root / "Contents" / "Info.plist"
    if not plist.is_file():
        return None
    with plist.open("rb") as fh:
        return plistlib.load(fh).get("CFBundleExecutable")


def expand(root: Path, pattern: str) -> list[Path]:
    if pattern == "@CFBundleExecutable":
        name = bundle_executable(root)
        if not name:
            return []
        candidate = root / "Contents" / "MacOS" / name
        return [candidate] if candidate.exists() else []
    rel = pattern.lstrip("/")
    if not any(ch in rel for ch in "*?["):
        p = root / rel
        return [p] if p.exists() else []
    return sorted(p for p in root.glob(rel) if p.exists())


def rel(root: Path, p: Path | str) -> str:
    try:
        return Path(p).relative_to(root).as_posix()
    except ValueError:
        return str(p)


def check_required(root: Path, info: dict) -> None:
    for group in ("executables", "data", "qt_plugins", "gdal_tools", "licenses"):
        results = {}
        for pattern in info.get(group, []):
            hits = expand(root, pattern)
            results[pattern] = [rel(root, h) for h in hits]
            if not hits:
                error(f"[{group}] required path missing: {pattern}")
        REPORT["checks"][group] = results
        log(f"[{group}] {sum(1 for v in results.values() if v)}/{len(results)} present")


# --------------------------------------------------------------------------
# Windows (PE)
# --------------------------------------------------------------------------

def pe_imports(path: Path) -> tuple[list[str], list[str]] | None:
    """Return (imports, delay_imports) DLL names, or None if not a PE image."""
    try:
        with path.open("rb") as fh:
            if fh.read(2) != b"MZ":
                return None
            fh.seek(0)
            data = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    except (OSError, ValueError):
        return None
    try:
        if len(data) < 0x40:
            return None
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            return None
        coff = pe + 4
        nsect = struct.unpack_from("<H", data, coff + 2)[0]
        optsize = struct.unpack_from("<H", data, coff + 16)[0]
        opt = coff + 20
        magic = struct.unpack_from("<H", data, opt)[0]
        if magic == 0x20B:
            dd = opt + 112
        elif magic == 0x10B:
            dd = opt + 96
        else:
            return None
        num_dd = struct.unpack_from("<I", data, dd - 4)[0]
        sections = []
        sec = opt + optsize
        for i in range(nsect):
            vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, sec + i * 40 + 8)
            sections.append((va, max(vsize, rawsize), rawptr))

        def rva2off(rva: int) -> int | None:
            for va, size, raw in sections:
                if va <= rva < va + size:
                    return rva - va + raw
            return None

        def cstr(off: int) -> str:
            end = data.find(b"\0", off)
            return bytes(data[off:end]).decode("ascii", "replace")

        out: list[list[str]] = [[], []]
        # (data directory index, descriptor size, offset of DLL name RVA)
        for slot, (idx, entsize, name_off) in enumerate(((1, 20, 12), (13, 32, 4))):
            if num_dd <= idx:
                continue
            rva, _size = struct.unpack_from("<II", data, dd + idx * 8)
            off = rva2off(rva) if rva else None
            while off is not None and off + entsize <= len(data):
                entry = bytes(data[off:off + entsize])
                if entry == b"\0" * entsize:
                    break
                nrva = struct.unpack_from("<I", entry, name_off)[0]
                if nrva == 0:
                    break
                noff = rva2off(nrva)
                if noff is not None:
                    out[slot].append(cstr(noff))
                off += entsize
        return out[0], out[1]
    except (struct.error, ValueError):
        return None
    finally:
        data.close()


def analyze_windows(root: Path, info: dict) -> None:
    system32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
    search_dirs = [root / d for d in info.get("dll_search_dirs", ["bin"])]
    must_bundle = {n.lower() for n in info.get("must_bundle_dlls", [])}

    pe_files: list[Path] = []
    by_name: dict[str, list[Path]] = {}
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            if name.lower().endswith((".exe", ".dll", ".pyd")):
                p = Path(dirpath) / name
                pe_files.append(p)
                by_name.setdefault(name.lower(), []).append(p)

    for name, paths in by_name.items():
        if len(paths) > 1 and name.endswith(".dll"):
            REPORT["duplicates"][name] = [rel(root, p) for p in paths]

    roots: set[Path] = set()
    for group in ("executables", "qt_plugins", "native_module_globs"):
        for pattern in info.get(group, []):
            roots.update(p for p in expand(root, pattern) if p.is_file())

    def resolve(dep: str, module: Path) -> tuple[str, Path | None]:
        low = dep.lower()
        if low.startswith(("api-ms-win-", "ext-ms-")):
            return "apiset", None
        for d in [module.parent] + search_dirs:
            cand = d / dep
            if cand.is_file():
                return "bundled", cand
        sys_cand = system32 / dep
        if sys_cand.is_file():
            return "system", sys_cand
        return "missing", None

    closure: set[Path] = set()
    queue = deque(sorted(roots))
    while queue:
        mod = queue.popleft()
        if mod in closure:
            continue
        closure.add(mod)
        parsed = pe_imports(mod)
        if parsed is None:
            continue
        for dep in parsed[0]:
            kind, target = resolve(dep, mod)
            if kind == "bundled" and target not in closure:
                queue.append(target)

    scanned = 0
    for mod in sorted(pe_files):
        parsed = pe_imports(mod)
        if parsed is None:
            continue
        scanned += 1
        required = mod in closure
        imports, delay = parsed
        entry = {"required": required, "imports": {}, "delay_imports": {}}
        for table, deps in (("imports", imports), ("delay_imports", delay)):
            for dep in deps:
                kind, target = resolve(dep, mod)
                entry[table][dep] = rel(root, target) if kind == "bundled" else kind
                if kind == "system":
                    REPORT["external"][dep.lower()] = str(target)
                    if dep.lower() in must_bundle:
                        msg = f"{rel(root, mod)}: {dep} resolves only from {target}; it must be bundled"
                        error(msg) if required else warn(msg)
                elif kind == "missing":
                    msg = f"{rel(root, mod)}: unresolved {table[:-1].replace('_', '-')} {dep}"
                    if required and table == "imports":
                        error(msg)
                    else:
                        warn(msg)
        REPORT["modules"][rel(root, mod)] = entry

    for name in must_bundle:
        if not any((d / name).is_file() for d in search_dirs):
            error(f"must-bundle DLL {name} not found in {', '.join(rel(root, d) for d in search_dirs)}")

    log(f"PE modules scanned: {scanned}; required closure: {len(closure)}; "
        f"system DLLs referenced: {len(REPORT['external'])}")


# --------------------------------------------------------------------------
# Linux (ELF)
# --------------------------------------------------------------------------

def is_elf(path: Path) -> bool:
    try:
        with path.open("rb") as fh:
            return fh.read(4) == b"\x7fELF"
    except OSError:
        return False


def run(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, check=False)


def ldd(path: Path) -> dict[str, str | None]:
    proc = run(["ldd", str(path)])
    deps: dict[str, str | None] = {}
    for line in proc.stdout.splitlines():
        line = line.strip()
        if "=>" in line:
            name, _, rest = line.partition("=>")
            rest = rest.strip()
            if rest.startswith("not found"):
                deps[name.strip()] = None
            else:
                deps[name.strip()] = rest.split(" (")[0].strip()
        elif line.startswith("/"):
            p = line.split(" (")[0].strip()
            deps[os.path.basename(p)] = p
    return deps


def elf_runpaths(path: Path) -> list[str]:
    proc = run(["readelf", "-d", str(path)])
    paths: list[str] = []
    for line in proc.stdout.splitlines():
        if "(RPATH)" in line or "(RUNPATH)" in line:
            value = line.split("[", 1)[-1].rstrip("]")
            paths.extend(v for v in value.split(":") if v)
    return paths


def analyze_linux(root: Path, info: dict, forbidden: list[str]) -> None:
    files: list[Path] = []
    pkg = info.get("deb_package")
    if pkg and run(["dpkg-query", "-W", pkg]).returncode == 0:
        proc = run(["dpkg", "-L", pkg])
        for line in proc.stdout.splitlines():
            p = Path(line.strip())
            if p.is_file() and not p.is_symlink() and is_elf(p):
                files.append(p)
        log(f"ELF files owned by {pkg}: {len(files)}")
    else:
        warn(f"Debian package {pkg!r} is not installed; scanning required globs only")

    for group in ("executables", "qt_plugins", "native_module_globs"):
        for pattern in info.get(group, []):
            for p in expand(root, pattern):
                if p.is_file() and not p.is_symlink() and is_elf(p) and p not in files:
                    files.append(p)

    have_readelf = run(["sh", "-c", "command -v readelf"]).returncode == 0
    if not have_readelf:
        warn("readelf not available; RPATH/RUNPATH audit skipped")

    resolved_libs: set[str] = set()
    for f in sorted(files):
        deps = ldd(f)
        entry: dict = {"required": True, "imports": {}, "runpath": []}
        for name, target in deps.items():
            entry["imports"][name] = target
            if target is None:
                error(f"{f}: unresolved shared library {name}")
                continue
            resolved_libs.add(target)
            for bad in forbidden:
                if target == bad or target.startswith(bad.rstrip("/") + "/"):
                    error(f"{f}: {name} resolves to forbidden location {target}")
        if have_readelf:
            entry["runpath"] = elf_runpaths(f)
            for rp in entry["runpath"]:
                if rp.startswith("$ORIGIN"):
                    continue
                if any(rp.startswith(b) for b in forbidden) or not rp.startswith(("/usr/lib", "/lib")):
                    error(f"{f}: RPATH/RUNPATH entry {rp} points outside the system library tree")
        REPORT["modules"][str(f)] = entry

    if resolved_libs and run(["sh", "-c", "command -v dpkg"]).returncode == 0:
        owners: dict[str, str] = {}
        for lib in sorted(resolved_libs):
            candidates = [lib, os.path.realpath(lib)]
            candidates += ["/usr" + c for c in list(candidates) if c.startswith("/lib")]
            for cand in candidates:
                proc = run(["dpkg", "-S", cand])
                if proc.returncode == 0 and proc.stdout:
                    owners[lib] = proc.stdout.split(":", 1)[0].strip()
                    break
            else:
                owners[lib] = "(unowned)"
                warn(f"{lib} is not owned by any Debian package")
        REPORT["packages"] = owners
        log(f"Shared libraries resolved: {len(owners)} across "
            f"{len(set(owners.values()))} Debian packages")


# --------------------------------------------------------------------------
# macOS (Mach-O)
# --------------------------------------------------------------------------

MACHO_MAGICS = {0xFEEDFACE, 0xFEEDFACF, 0xCEFAEDFE, 0xCFFAEDFE, 0xCAFEBABE, 0xBEBAFECA}


def is_macho(path: Path) -> bool:
    try:
        with path.open("rb") as fh:
            head = fh.read(8)
    except OSError:
        return False
    if len(head) < 8:
        return False
    magic = int.from_bytes(head[:4], "big")
    if magic == 0xCAFEBABE:
        # Java class files share this magic; fat Mach-O has a small arch count.
        return int.from_bytes(head[4:8], "big") < 20
    return magic in MACHO_MAGICS


def otool_load_commands(path: Path) -> tuple[list[str], list[str]]:
    proc = run(["otool", "-l", str(path)])
    deps: list[str] = []
    rpaths: list[str] = []
    lines = proc.stdout.splitlines()
    for i, line in enumerate(lines):
        line = line.strip()
        if not line.startswith("cmd LC_"):
            continue
        cmd = line.split()[-1]
        if i + 2 >= len(lines):
            continue
        detail = lines[i + 2].strip()
        if cmd in ("LC_LOAD_DYLIB", "LC_LOAD_WEAK_DYLIB", "LC_REEXPORT_DYLIB") and detail.startswith("name "):
            deps.append(detail.split()[1])
        elif cmd == "LC_RPATH" and detail.startswith("path "):
            rpaths.append(detail.split()[1])
    return sorted(set(deps)), rpaths


def analyze_macos(root: Path, info: dict) -> None:
    allowed_sys = tuple(info.get("allowed_system_prefixes", ["/usr/lib/", "/System/Library/"]))
    macos_dir = root / "Contents" / "MacOS"

    machos: list[Path] = []
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            p = Path(dirpath) / name
            if not p.is_symlink() and is_macho(p):
                machos.append(p)

    commands = {p: otool_load_commands(p) for p in machos}
    exe_rpaths: list[str] = []
    for p in machos:
        if p.parent == macos_dir:
            exe_rpaths.extend(r.replace("@loader_path", str(macos_dir))
                              .replace("@executable_path", str(macos_dir)) for r in commands[p][1])

    def resolve(dep: str, module: Path, rpaths: list[str]) -> tuple[str, Path | None]:
        if dep.startswith(allowed_sys):
            return "system", None
        if dep.startswith("@executable_path/"):
            cand = macos_dir / dep[len("@executable_path/"):]
            return ("bundled", cand.resolve()) if cand.exists() else ("missing", None)
        if dep.startswith("@loader_path/"):
            cand = module.parent / dep[len("@loader_path/"):]
            return ("bundled", cand.resolve()) if cand.exists() else ("missing", None)
        if dep.startswith("@rpath/"):
            tail = dep[len("@rpath/"):]
            own = [r.replace("@loader_path", str(module.parent))
                   .replace("@executable_path", str(macos_dir)) for r in rpaths]
            for rp in own + exe_rpaths:
                cand = Path(rp) / tail
                if cand.exists():
                    return ("bundled", cand.resolve()) if str(cand.resolve()).startswith(str(root)) \
                        else ("forbidden", cand)
            return "missing", None
        return "forbidden", Path(dep)

    roots: set[Path] = set()
    for group in ("executables", "qt_plugins", "native_module_globs"):
        for pattern in info.get(group, []):
            roots.update(p.resolve() for p in expand(root, pattern) if p.is_file())

    resolved_cache: dict[Path, dict[str, tuple[str, Path | None]]] = {}
    for p in machos:
        deps, rpaths = commands[p]
        resolved_cache[p.resolve()] = {d: resolve(d, p, rpaths) for d in deps}

    closure: set[Path] = set()
    queue = deque(sorted(roots))
    while queue:
        mod = queue.popleft()
        if mod in closure:
            continue
        closure.add(mod)
        for kind, target in resolved_cache.get(mod, {}).values():
            if kind == "bundled" and target is not None and target not in closure:
                queue.append(target)

    for p in machos:
        key = p.resolve()
        required = key in closure
        deps, rpaths = commands[p]
        entry = {"required": required, "imports": {}, "rpaths": rpaths}
        for dep, (kind, target) in resolved_cache[key].items():
            entry["imports"][dep] = rel(root, target) if kind == "bundled" else kind
            if kind == "system":
                REPORT["external"][dep] = dep
            elif kind in ("missing", "forbidden"):
                where = f" ({target})" if target else ""
                msg = f"{rel(root, p)}: {kind} dependency {dep}{where}"
                error(msg) if required else warn(msg)
        for rp in rpaths:
            if rp.startswith("/") and not rp.startswith(allowed_sys):
                msg = f"{rel(root, p)}: absolute LC_RPATH {rp} outside the bundle"
                error(msg) if required else warn(msg)
        REPORT["modules"][rel(root, p)] = entry

    log(f"Mach-O files scanned: {len(machos)}; required closure: {len(closure)}")


# --------------------------------------------------------------------------

def merge_smoke_reports(paths: list[Path]) -> None:
    for path in paths:
        if not path.is_file():
            warn(f"smoke report {path} not found")
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        for key, value in (data.get("versions") or {}).items():
            REPORT["versions"].setdefault(key, value)
        if data.get("python_modules"):
            REPORT.setdefault("python_modules", {}).update(data["python_modules"])


def print_summary(root: Path) -> None:
    v = REPORT["versions"]
    log("")
    log("Hake GeoDesk Runtime Dependency Report")
    log("")
    log(f"Platform: {REPORT['platform']}")
    log(f"Root:     {root}")
    for key in ("python", "qgis", "qt", "gdal", "proj", "geos"):
        if key in v:
            log(f"{key.upper() if len(key) <= 4 else key.capitalize()}: {v[key]}")
    required = [m for m, e in REPORT["modules"].items() if e.get("required")]
    log("")
    log(f"Native modules in required closure: {len(required)}")
    log(f"External (OS-provided) libraries referenced: {len(REPORT['external'])}")
    if REPORT["packages"]:
        log(f"Debian packages providing resolved libraries: {len(set(REPORT['packages'].values()))}")
    log("Required data:")
    for pattern, hits in REPORT["checks"].get("data", {}).items():
        log(f"    {pattern}: {'OK' if hits else 'MISSING'}")
    log(f"Errors: {len(REPORT['errors'])}  Warnings: {len(REPORT['warnings'])}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--platform", choices=("windows", "linux", "macos"), default=detect_platform())
    parser.add_argument("--deps", type=Path, default=HERE / "hake_runtime_deps.json")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--forbid", action="append", default=[],
                        help="Additional forbidden path prefix (build tree, source tree)")
    parser.add_argument("--smoke-report", action="append", default=[], type=Path,
                        help="Runtime smoke JSON report whose versions are merged into the manifest")
    args = parser.parse_args()

    root = args.root.resolve()
    _model, info = load_model(args.deps, args.platform)
    REPORT["platform"] = args.platform
    REPORT["root"] = str(root)
    REPORT["generated_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()

    log(f"=== Hake GeoDesk dependency manifest ({args.platform}) root={root} ===")
    if not root.is_dir():
        error(f"install root {root} does not exist")
    else:
        check_required(root, info)
        forbidden = list(info.get("forbidden_prefixes", [])) + args.forbid
        if args.platform == "windows":
            analyze_windows(root, info)
        elif args.platform == "linux":
            analyze_linux(root, info, forbidden)
        else:
            analyze_macos(root, info)

    merge_smoke_reports(args.smoke_report)
    print_summary(root)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(REPORT, indent=2, sort_keys=True), encoding="utf-8")
    log(f"Wrote {args.out}")
    return 1 if REPORT["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
