#!/usr/bin/env python3
"""Validate Hake icon coverage against the menu icon catalog.

Checks that every catalog row is mapped in the HAKE_ICONS registry in
src/app/qgshakeicons.cpp, that every mapped SVG exists and is compiled into
resources/icons/hake/hake_icons.qrc, and that every Hake SVG follows the icon
style rules. Exit status: 0 PASS, 1 FAIL, 2 catalog missing.
"""

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_CATALOG = ROOT / "docs/hake-archive/hake-menu-icon-prompt-catalog.md"
REGISTRY = ROOT / "src/app/qgshakeicons.cpp"
ICON_DIR = ROOT / "resources/icons/hake"
QRC = ICON_DIR / "hake_icons.qrc"
APP_CMAKE = ROOT / "src/app/CMakeLists.txt"

EXPECTED_CATALOG_ROWS = 228

# Catalog rows with no production counterpart; they are not counted as missing.
EXCLUDED = {
    "dsm:sqlite": "NOT PRESENT IN PRODUCTION (no SQLite source tab is registered; provider not re-enabled)",
}

# Catalog identifiers that differ from the production identifier.
CORRECTED = {
    "native:qgis:randompointsinlayerbounds": "qgis:randompointsinlayerbounds",
}

# Data Source Manager catalog ids -> provider key stored in the dialog items.
DSM_KEYS = {
    "dsm:browser": "browser",
    "dsm:vector": "ogr",
    "dsm:raster": "gdal",
    "dsm:mesh": "mdal",
    "dsm:point-cloud": "pointcloud",
    "dsm:delimited-text": "delimitedtext",
    "dsm:geopackage": "GeoPackage",
    "dsm:gps": "gpx",
    "dsm:spatialite": "spatialite",
    "dsm:postgresql": "postgres",
    "dsm:ms-sql-server": "mssql",
    "dsm:oracle": "oracle",
    "dsm:sap-hana": "hana",
    "dsm:virtual-layer": "virtual",
    "dsm:wms-wmts": "wms",
    "dsm:wfs-ogc-api-features": "WFS",
    "dsm:wcs": "wcs",
    "dsm:xyz": "xyz",
    "dsm:vector-tile": "vectortile",
    "dsm:scene": "tiledscene",
    "dsm:arcgis-rest-server": "arcgisfeatureserver",
    "dsm:sensorthings": "sensorthings",
    "dsm:stac": "stac",
    "dsm:metadata-search": "layermetadata",
}

SECTION_KINDS = {
    "Ribbon panels": "DOCK",
    "Data Source Manager": "DSM",
    "Processing, Database, and Web": "PLUGIN",
}

ALLOWED_COLOURS = {"#164A73", "#FFFFFF", "#D6E4F4"}
FORBIDDEN_TAGS = {
    "text",
    "tspan",
    "textPath",
    "image",
    "filter",
    "mask",
    "linearGradient",
    "radialGradient",
    "pattern",
    "style",
    "font",
    "font-face",
    "foreignObject",
    "script",
    "use",
    "symbol",
    "clipPath",
}
STROKE_WIDTH_RANGE = (0.9, 2.6)


def parse_catalog(path):
    rows = []
    part = 1
    section = None
    current = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# Part 2"):
            part = 2
        elif line.startswith("## "):
            section = line[3:].strip()
        elif line.startswith("### "):
            pass
        m = re.match(r"^- action: `([^`]+)`", line)
        if m:
            current = {"id": m.group(1), "part": part, "section": section}
            rows.append(current)
            continue
        m = re.match(r"^  - (menu|label|stock icon|subject): (.*)$", line)
        if m and current is not None:
            current[m.group(1)] = m.group(2).strip()
    return rows


def expected_mapping(row):
    cid = row["id"]
    kind = SECTION_KINDS.get(row["section"])
    if kind is None:
        kind = "ALG" if row["part"] == 2 else "A"
    key = CORRECTED.get(cid, cid)
    if kind == "DSM":
        key = DSM_KEYS.get(cid)
    return kind, key


