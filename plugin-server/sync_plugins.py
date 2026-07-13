#!/usr/bin/env python3
"""
Hake Geospatial Plugin Mirror
Syncs plugins from the upstream QGIS plugin repository and rewrites
download URLs to point to the local server for downloaded plugins.
"""

import os
import sys
import shutil
import urllib.request
import xml.etree.ElementTree as ET

# NOTE: The upstream URL uses ?qgis= because that's what plugins.qgis.org expects.
# Our local server accepts ?hake-gis= instead (see installer_data.py urlParams()).
UPSTREAM_URL = "https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0"
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGINS_DIR = os.path.join(BASE_DIR, "plugins")
XML_PATH = os.path.join(PLUGINS_DIR, "plugins.xml")
XSL_SRC = os.path.join(BASE_DIR, "plugins.xsl")
XSL_DEST = os.path.join(PLUGINS_DIR, "plugins.xsl")
LOGO_SRC = os.path.join(BASE_DIR, "logo.png")
LOGO_DEST = os.path.join(PLUGINS_DIR, "logo.png")

# Local server base URL
LOCAL_BASE_URL = os.environ.get("PLUGIN_SERVER_URL", "http://localhost")


def fetch_upstream_xml():
    """Download the upstream plugins.xml."""
    print("[sync] Fetching upstream plugins.xml ...")
    req = urllib.request.Request(UPSTREAM_URL, headers={"User-Agent": "HakeGIS-PluginSync/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def download_zip(url, dest_path):
    """Download a plugin zip file if it doesn't already exist."""
    if os.path.exists(dest_path):
        print("  [skip] already exists: " + os.path.basename(dest_path))
        return True
    print("  [download] " + os.path.basename(dest_path))
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "HakeGIS-PluginSync/1.0"})
        with urllib.request.urlopen(req, timeout=120) as resp:
            data = resp.read()
        with open(dest_path, "wb") as f:
            f.write(data)
        return True
    except Exception as e:
        print("  [error] Failed to download " + url + ": " + str(e))
        return False


def write_xml(root):
    """Write the plugins.xml file with the XSL stylesheet instruction."""
    tree = ET.ElementTree(root)
    ET.indent(tree, space="    ")
    
    xml_str = ET.tostring(root, encoding="unicode", xml_declaration=True)
    # Insert the stylesheet instruction right after the XML declaration
    xml_lines = xml_str.split("\n")
    if xml_lines:
        xml_lines.insert(1, '<?xml-stylesheet type="text/xsl" href="plugins.xsl" ?>')
    
    with open(XML_PATH, "w", encoding="UTF-8") as f:
        f.write("\n".join(xml_lines))
        
    print("[sync] XML written to: " + XML_PATH)


def sync():
    """Main sync routine."""
    os.makedirs(PLUGINS_DIR, exist_ok=True)

    raw_xml = fetch_upstream_xml()
    root = ET.fromstring(raw_xml)

    total = 0
    downloaded = 0
    failed = 0

    try:
        for plugin_elem in root.findall("pyqgis_plugin"):
            total += 1
            name = plugin_elem.get("name", "unknown")

            dl_elem = plugin_elem.find("download_url")
            if dl_elem is None or not dl_elem.text:
                print("  [warn] No download_url for: " + name)
                continue
            upstream_download_url = dl_elem.text.strip()

            fn_elem = plugin_elem.find("file_name")
            if fn_elem is None or not fn_elem.text:
                filename = upstream_download_url.split("/")[-2] + ".zip"
            else:
                filename = fn_elem.text.strip()

            dest_path = os.path.join(PLUGINS_DIR, filename)
            if download_zip(upstream_download_url, dest_path):
                local_url = LOCAL_BASE_URL + "/plugins/" + filename
                dl_elem.text = local_url
                downloaded += 1
            else:
                # Keep upstream URL for failed downloads
                failed += 1

    except KeyboardInterrupt:
        print("\n[sync] Interrupted. Writing XML with what we have so far ...")

    # Rewrite URLs for ALL plugins to point to local server, so no traffic goes to qgis.org
    for plugin_elem in root.findall("pyqgis_plugin"):
        dl_elem = plugin_elem.find("download_url")
        fn_elem = plugin_elem.find("file_name")
        if dl_elem is None or fn_elem is None:
            continue
        filename = fn_elem.text.strip() if fn_elem.text else ""
        if not filename:
            continue
        dl_elem.text = LOCAL_BASE_URL + "/plugins/" + filename

    write_xml(root)
    
    # Copy plugins.xsl and logo.png to the plugins directory so they can be served alongside
    if os.path.exists(XSL_SRC):
        shutil.copy2(XSL_SRC, XSL_DEST)
    if os.path.exists(LOGO_SRC):
        shutil.copy2(LOGO_SRC, LOGO_DEST)

    print("")
    print("[sync] Done.")
    print("  Total plugins: " + str(total))
    print("  Downloaded/cached: " + str(downloaded))
    print("  Failed: " + str(failed))


if __name__ == "__main__":
    sync()
