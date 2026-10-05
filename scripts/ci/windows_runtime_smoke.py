#!/usr/bin/env python3
"""Compatibility shim: Windows packaged-runtime smoke test.

The checks live in hake_runtime_smoke.py (shared by Windows, Linux and macOS).
Run with the bundled install-tree python.exe:
  python.exe windows_runtime_smoke.py --install-root C:\\Hake GeoDesk --report-dir out
"""

import runpy
import sys
from pathlib import Path

if __name__ == "__main__":
    if "--platform" not in sys.argv:
        sys.argv += ["--platform", "windows"]
    runpy.run_path(str(Path(__file__).resolve().with_name("hake_runtime_smoke.py")), run_name="__main__")
