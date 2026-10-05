#!/usr/bin/env bash
# Hake GeoDesk installed-runtime validation for Linux (.deb) and macOS (.app).
#
# Usage:
#   unix_runtime_smoke.sh --platform linux --root / --report-dir DIR
#   unix_runtime_smoke.sh --platform macos --root "/path/Hake GeoDesk.app" --report-dir DIR
#
# Every check runs under `env -i` with PATH reduced to the system directories, a
# throw-away HOME and QT_QPA_PLATFORM=offscreen, so nothing from the CI runner's
# toolchain (Homebrew, hostedtoolcache, vcpkg, the build tree) can satisfy a
# dependency. On macOS the process-tool and GUI checks additionally run with a
# decoy directory first on PATH and decoy PROJ_LIB/PROJ_DATA/GDAL_DATA/
# PYTHONHOME/PYTHONPATH values: the bundle must override them. On Linux PROJ,
# GDAL and Python intentionally come from the distribution packages, so no
# decoys are planted there.
#
# Steps (all run; the script fails at the end if any failed):
#   clean-env       hake_runtime_smoke.py with the platform's interpreter
#   process         hake-geodesk-process --version and run gdal:polygonize
#   gui             hake-geodesk --code hake_gui_smoke.py (offscreen)
#   manifest        hake_dependency_manifest.py (ELF/Mach-O graph, leak checks)

set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PLATFORM=""
ROOT=""
REPORT_DIR=""
GUI_TIMEOUT=600

while [ $# -gt 0 ]; do
  case "$1" in
    --platform) PLATFORM="$2"; shift 2 ;;
    --root) ROOT="$2"; shift 2 ;;
    --report-dir) REPORT_DIR="$2"; shift 2 ;;
    --gui-timeout) GUI_TIMEOUT="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
if [ -z "$PLATFORM" ] || [ -z "$ROOT" ] || [ -z "$REPORT_DIR" ]; then
  echo "usage: $0 --platform linux|macos --root ROOT --report-dir DIR" >&2
  exit 2
fi

mkdir -p "$REPORT_DIR"
REPORT_DIR="$(cd "$REPORT_DIR" && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/hake-smoke.XXXXXX")"
mkdir -p "$WORK/home" "$WORK/xdg" "$WORK/profiles" "$WORK/scripts"
chmod 700 "$WORK/xdg"
# Copy the scripts out of the checkout so the source tree is not on any path the
# runtime can see.
cp "$HERE"/hake_runtime_smoke.py "$HERE"/hake_gui_smoke.py "$HERE"/hake_dependency_manifest.py \
   "$HERE"/hake_runtime_deps.json "$WORK/scripts/"
SCRIPTS="$WORK/scripts"
DECOY="$WORK/decoy"

case "$PLATFORM" in
  linux)
    SAFE_PATH="/usr/bin:/bin"
    PY="/usr/bin/python3"
    PROC="/usr/bin/hake-geodesk-process"
    GUI="/usr/bin/hake-geodesk"
    MANIFEST_PY="/usr/bin/python3"
    ;;
  macos)
    SAFE_PATH="/usr/bin:/bin:/usr/sbin:/sbin"
    PY="$ROOT/Contents/MacOS/python"
    PROC="$ROOT/Contents/MacOS/hake-geodesk-process"
    GUI="$ROOT/Contents/MacOS/$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$ROOT/Contents/Info.plist")"
    MANIFEST_PY="/usr/bin/python3"
    ;;
  *) echo "unsupported platform: $PLATFORM" >&2; exit 2 ;;
esac

CLEAN_ENV=(env -i "PATH=$SAFE_PATH" "HOME=$WORK/home" "LANG=C.UTF-8" "LC_ALL=C.UTF-8"
           "TMPDIR=$WORK" "XDG_RUNTIME_DIR=$WORK/xdg" "QT_QPA_PLATFORM=offscreen")
FORBID=(--forbid "$WORK/home")

if [ "$PLATFORM" = "macos" ]; then
  mkdir -p "$DECOY/bin" "$DECOY/proj" "$DECOY/gdal" "$DECOY/pyhome" "$DECOY/python/osgeo" "$DECOY/python/qgis"
  for tool in gdal_polygonize gdal_polygonize.py gdalinfo ogr2ogr gdal_translate gdalwarp python python3; do
    printf '#!/bin/sh\necho "DECOY %s invoked" >&2\nexit 97\n' "$tool" > "$DECOY/bin/$tool"
    chmod +x "$DECOY/bin/$tool"
  done
  echo "not a sqlite database" > "$DECOY/proj/proj.db"
  echo 'raise ImportError("DECOY osgeo imported from PYTHONPATH")' > "$DECOY/python/osgeo/__init__.py"
  echo 'raise ImportError("DECOY qgis imported from PYTHONPATH")' > "$DECOY/python/qgis/__init__.py"
  HOSTILE_ENV=(env -i "PATH=$DECOY/bin:$SAFE_PATH" "HOME=$WORK/home" "LANG=C.UTF-8" "LC_ALL=C.UTF-8"
               "TMPDIR=$WORK" "XDG_RUNTIME_DIR=$WORK/xdg" "QT_QPA_PLATFORM=offscreen"
               "PROJ_LIB=$DECOY/proj" "PROJ_DATA=$DECOY/proj" "GDAL_DATA=$DECOY/gdal"
               "PYTHONHOME=$DECOY/pyhome" "PYTHONPATH=$DECOY/python")
  FORBID+=(--forbid "$DECOY")
