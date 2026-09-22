#!/bin/bash
# fix_mirror_permissions.sh — make plugin mirror readable by Nginx (www-data).
# Fixes 403 Forbidden caused by mode-0600 ZIPs from tempfile.mkstemp.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PLUGINS_DIR="$SCRIPT_DIR/plugins"

if [ ! -d "$PLUGINS_DIR" ]; then
    echo "[perms] ERROR: missing $PLUGINS_DIR"
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "[perms] Re-running with sudo ..."
    exec sudo "$0" "$@"
fi

find "$PLUGINS_DIR" -type f ! -name '*.partial' ! -name '*.tmp' -exec chmod 0644 {} +
find "$PLUGINS_DIR" -type d -exec chmod 0755 {} +
if getent group www-data >/dev/null 2>&1; then
    chgrp -R www-data "$PLUGINS_DIR" || true
fi

echo "[perms] Mirror permissions updated under $PLUGINS_DIR"
stat -c '%a %U:%G %n' "$PLUGINS_DIR"/MemoryLayerSaver*.zip 2>/dev/null | head -3 || true
echo "[perms] Test: curl -sI http://localhost/plugins/MemoryLayerSaver.6.0.1.zip"
