#!/usr/bin/env python3
"""
Hake Geospatial Plugin Server
Serves the local plugin mirror over HTTP.
Handles /plugins/plugins.xml?hake-gis=X.Y requests.
"""

import http.server
import os
import sys

PORT = int(os.environ.get("PLUGIN_SERVER_PORT", "8005"))
PLUGINS_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "plugins")


class PluginHandler(http.server.SimpleHTTPRequestHandler):
    """Serves files from the plugins directory with QGIS-compatible URL routing."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=PLUGINS_DIR, **kwargs)

    def do_GET(self):
        # Strip query params for file lookup (e.g. ?hake-gis=4.0)
        path = self.path
        if "?" in path:
            path = path.split("?")[0]

        # Route /plugins/plugins.xml -> /plugins.xml
        if path == "/plugins/plugins.xml":
            self.path = "/plugins.xml"
        # Route /plugins/<file>.zip -> /<file>.zip
        elif path.startswith("/plugins/"):
            self.path = path[len("/plugins"):]
        else:
            self.path = path

        return super().do_GET()

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()


def main():
    if not os.path.isdir(PLUGINS_DIR):
        print("[error] plugins/ directory not found. Run sync_plugins.py first.")
        sys.exit(1)

    xml_path = os.path.join(PLUGINS_DIR, "plugins.xml")
    if not os.path.isfile(xml_path):
        print("[error] plugins/plugins.xml not found. Run sync_plugins.py first.")
        sys.exit(1)

    server = http.server.HTTPServer(("0.0.0.0", PORT), PluginHandler)
    print("[server] Hake Geospatial Plugin Server")
    print("[server] Serving plugins from: " + PLUGINS_DIR)
    print("[server] Listening on http://0.0.0.0:" + str(PORT))
    print("")
    print("[server] Plugin XML endpoint:")
    print("  http://localhost:" + str(PORT) + "/plugins/plugins.xml?hake-gis=4.0")
    print("")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[server] Shutting down.")
        server.shutdown()


if __name__ == "__main__":
    main()
