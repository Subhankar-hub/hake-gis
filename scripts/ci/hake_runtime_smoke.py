#!/usr/bin/env python3
"""Hake GeoDesk packaged-runtime smoke test (Windows, Linux, macOS).

Run with the interpreter the *installed* product uses:
  Windows: <install>\\bin\\python.exe
  macOS:   <App>.app/Contents/MacOS/python
  Linux:   /usr/bin/python3 (the .deb depends on the distribution Python)

The caller is expected to sanitize PATH / PYTHON* / PROJ_* / GDAL_* first;
this script then establishes only the paths the product itself provides
(scripts/ci/hake_runtime_deps.json) and verifies:
  - interpreter, sys.path and every imported module resolve inside the product
  - runtime Python modules and native extensions import
  - no native library is loaded from a forbidden location (PATH decoys,
    Homebrew, vcpkg, build tree, /usr/local)
  - PROJ finds the packaged proj.db; EPSG 4326/2193/3978/5936/5482 resolve via
    osr and QgsCoordinateReferenceSystem; a 4326 -> 2193 transform works
  - GeoPackage creation
  - the packaged gdal_polygonize utility (child process)
  - Processing gdal:polygonize and native:reprojectlayer in a headless
    QgsApplication

Must stay compatible with Python 3.9.
"""

from __future__ import annotations

import argparse
import importlib
import json
import os
import site
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent

REPORT: dict = {
    "ok": False,
    "label": None,
    "platform": None,
    "install_root": None,
    "sys_executable": None,
    "sys_prefix": None,
    "sys_path": [],
    "env": {},
    "versions": {},
    "python_modules": {},
    "optional_modules": {},
    "loaded_libraries_outside_root": [],
    "proj_db_locations": [],
    "proj_search_paths": [],
    "epsg": {},
    "transform": None,
    "gpkg": None,
    "polygonize": None,
    "processing": None,
    "errors": [],
    "warnings": [],
}

ENV_KEYS = ("PATH", "PYTHONHOME", "PYTHONPATH", "PROJ_DATA", "PROJ_LIB", "GDAL_DATA",
            "GDAL_DRIVER_PATH", "QT_PLUGIN_PATH", "QGIS_PREFIX_PATH", "QT_QPA_PLATFORM")
LEAK_TOKENS = ("conda", "venv", "virtualenv", "hostedtoolcache", "vcpkg_installed",
               "/opt/homebrew", "/usr/local/cellar", "pyenv")


def log(msg: str) -> None:
    print(msg, flush=True)


def fail(msg: str) -> None:
    REPORT["errors"].append(msg)
    log(f"FAIL: {msg}")


def warn(msg: str) -> None:
    REPORT["warnings"].append(msg)
    log(f"WARN: {msg}")


def detect_platform() -> str:
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    return "linux"


def norm(p: str | Path) -> str:
    return os.path.normcase(os.path.realpath(str(p)))


def is_under(child: str | Path, parent: str | Path) -> bool:
    c, p = norm(child), norm(parent)
    return c == p or c.startswith(p.rstrip(os.sep) + os.sep)


def path_str(value: object) -> str | None:
    if value is None:
        return None
    if isinstance(value, (bytes, bytearray)):
        for encoding in ("utf-8", "mbcs", "cp1252", "latin-1"):
            try:
                return bytes(value).decode(encoding)
            except (LookupError, UnicodeDecodeError):
                continue
    return str(value)


class Context:
    def __init__(self, args: argparse.Namespace):
        self.platform: str = args.platform
        self.root = args.install_root.resolve()
        model = json.loads(args.deps.read_text(encoding="utf-8"))
        self.model = model
        self.info = model["platforms"][self.platform]
        self.prefix = (self.root / self.info.get("prefix", ".")).resolve()
        self.module_roots = [(self.root / r).resolve() for r in self.info.get("allowed_module_roots", ["."])]
        self.forbidden = [f for f in self.info.get("forbidden_prefixes", []) + args.forbid if f]
        self.bin_dir = self.root / "bin" if self.platform == "windows" else None

    def module_allowed(self, path: str) -> bool:
        if any(is_under(path, r) for r in self.module_roots):
            return not any(is_under(path, f) for f in self.forbidden)
        return False