else
  HOSTILE_ENV=("${CLEAN_ENV[@]}")
fi

FAILED=()
section() { printf '\n========== %s ==========\n' "$1"; }
record() { if [ "$2" -ne 0 ]; then echo "FAILED: $1 (exit $2)"; FAILED+=("$1"); else echo "PASSED: $1"; fi; }

# Runs "$@" with a wall-clock limit (macOS has no coreutils `timeout`).
run_with_timeout() {
  local limit="$1"; shift
  "$@" &
  local pid=$!
  ( sleep "$limit"; echo "TIMEOUT after ${limit}s, killing $pid" >&2; kill -9 "$pid" 2>/dev/null ) &
  local watcher=$!
  wait "$pid"
  local rc=$?
  kill "$watcher" 2>/dev/null
  wait "$watcher" 2>/dev/null
  return "$rc"
}

section "Python runtime smoke (clean environment)"
"${CLEAN_ENV[@]}" "$PY" -u "$SCRIPTS/hake_runtime_smoke.py" \
  --install-root "$ROOT" --report-dir "$REPORT_DIR" --label clean-env \
  --platform "$PLATFORM" --deps "$SCRIPTS/hake_runtime_deps.json" "${FORBID[@]}"
record "clean-env python smoke" $?

section "hake-geodesk-process gdal:polygonize"
TIF="$WORK/process_input.tif"
OUT="$WORK/process_output.gpkg"
PROC_LOG="$REPORT_DIR/process_polygonize.log"
"${CLEAN_ENV[@]}" "$PY" -c '
import sys
from osgeo import gdal, osr
gdal.UseExceptions()
ds = gdal.GetDriverByName("GTiff").Create(sys.argv[1], 32, 32, 1, gdal.GDT_Byte)
srs = osr.SpatialReference(); srs.ImportFromEPSG(2193)
ds.SetProjection(srs.ExportToWkt())
ds.SetGeoTransform([1600000.0, 10.0, 0.0, 5500000.0, 0.0, -10.0])
ds.GetRasterBand(1).WriteRaster(0, 0, 32, 32, bytes([1] * 16 + [2] * 16) * 32)
ds = None
' "$TIF"
rc=$?
if [ $rc -eq 0 ]; then
  "${HOSTILE_ENV[@]}" "$PROC" --version > "$REPORT_DIR/process_version.log" 2>&1
  rc=$?
  cat "$REPORT_DIR/process_version.log"
fi
if [ $rc -eq 0 ]; then
  (cd "$WORK" && run_with_timeout "$GUI_TIMEOUT" "${HOSTILE_ENV[@]}" "$PROC" run gdal:polygonize -- \
     "INPUT=$TIF" BAND=1 FIELD=DN EIGHT_CONNECTEDNESS=false "OUTPUT=$OUT") > "$PROC_LOG" 2>&1
  rc=$?
  cat "$PROC_LOG"
fi
if [ $rc -eq 0 ] && grep -q DECOY "$PROC_LOG"; then
  echo "hake-geodesk-process executed a decoy from PATH or PYTHONPATH"
  rc=1
fi
if [ $rc -eq 0 ]; then
  "${CLEAN_ENV[@]}" "$PY" -c '
import sys
from osgeo import ogr
ogr.UseExceptions()
layer = ogr.Open(sys.argv[1]).GetLayer(0)
count = layer.GetFeatureCount()
print("layer", layer.GetName(), "features", count)
assert count >= 2, count
' "$OUT"
  rc=$?
fi
record "process polygonize" $rc

section "GUI smoke (hake-geodesk --code, offscreen)"
GUI_REPORT="$REPORT_DIR/gui-smoke.json"
rm -f "$GUI_REPORT"
run_with_timeout "$GUI_TIMEOUT" "${HOSTILE_ENV[@]}" "HAKE_GUI_SMOKE_REPORT=$GUI_REPORT" \
  "$GUI" --nologo --profiles-path "$WORK/profiles" --code "$SCRIPTS/hake_gui_smoke.py" \
  > "$REPORT_DIR/gui-smoke.log" 2>&1
rc=$?
cat "$REPORT_DIR/gui-smoke.log"
if [ ! -f "$GUI_REPORT" ]; then
  echo "GUI smoke wrote no report"
  [ $rc -eq 0 ] && rc=1
elif ! "$MANIFEST_PY" -c 'import json,sys; sys.exit(0 if json.load(open(sys.argv[1]))["ok"] else 1)' "$GUI_REPORT"; then
  [ $rc -eq 0 ] && rc=1
fi
record "gui smoke" $rc

section "Runtime dependency manifest"
MANIFEST_ARGS=(--root "$ROOT" --platform "$PLATFORM" --deps "$SCRIPTS/hake_runtime_deps.json"
               --out "$REPORT_DIR/runtime-deps-$PLATFORM.json" "${FORBID[@]}")
[ -f "$REPORT_DIR/clean-env.json" ] && MANIFEST_ARGS+=(--smoke-report "$REPORT_DIR/clean-env.json")
"$MANIFEST_PY" "$SCRIPTS/hake_dependency_manifest.py" "${MANIFEST_ARGS[@]}"
record "dependency manifest" $?

rm -rf "$WORK"
section "Summary"
if [ ${#FAILED[@]} -ne 0 ]; then
  printf 'FAILED: %s\n' "${FAILED[@]}"
  exit 1
fi
echo "All runtime checks passed"