def parse_registry(path):
    text = path.read_text(encoding="utf-8")
    body = re.search(r"HAKE_ICONS\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    if not body:
        return None
    return [
        (m.group(1), m.group(2), m.group(3))
        for m in re.finditer(
            r'\{\s*(A|ALG|PLUGIN|DOCK|DSM)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\}',
            body.group(1),
        )
    ]


def parse_property_pages(path):
    text = path.read_text(encoding="utf-8")
    body = re.search(r"HAKE_PROPERTY_PAGE_ICONS\[\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    if not body:
        return []
    return re.findall(
        r'\{\s*"(mOptsPage_[A-Za-z]+)"\s*,\s*"([^"]+)"\s*\}', body.group(1)
    )


def parse_qrc(path):
    root = ET.parse(path).getroot()
    entries = []
    for res in root.iter("qresource"):
        if res.get("prefix") != "/hake/icons":
            continue
        entries.extend(f.text.strip() for f in res.iter("file"))
    return entries


def source_references():
    refs = set()
    for src in (ROOT / "src/app").rglob("*.cpp"):
        if src == REGISTRY:
            continue
        refs.update(
            re.findall(
                r'"((?:[a-z]+)/hake-[a-z0-9-]+\.svg)"',
                src.read_text(encoding="utf-8", errors="replace"),
            )
        )
    return refs


def local(tag):
    return tag.rsplit("}", 1)[-1]


def lint_svg(path):
    errors, warnings, widths = [], [], []
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        return [f"XML parse error: {e}"], warnings, widths
    if local(root.tag) != "svg":
        errors.append("root element is not <svg>")
    if root.get("width") != "24" or root.get("height") != "24":
        errors.append(f"size {root.get('width')}x{root.get('height')}, expected 24x24")
    if (root.get("viewBox") or "").split() != ["0", "0", "24", "24"]:
        errors.append(f"viewBox '{root.get('viewBox')}', expected '0 0 24 24'")
    caps, joins = set(), set()
    for el in root.iter():
        tag = local(el.tag)
        if tag in FORBIDDEN_TAGS:
            errors.append(f"forbidden element <{tag}>")
        for attr, value in el.attrib.items():
            name = local(attr)
            if (
                name in ("href", "style", "class")
                or "data:" in value
                or "url(" in value
            ):
                errors.append(f"forbidden attribute {name}='{value[:40]}'")
            if name in ("fill", "stroke") and value != "none":
                if value.upper() in ALLOWED_COLOURS and value not in ALLOWED_COLOURS:
                    errors.append(
                        f"{name} '{value}' must be uppercase for state recolouring"
                    )
                elif value not in ALLOWED_COLOURS:
                    errors.append(f"{name} colour '{value}' outside palette")
            if name in ("opacity", "fill-opacity", "stroke-opacity"):
                errors.append(f"{name} not allowed")
            if name == "stroke-linecap":
                caps.add(value)
            if name == "stroke-linejoin":
                joins.add(value)
            if name == "stroke-width":
                try:
                    w = float(value)
                    widths.append(w)
                    if not STROKE_WIDTH_RANGE[0] <= w <= STROKE_WIDTH_RANGE[1]:
                        warnings.append(
                            f"stroke-width {w} outside {STROKE_WIDTH_RANGE}"
                        )
                except ValueError:
                    errors.append(f"stroke-width '{value}' not numeric")
            if name == "transform" and not re.fullmatch(
                r"\s*(translate|rotate|scale)\([^)]*\)\s*", value
            ):
                warnings.append(f"transform '{value}'")
    if caps - {"round"}:
        errors.append(f"stroke-linecap {sorted(caps)} (must be round)")
    if joins - {"round"}:
        errors.append(f"stroke-linejoin {sorted(joins)} (must be round)")
    if "round" not in caps or "round" not in joins:
        errors.append("round caps/joins not declared")
    if "1.6" not in {f"{w:g}" for w in widths}:
        warnings.append("primary stroke-width 1.6 not used")
    return errors, warnings, widths


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    ap.add_argument(
        "--verbose",
        action="store_true",
        help="list every mapping and every lint warning",
    )
    args = ap.parse_args()

    if not args.catalog.is_file():
        print(f"Catalog not found: {args.catalog}")
        print("RESULT: NOT RUN (catalog missing)")
        return 2

    failures = []

    def fail(msg):
        failures.append(msg)
        print(f"  FAIL: {msg}")

    print("=== Hake icon coverage ===")
    rows = parse_catalog(args.catalog)
    print(
        f"Catalog: {args.catalog.relative_to(ROOT) if args.catalog.is_relative_to(ROOT) else args.catalog}"
    )
    print(f"Catalog rows: {len(rows)} (expected {EXPECTED_CATALOG_ROWS})")
    if len(rows) != EXPECTED_CATALOG_ROWS:
        fail(f"catalog row count {len(rows)} != {EXPECTED_CATALOG_ROWS}")
    dup_ids = [i for i, n in Counter(r["id"] for r in rows).items() if n > 1]
    for i in dup_ids:
        fail(f"duplicate catalog id {i}")

    registry = parse_registry(REGISTRY)
    if registry is None:
        fail("HAKE_ICONS table not found in qgshakeicons.cpp")
        registry = []
    by_key = defaultdict(list)
    for kind, key, res in registry:
        by_key[(kind, key)].append(res)
    print(f"Registry entries: {len(registry)}")

    print("\n-- Duplicate registry entries")
    dups = {k: v for k, v in by_key.items() if len(v) > 1}
    for (kind, key), res in sorted(dups.items()):
        fail(f"{kind} {key} mapped {len(res)} times")
    if not dups:
        print("  none")

    print("\n-- Catalog coverage")
    covered, missing, excluded = [], [], []
    catalog_keys = set()
    for row in rows:
        cid = row["id"]
        if cid in EXCLUDED:
            excluded.append(row)
            continue
        kind, key = expected_mapping(row)
        if key is None:
            fail(f"{cid}: no production key known for this catalog id")
            missing.append(row)
            continue
        catalog_keys.add((kind, key))
        if (kind, key) in by_key:
            covered.append((row, kind, key, by_key[(kind, key)][0]))
        else:
            missing.append(row)
            fail(f"{cid}: no {kind} mapping for '{key}'")
    print(f"  production mappings required: {len(rows) - len(excluded)}")
    print(
        f"  covered: {len(covered)}  missing: {len(missing)}  excluded: {len(excluded)}"
    )
    for row in excluded:
        print(f"  EXCLUDED {row['id']}: {EXCLUDED[row['id']]}")
    for bad, good in CORRECTED.items():
        state = (
            "applied" if any(r["id"] == bad for r in rows) else "catalog row not found"
        )
        print(f"  CORRECTED {bad} -> {good} ({state})")

    print("\n-- Orphan registry entries (not in catalog)")
    reference = [(k, key, r) for k, key, r in registry if (k, key) not in catalog_keys]
    orphans = [e for e in reference if e[0] != "A"]
    for kind, key, _ in orphans:
        fail(f"{kind} {key} is registered but not in the catalog")
    print(
        f"  pre-existing Action mappings outside the catalog (reference set): {sum(1 for e in reference if e[0] == 'A')}"
    )
    if not orphans:
        print("  no non-Action orphans")

    print("\n-- Resources")
    qrc = parse_qrc(QRC)
    qrc_set = set(qrc)
    on_disk = {p.relative_to(ICON_DIR).as_posix() for p in ICON_DIR.rglob("*.svg")}
    for f in sorted(n for n, c in Counter(qrc).items() if c > 1):
        fail(f"qrc lists {f} more than once")
    for f in sorted(qrc_set - on_disk):
        fail(f"qrc entry {f} does not exist on disk")
    for f in sorted(on_disk - qrc_set):
        fail(f"{f} exists on disk but is not in hake_icons.qrc")
    for kind, key, res in registry:
        if res not in on_disk:
            fail(f"{kind} {key}: resource {res} missing on disk")
        elif res not in qrc_set:
            fail(f"{kind} {key}: resource {res} not in qrc")
    property_pages = parse_property_pages(REGISTRY)
    for page, n in Counter(p for p, _ in property_pages).items():
        if n > 1:
            fail(f"property page {page} mapped {n} times")
    for page, res in property_pages:
        if res not in on_disk:
            fail(f"property page {page}: resource {res} missing on disk")
        elif res not in qrc_set:
            fail(f"property page {page}: resource {res} not in qrc")
    extra_refs = source_references()
    for res in sorted(extra_refs - on_disk):
        fail(f"source references {res} which does not exist")
    used = {r for _, _, r in registry} | extra_refs | {r for _, r in property_pages}
    unused = sorted(qrc_set - used)
    for f in unused:
        fail(f"{f} is compiled but never referenced")
    if "hake_icons.qrc" not in APP_CMAKE.read_text(encoding="utf-8"):
        fail("hake_icons.qrc is not listed in src/app/CMakeLists.txt")
    print(
        f"  qrc entries: {len(qrc)}  SVGs on disk: {len(on_disk)}  referenced outside registry: {len(extra_refs)}"
        f"  layer properties pages: {len(property_pages)}"
    )

    print("\n-- Reuse (one SVG serving several mappings)")
    users = defaultdict(list)
    for kind, key, res in registry:
        users[res].append(f"{kind}:{key}")
    for res in extra_refs:
        users[res].append("source")
    catalog_resources = {c[3] for c in covered}
    reused = {r: u for r, u in users.items() if len(u) > 1 and r in catalog_resources}
    for res, u in sorted(reused.items()):
        print(f"  {res}: {', '.join(sorted(u))}")
    new_catalog_svgs = {c[3] for c in covered if len(users[c[3]]) == 1}
    print(
        f"  catalog mappings using a shared SVG: {sum(1 for c in covered if len(users[c[3]]) > 1)}"
    )
    print(f"  catalog mappings with a dedicated SVG: {len(new_catalog_svgs)}")

    print("\n-- SVG style lint")
    lint_fail = 0
    warn_count = 0
    all_widths = Counter()
    for rel in sorted(on_disk):
        errors, warnings, widths = lint_svg(ICON_DIR / rel)
        all_widths.update(f"{w:g}" for w in widths)
        for e in errors:
            fail(f"{rel}: {e}")
            lint_fail += 1
        warn_count += len(warnings)
        if args.verbose:
            for w in warnings:
                print(f"  WARN: {rel}: {w}")
    print(f"  files: {len(on_disk)}  errors: {lint_fail}  warnings: {warn_count}")
    print(
        f"  stroke widths used: {', '.join(f'{w} x{n}' for w, n in sorted(all_widths.items(), key=lambda x: (
                    float(x[0])
                )))}"
    )

    if args.verbose:
        print("\n-- Mappings")
        for row, kind, key, res in covered:
            print(f"  {row['section']:<32} {kind:<6} {key:<48} {res}")

    print()
    if failures:
        print(f"RESULT: FAIL ({len(failures)} problem(s))")
        return 1
    print(
        f"RESULT: PASS ({len(covered)} mapped, {len(excluded)} excluded, {len(CORRECTED)} corrected)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