# --------------------------------------------------------------------------

def check_interpreter(ctx: Context) -> None:
    exe = Path(sys.executable).resolve()
    REPORT["sys_executable"] = str(exe)
    REPORT["sys_prefix"] = sys.prefix
    log(f"sys.executable={exe}")
    log(f"sys.prefix={sys.prefix}")
    if ctx.platform == "linux":
        expected = Path(ctx.info["python_executable"]).resolve()
        if exe != expected:
            fail(f"Interpreter {exe} is not the distribution Python {expected}")
        if norm(sys.prefix) != norm("/usr"):
            fail(f"sys.prefix {sys.prefix} is not /usr (virtualenv/conda?)")
    else:
        if not is_under(exe, ctx.root):
            fail(f"Interpreter is not inside the install root: {exe}")
        if not is_under(sys.prefix, ctx.root):
            fail(f"sys.prefix {sys.prefix} is outside the install root")


def check_sys_path(ctx: Context) -> None:
    REPORT["sys_path"] = list(sys.path)
    user_site = site.getusersitepackages() if hasattr(site, "getusersitepackages") else None
    for entry in sys.path:
        if not entry:
            continue
        low = entry.replace("\\", "/").lower()
        if user_site and norm(entry) == norm(user_site):
            fail(f"user site-packages on sys.path: {entry}")
        if any(tok in low for tok in LEAK_TOKENS):
            fail(f"developer environment on sys.path: {entry}")
        if ctx.platform != "linux" and os.path.exists(entry) and not is_under(entry, ctx.root):
            fail(f"sys.path entry outside install root: {entry}")


def module_file(mod) -> str | None:
    f = getattr(mod, "__file__", None)
    if f:
        return f
    spec = getattr(mod, "__spec__", None)
    return getattr(spec, "origin", None) if spec else None


def import_modules(ctx: Context) -> None:
    py = ctx.model["python"]
    for name in py["runtime"] + py["native_extensions"]:
        if name in REPORT["python_modules"]:
            continue
        try:
            mod = importlib.import_module(name)
        except Exception as exc:  # noqa: BLE001
            REPORT["python_modules"][name] = f"IMPORT FAILED: {exc}"
            fail(f"required Python module {name} failed to import: {exc}")
            continue
        location = module_file(mod)
        REPORT["python_modules"][name] = location or "(builtin)"
        if location and location not in ("built-in", "frozen") and not ctx.module_allowed(location):
            fail(f"{name} imported from outside the product: {location}")
        log(f"import {name}: {location}")
    for name in py["optional"]:
        try:
            mod = importlib.import_module(name)
            REPORT["optional_modules"][name] = module_file(mod) or "(builtin)"
        except Exception as exc:  # noqa: BLE001
            REPORT["optional_modules"][name] = f"unavailable: {exc}"
            warn(f"optional Python module {name} unavailable: {exc}")


