"""Hake GeoDesk GUI smoke, executed inside the application.

Usage (offscreen, fresh profile):
  HAKE_GUI_SMOKE_REPORT=/tmp/gui.json QT_QPA_PLATFORM=offscreen \
    hake-geodesk --nologo --profiles-path /tmp/profile --code hake_gui_smoke.py

Verifies that the embedded interpreter started from the packaged runtime,
user site-packages are disabled on Windows/macOS, the main window exists,
the native and GDAL Processing providers are registered, the Python console
and plugin installer import, and EPSG codes resolve. Writes a JSON report to
$HAKE_GUI_SMOKE_REPORT and terminates the application with exit code 0/1.
"""

import json
import os
import site
import sys
import traceback

REQUIRED_PROVIDERS = ("native", "gdal")
REQUIRED_EPSG = (4326, 2193, 3978, 5936, 5482)


def _run():
    report = {"ok": False, "errors": [], "sys_path": list(sys.path)}
    errors = report["errors"]
    try:
        from qgis.core import Qgis, QgsApplication, QgsCoordinateReferenceSystem
        from qgis.utils import iface, plugins

        report["qgis_version"] = Qgis.version()
        report["prefix_path"] = QgsApplication.prefixPath()
        report["python_home"] = sys.prefix
        report["user_site_enabled"] = bool(site.ENABLE_USER_SITE)
        if sys.platform in ("win32", "darwin") and site.ENABLE_USER_SITE:
            errors.append("user site-packages enabled in the embedded interpreter")
        for key in ("PROJ_DATA", "PROJ_LIB", "GDAL_DATA", "PYTHONHOME", "PYTHONPATH"):
            report[key] = os.environ.get(key)

        if iface is None or iface.mainWindow() is None:
            errors.append("iface/main window not available")

        report["loaded_plugins"] = sorted(plugins.keys())
        providers = sorted(p.id() for p in QgsApplication.processingRegistry().providers())
        report["processing_providers"] = providers
        for pid in REQUIRED_PROVIDERS:
            if pid not in providers:
                errors.append(f"Processing provider '{pid}' not registered")

        for module in ("console", "console.console", "pyplugin_installer", "processing"):
            try:
                __import__(module)
            except Exception as exc:  # noqa: BLE001
                errors.append(f"import {module} failed: {exc}")

        report["epsg"] = {}
        for code in REQUIRED_EPSG:
            valid = QgsCoordinateReferenceSystem.fromEpsgId(code).isValid()
            report["epsg"][str(code)] = valid
            if not valid:
                errors.append(f"EPSG:{code} invalid in GUI process")
    except Exception as exc:  # noqa: BLE001
        errors.append(f"unhandled exception: {exc}")
        report["traceback"] = traceback.format_exc()

    report["ok"] = not errors
    out = os.environ.get("HAKE_GUI_SMOKE_REPORT")
    if out:
        with open(out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, indent=2, default=str)
    print(json.dumps(report, indent=2, default=str), flush=True)
    sys.stdout.flush()
    os._exit(0 if report["ok"] else 1)


_run()
