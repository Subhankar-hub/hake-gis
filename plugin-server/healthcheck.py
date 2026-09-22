#!/usr/bin/env python3
"""
Health check for the Hake GeoDesk plugin mirror.

Verifies:
  - plugins/plugins.xml exists and is well-formed
  - root element is <plugins>
  - stylesheet reference / plugins.xsl / logo.png exist
  - download URLs point at PLUGIN_SERVER_URL host
  - referenced ZIP files exist where practical (missing = warning)

Exit 0 on healthy (missing ZIPs allowed as warnings).
Exit 1 on hard failure.
"""

from __future__ import annotations

import os
import sys
import urllib.parse
import xml.etree.ElementTree as ET

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGINS_DIR = os.path.join(BASE_DIR, "plugins")
XML_PATH = os.path.join(PLUGINS_DIR, "plugins.xml")
XSL_PATH = os.path.join(PLUGINS_DIR, "plugins.xsl")
LOGO_PATH = os.path.join(PLUGINS_DIR, "logo.png")
LOCAL_BASE_URL = os.environ.get(
    "PLUGIN_SERVER_URL", "http://plugins.haketech.com"
).rstrip("/")


def main() -> int:
    errors = 0
    warnings = 0

    if not os.path.isfile(XML_PATH):
        print(f"[health] FAIL: missing {XML_PATH}")
        return 1

    try:
        with open(XML_PATH, encoding="utf-8") as f:
            raw = f.read()
        root = ET.fromstring(raw)
    except ET.ParseError as e:
        print(f"[health] FAIL: invalid XML: {e}")
        return 1

    tag = root.tag.split("}")[-1] if "}" in root.tag else root.tag
    if tag != "plugins":
        print(f"[health] FAIL: unexpected root element: {root.tag}")
        return 1

    if "xml-stylesheet" not in raw and "plugins.xsl" not in raw:
        print("[health] WARN: stylesheet reference not found in plugins.xml")
        warnings += 1

    if not os.path.isfile(XSL_PATH):
        print(f"[health] FAIL: missing {XSL_PATH}")
        errors += 1
    if not os.path.isfile(LOGO_PATH):
        print(f"[health] WARN: missing {LOGO_PATH}")
        warnings += 1

    expected_host = urllib.parse.urlparse(LOCAL_BASE_URL).netloc or "localhost"
    plugins = root.findall("pyqgis_plugin")
    print(f"[health] plugins.xml OK ({len(plugins)} plugins)")
    print(f"[health] expecting download host: {expected_host}")

    missing_zips = 0
    foreign_urls = 0
    checked = 0

    for plugin in plugins:
        name = plugin.get("name", "unknown")
        dl = plugin.find("download_url")
        fn = plugin.find("file_name")
        if dl is None or not dl.text:
            print(f"[health] WARN: {name}: no download_url")
            warnings += 1
            continue

        url = dl.text.strip()
        parsed = urllib.parse.urlparse(url)
        checked += 1

        if parsed.netloc and parsed.netloc != expected_host:
            print(f"[health] FAIL: {name}: unexpected URL host: {url}")
            foreign_urls += 1
            errors += 1
            continue

        filename = ""
        if fn is not None and fn.text:
            filename = fn.text.strip()
        if not filename:
            filename = os.path.basename(parsed.path)

        if filename:
            zip_path = os.path.join(PLUGINS_DIR, filename)
            if not os.path.isfile(zip_path):
                missing_zips += 1
                warnings += 1

    if foreign_urls:
        print(f"[health] FAIL: {foreign_urls} unexpected download URL(s)")
    if missing_zips:
        print(
            f"[health] WARN: {missing_zips} referenced ZIP(s) missing "
            "(partial sync is allowed)"
        )
    else:
        print(f"[health] All checked ZIP references present ({checked} URLs)")

    print(f"[health] errors={errors} warnings={warnings}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