def loaded_libraries(platform: str) -> list[str]:
    if platform == "linux":
        libs = set()
        try:
            with open("/proc/self/maps", encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    parts = line.split(None, 5)
                    if len(parts) == 6 and ".so" in parts[5]:
                        libs.add(parts[5].strip())
        except OSError:
            pass
        return sorted(libs)
    import ctypes
    if platform == "macos":
        libsystem = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
        libsystem._dyld_image_count.restype = ctypes.c_uint32
        libsystem._dyld_get_image_name.restype = ctypes.c_char_p
        libsystem._dyld_get_image_name.argtypes = [ctypes.c_uint32]
        return sorted({libsystem._dyld_get_image_name(i).decode("utf-8", "replace")
                       for i in range(libsystem._dyld_image_count())})
    from ctypes import wintypes
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
                                           ctypes.POINTER(wintypes.DWORD), wintypes.DWORD]
    psapi.GetModuleFileNameExW.argtypes = [wintypes.HANDLE, wintypes.HMODULE, wintypes.LPWSTR, wintypes.DWORD]
    psapi.GetModuleFileNameExW.restype = wintypes.DWORD
    process = kernel32.GetCurrentProcess()
    needed = wintypes.DWORD()
    if not psapi.EnumProcessModulesEx(process, None, 0, ctypes.byref(needed), 0x03):
        return []
    modules = (wintypes.HMODULE * (needed.value // ctypes.sizeof(wintypes.HMODULE)))()
    if not psapi.EnumProcessModulesEx(process, ctypes.byref(modules), ctypes.sizeof(modules),
                                      ctypes.byref(needed), 0x03):
        return []
    buf = ctypes.create_unicode_buffer(32768)
    out = set()
    for handle in modules:
        if handle and psapi.GetModuleFileNameExW(process, handle, buf, len(buf)):
            out.add(buf.value)
    return sorted(out)


def check_loaded_libraries(ctx: Context) -> None:
    libs = loaded_libraries(ctx.platform)
    log(f"Loaded native libraries: {len(libs)}")
    if not libs:
        warn("could not enumerate loaded native libraries")
        return
    if ctx.platform == "windows":
        system_root = os.environ.get("SystemRoot", r"C:\Windows")
        allowed = [ctx.root, system_root]
        bad = [lib for lib in libs if not any(is_under(lib, a) for a in allowed)]
    elif ctx.platform == "macos":
        sys_prefixes = tuple(ctx.info.get("allowed_system_prefixes", ["/usr/lib/", "/System/Library/"]))
        bad = [lib for lib in libs if not lib.startswith(sys_prefixes) and not is_under(lib, ctx.root)]
    else:
        bad = [lib for lib in libs if any(is_under(lib, f) for f in ctx.forbidden)]
    REPORT["loaded_libraries_outside_root"] = bad
    for lib in bad:
        fail(f"native library loaded from outside the product: {lib}")


# --------------------------------------------------------------------------

def collect_versions(gdal, osr) -> None:
    v = REPORT["versions"]
    v["python"] = sys.version.split()[0]
    v["gdal"] = gdal.VersionInfo("--version")
    v["proj"] = ".".join(str(x) for x in (osr.GetPROJVersionMajor(), osr.GetPROJVersionMinor(),
                                            osr.GetPROJVersionMicro()))
    try:
        from qgis.core import Qgis
        v["qgis"] = Qgis.version()
        if hasattr(Qgis, "geosVersion"):
            v["geos"] = Qgis.geosVersion()
    except Exception as exc:  # noqa: BLE001
        fail(f"could not read QGIS version: {exc}")
    try:
        from PyQt6.QtCore import PYQT_VERSION_STR, QT_VERSION_STR
        v["qt"] = QT_VERSION_STR
        v["pyqt"] = PYQT_VERSION_STR
    except Exception as exc:  # noqa: BLE001
        fail(f"could not read Qt version: {exc}")
    for key, value in v.items():
        log(f"{key}: {value}")


def check_proj(ctx: Context, osr) -> None:
    try:
        search = [path_str(p) for p in osr.GetPROJSearchPaths()]
    except Exception as exc:  # noqa: BLE001
        search = []
        fail(f"GetPROJSearchPaths failed: {exc}")
    search = [s for s in search if s]
    REPORT["proj_search_paths"] = search
    hits = []
    for sp in search + [os.environ.get("PROJ_DATA"), os.environ.get("PROJ_LIB")]:
        if sp and (Path(sp) / "proj.db").is_file() and str(Path(sp) / "proj.db") not in hits:
            hits.append(str(Path(sp) / "proj.db"))
    REPORT["proj_db_locations"] = hits
    log(f"PROJ search paths: {search}")
    log(f"proj.db reachable via search paths/env: {hits}")
    expected = [p for pattern in ctx.info["data"] if pattern.endswith("proj.db")
                for p in [ctx.root / pattern.lstrip("/")]]
    for p in expected:
        if not p.is_file():
            fail(f"packaged proj.db missing: {p}")
    if ctx.platform != "linux" and not any(is_under(h, ctx.root) for h in hits):
        warn("PROJ search paths do not list the packaged proj.db yet (QgsApplication init adds it)")


def init_qgis(ctx: Context):
    from qgis.core import QgsApplication
    app = QgsApplication([], False)
    QgsApplication.setPrefixPath(str(ctx.prefix), True)
    app.initQgis()
    log(f"QgsApplication prefixPath={QgsApplication.prefixPath()} pkgDataPath={QgsApplication.pkgDataPath()}")
    for key in ("PROJ_DATA", "PROJ_LIB", "GDAL_DATA"):
        log(f"after initQgis {key}={os.environ.get(key)}")
    return app


def check_epsg(ctx: Context, osr) -> None:
    from qgis.core import (QgsCoordinateReferenceSystem, QgsCoordinateTransform,
                           QgsPointXY, QgsProject)
    for code in ctx.model["epsg"]:
        result = {}
        try:
            srs = osr.SpatialReference()
            rc = srs.ImportFromEPSG(code)
            result["osr"] = "ok" if rc == 0 and srs.ExportToWkt() else f"ImportFromEPSG={rc}"
        except Exception as exc:  # noqa: BLE001
            result["osr"] = str(exc)
        crs = QgsCoordinateReferenceSystem.fromEpsgId(code)
        result["qgis"] = "ok" if crs.isValid() else "invalid"
        REPORT["epsg"][str(code)] = result
        for k, v in result.items():
            if v != "ok":
                fail(f"EPSG:{code} via {k}: {v}")
        log(f"EPSG:{code} {result}")
    try:
        xform = QgsCoordinateTransform(QgsCoordinateReferenceSystem("EPSG:4326"),
                                       QgsCoordinateReferenceSystem("EPSG:2193"),
                                       QgsProject.instance())
        pt = xform.transform(QgsPointXY(174.7762, -41.2865))
        REPORT["transform"] = [pt.x(), pt.y()]
        log(f"EPSG:4326 -> EPSG:2193 Wellington = {pt.x():.1f}, {pt.y():.1f}")
        if not (1.7e6 < pt.x() < 1.8e6 and 5.4e6 < pt.y() < 5.5e6):
            fail(f"EPSG:4326 -> 2193 transform out of range: {pt.x()}, {pt.y()}")
    except Exception as exc:  # noqa: BLE001
        fail(f"QgsCoordinateTransform failed: {exc}")


def check_gpkg(gdal, work: Path) -> None:
    path = work / "smoke_create.gpkg"
    try:
        ds = gdal.GetDriverByName("GPKG").Create(str(path), 0, 0, 0, gdal.GDT_Unknown)
        ds = None
        ok = path.is_file() and path.stat().st_size > 0
        REPORT["gpkg"] = "ok" if ok else "missing/empty"
        if not ok:
            fail(f"GeoPackage create produced no file: {path}")
    except Exception as exc:  # noqa: BLE001
        REPORT["gpkg"] = str(exc)
        fail(f"GeoPackage create failed: {exc}")


def write_test_raster(gdal, osr, path: Path) -> None:
    ds = gdal.GetDriverByName("GTiff").Create(str(path), 32, 32, 1, gdal.GDT_Byte)
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(2193)
    ds.SetProjection(srs.ExportToWkt())
    ds.SetGeoTransform([1600000.0, 10.0, 0.0, 5500000.0, 0.0, -10.0])
    ds.GetRasterBand(1).WriteRaster(0, 0, 32, 32, bytes([1] * 16 + [2] * 16) * 32)
    ds = None


def validate_polygons(ogr, path: Path, layer_name: str | None = None) -> tuple[int, list[str]]:
    ds = ogr.Open(str(path))
    if ds is None:
        raise RuntimeError(f"cannot open {path}")
    layer = (ds.GetLayerByName(layer_name) if layer_name else None) or ds.GetLayer(0)
    defn = layer.GetLayerDefn()
    fields = [defn.GetFieldDefn(i).GetName() for i in range(defn.GetFieldCount())]
    return layer.GetFeatureCount(), fields


def child_env(ctx: Context) -> dict:
    env = os.environ.copy()
    if ctx.bin_dir is not None:
        env["PATH"] = str(ctx.bin_dir) + os.pathsep + env.get("PATH", "")
    return env


def check_polygonize_tool(ctx: Context, ogr, tif: Path, work: Path) -> None:
    tool = next((ctx.root / c.lstrip("/") for c in ctx.info["polygonize_command"]
                 if (ctx.root / c.lstrip("/")).is_file()), None)
    if tool is None:
        REPORT["polygonize"] = "tool missing"
        fail(f"gdal_polygonize not found (candidates {ctx.info['polygonize_command']})")
        return
    out = work / "tool_output.gpkg"
    cmd = [str(tool), str(tif), "-b", "1", "-f", "GPKG", str(out), "OUTPUT", "DN"]
    log("Running: " + " ".join(cmd))
    proc = subprocess.run(cmd, capture_output=True, text=True, env=child_env(ctx),
                          cwd=str(work), check=False)
    log(f"exit={proc.returncode}\n--- stdout ---\n{proc.stdout}\n--- stderr ---\n{proc.stderr}")
    if proc.returncode != 0 or not out.is_file():
        REPORT["polygonize"] = f"exit {proc.returncode}"
        fail(f"{tool.name} failed (exit {proc.returncode})")
        return
    try:
        count, fields = validate_polygons(ogr, out, "OUTPUT")
        REPORT["polygonize"] = {"tool": str(tool), "features": count, "fields": fields}
        if count < 2 or "DN" not in fields:
            fail(f"{tool.name} output unexpected: features={count} fields={fields}")
    except Exception as exc:  # noqa: BLE001
        REPORT["polygonize"] = str(exc)
        fail(f"validating {tool.name} output failed: {exc}")


def find_plugins_dir() -> Path | None:
    from qgis.core import QgsApplication
    import qgis
    candidates = [Path(QgsApplication.pkgDataPath()) / "python" / "plugins",
                  Path(qgis.__file__).resolve().parent.parent / "plugins"]
    for c in candidates:
        if (c / "processing" / "__init__.py").is_file():
            return c
    return None


def check_processing(ctx: Context, ogr, tif: Path, work: Path) -> None:
    result: dict = {}
    REPORT["processing"] = result
    plugins = find_plugins_dir()
    if plugins is None:
        fail("Processing plugin directory not found in the installed product")
        return
    if str(plugins) not in sys.path:
        sys.path.insert(0, str(plugins))
    result["plugins_dir"] = str(plugins)
    try:
        from qgis.analysis import QgsNativeAlgorithms
        from qgis.core import QgsApplication, QgsProcessingFeedback
        import processing
        from processing.core.Processing import Processing
        Processing.initialize()
        registry = QgsApplication.processingRegistry()
        if registry.providerById("native") is None:
            registry.addProvider(QgsNativeAlgorithms())
        providers = sorted(p.id() for p in registry.providers())
        result["providers"] = providers
        log(f"Processing providers: {providers}")
        for pid in ctx.model["required_processing_providers"]:
            if pid not in providers:
                fail(f"Processing provider '{pid}' not registered")

        feedback = QgsProcessingFeedback()
        poly_out = work / "processing_polygonize.gpkg"
        res = processing.run("gdal:polygonize", {
            "INPUT": str(tif), "BAND": 1, "FIELD": "DN",
            "EIGHT_CONNECTEDNESS": False, "OUTPUT": str(poly_out),
        }, feedback=feedback)
        count, fields = validate_polygons(ogr, Path(res["OUTPUT"]))
        result["gdal:polygonize"] = {"features": count, "fields": fields}
        log(f"gdal:polygonize -> {count} features, fields {fields}")
        if count < 2 or "DN" not in fields:
            fail(f"gdal:polygonize output unexpected: features={count} fields={fields}")

        reproj_out = work / "processing_reprojected.gpkg"
        res = processing.run("native:reprojectlayer", {
            "INPUT": str(poly_out), "TARGET_CRS": "EPSG:4326", "OUTPUT": str(reproj_out),
        }, feedback=feedback)
        count, _fields = validate_polygons(ogr, Path(res["OUTPUT"]))
        result["native:reprojectlayer"] = {"features": count}
        log(f"native:reprojectlayer -> {count} features")
        if count < 2:
            fail(f"native:reprojectlayer output unexpected: features={count}")
    except Exception as exc:  # noqa: BLE001
        result["exception"] = str(exc)
        fail(f"Processing check failed: {exc}")
        traceback.print_exc()


def rerun_with_proj_debug(work: Path) -> None:
    log("=== PROJ_DEBUG re-run for diagnostics ===")
    env = os.environ.copy()
    env.update({"PROJ_DEBUG": "3", "CPL_DEBUG": "ON"})
    code = ("from osgeo import gdal, osr\n"
            "print('GDAL', gdal.VersionInfo('--version'))\n"
            "print('search', list(osr.GetPROJSearchPaths()))\n"
            "s = osr.SpatialReference()\n"
            "print('ImportFromEPSG(2193)', s.ImportFromEPSG(2193))\n")
    proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True,
                          env=env, cwd=str(work), check=False)
    log(f"debug exit={proc.returncode}\n{proc.stdout}\n{proc.stderr}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--install-root", required=True, type=Path)
    parser.add_argument("--report-dir", required=True, type=Path)
    parser.add_argument("--label", default="smoke")
    parser.add_argument("--platform", choices=("windows", "linux", "macos"), default=detect_platform())
    parser.add_argument("--deps", type=Path, default=HERE / "hake_runtime_deps.json")
    parser.add_argument("--forbid", action="append", default=[],
                        help="Additional forbidden path prefix (decoy dir, build tree, source tree)")
    parser.add_argument("--skip-processing", action="store_true")
    args = parser.parse_args()

    ctx = Context(args)
    report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=True)
    REPORT.update(label=args.label, platform=ctx.platform, install_root=str(ctx.root),
                  env={k: os.environ.get(k) for k in ENV_KEYS})

    log(f"=== Hake GeoDesk runtime smoke ({args.label}, {ctx.platform}) root={ctx.root} ===")
    for k, v in REPORT["env"].items():
        log(f"{k}={v}")

    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    for entry in reversed(ctx.info.get("python_path", [])):
        sys.path.insert(0, str(ctx.root / entry))

    app = None
    with tempfile.TemporaryDirectory(prefix="hake-runtime-smoke-") as tmp:
        work = Path(tmp)
        try:
            check_interpreter(ctx)
            check_sys_path(ctx)
            from osgeo import gdal, ogr, osr
            gdal.UseExceptions()
            ogr.UseExceptions()
            import_modules(ctx)
            collect_versions(gdal, osr)
            check_proj(ctx, osr)
            app = init_qgis(ctx)
            check_epsg(ctx, osr)
            check_gpkg(gdal, work)
            tif = work / "input_2193.tif"
            write_test_raster(gdal, osr, tif)
            check_polygonize_tool(ctx, ogr, tif, work)
            if not args.skip_processing:
                check_processing(ctx, ogr, tif, work)
            check_loaded_libraries(ctx)
        except Exception as exc:  # noqa: BLE001
            fail(f"unhandled exception: {exc}")
            traceback.print_exc()
        if REPORT["errors"]:
            try:
                rerun_with_proj_debug(work)
            except Exception:  # noqa: BLE001
                traceback.print_exc()

    REPORT["ok"] = not REPORT["errors"]
    out = report_dir / f"{args.label}.json"
    out.write_text(json.dumps(REPORT, indent=2, default=str), encoding="utf-8")
    log(f"Wrote {out}")
    if REPORT["errors"]:
        log("=== FAILURES ===")
        for e in REPORT["errors"]:
            log(f" - {e}")
        code = 1
    else:
        log("=== ALL CHECKS PASSED ===")
        code = 0
    if app is not None:
        # exitQgis() can crash in some headless teardown paths; the result is already written.
        sys.stdout.flush()
        os._exit(code)
    return code


if __name__ == "__main__":
    sys.exit(main())
