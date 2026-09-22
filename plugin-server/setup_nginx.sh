#!/bin/bash
# setup_nginx.sh
# Installs the Hake GeoDesk plugin repository as an Nginx site (HTTP only).
# Nginx is the ONLY HTTP server — no Apache, no Python HTTP server.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
OPT_LINK="/opt/hake-geodesk/plugin-server"
SITE_SRC="$SCRIPT_DIR/nginx/hake-geodesk-plugins.conf"
SITE_AVAILABLE="/etc/nginx/sites-available/hake-geodesk-plugins"
SITE_ENABLED="/etc/nginx/sites-enabled/hake-geodesk-plugins"
CONF_D="/etc/nginx/conf.d/hake-geodesk-plugins.conf"

if ! command -v nginx >/dev/null 2>&1; then
    echo "[nginx] ERROR: nginx is not installed."
    exit 1
fi

if [ ! -d "$REPO_ROOT/plugins" ]; then
    echo "[nginx] ERROR: plugins/ directory missing at $REPO_ROOT/plugins"
    echo "        Run: PLUGIN_SERVER_URL=http://localhost python3 $REPO_ROOT/sync_plugins.py"
    exit 1
fi

if [ ! -f "$SITE_SRC" ]; then
    echo "[nginx] ERROR: Missing site config: $SITE_SRC"
    exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "[nginx] Re-running with sudo ..."
    exec sudo "$0" "$@"
fi

echo "[nginx] Installing Hake GeoDesk plugin repository ..."

mkdir -p /opt/hake-geodesk
if [ -L "$OPT_LINK" ] || [ -e "$OPT_LINK" ]; then
    if [ -L "$OPT_LINK" ]; then
        current="$(readlink -f "$OPT_LINK" || true)"
        if [ "$current" != "$REPO_ROOT" ]; then
            echo "[nginx] Updating symlink $OPT_LINK -> $REPO_ROOT"
            rm -f "$OPT_LINK"
            ln -s "$REPO_ROOT" "$OPT_LINK"
        else
            echo "[nginx] Symlink already correct: $OPT_LINK"
        fi
    else
        echo "[nginx] ERROR: $OPT_LINK exists and is not a symlink. Remove it and re-run."
        exit 1
    fi
else
    ln -s "$REPO_ROOT" "$OPT_LINK"
    echo "[nginx] Created symlink $OPT_LINK -> $REPO_ROOT"
fi

# Ensure Nginx can read the mirror
if [ -d "$REPO_ROOT/plugins" ]; then
    find "$REPO_ROOT/plugins" -type f -exec chmod 0644 {} + 2>/dev/null || true
    find "$REPO_ROOT/plugins" -type d -exec chmod 0755 {} + 2>/dev/null || true
fi
if getent group www-data >/dev/null 2>&1; then
    chgrp -R www-data "$REPO_ROOT/plugins" 2>/dev/null || true
    d="$REPO_ROOT"
    while [ "$d" != "/" ]; do
        chmod o+x "$d" 2>/dev/null || true
        d="$(dirname "$d")"
    done
fi

# Remove legacy snippet include from default site if present
for site in /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default; do
    if [ -f "$site" ] && grep -qF 'snippets/hake-geodesk-plugins.conf' "$site"; then
        tmp="$(mktemp)"
        grep -vF 'snippets/hake-geodesk-plugins.conf' "$site" \
            | grep -vF '# Hake GeoDesk plugin repository' > "$tmp" || true
        cp "$tmp" "$site"
        rm -f "$tmp"
        echo "[nginx] Removed legacy snippet include from $site"
    fi
done
rm -f /etc/nginx/snippets/hake-geodesk-plugins.conf

# Install as dedicated site (Debian/Ubuntu layout) or conf.d fallback
if [ -d /etc/nginx/sites-available ] && [ -d /etc/nginx/sites-enabled ]; then
    install -m 0644 "$SITE_SRC" "$SITE_AVAILABLE"
    ln -sfn "$SITE_AVAILABLE" "$SITE_ENABLED"
    echo "[nginx] Installed site: $SITE_ENABLED"
elif [ -d /etc/nginx/conf.d ]; then
    install -m 0644 "$SITE_SRC" "$CONF_D"
    echo "[nginx] Installed: $CONF_D"
else
    echo "[nginx] ERROR: Neither sites-available nor conf.d found."
    exit 1
fi

echo "[nginx] Validating configuration ..."
nginx -t

echo "[nginx] Applying Nginx configuration ..."
if command -v systemctl >/dev/null 2>&1 && systemctl list-unit-files nginx.service >/dev/null 2>&1; then
    if systemctl is-active --quiet nginx 2>/dev/null; then
        systemctl reload nginx
        echo "[nginx] Reloaded nginx.service"
    else
        if systemctl enable --now nginx; then
            echo "[nginx] Started nginx.service (was inactive)"
        else
            echo "[nginx] systemctl start failed; trying nginx binary ..."
            nginx
        fi
    fi
    if ! pgrep -x nginx >/dev/null 2>&1; then
        echo "[nginx] ERROR: nginx is not running."
        systemctl status nginx --no-pager -l 2>/dev/null || true
        exit 1
    fi
else
    if pgrep -x nginx >/dev/null 2>&1; then
        nginx -s reload
        echo "[nginx] Reloaded via nginx -s reload"
    else
        nginx
        echo "[nginx] Started via nginx binary"
    fi
fi

echo ""
echo "[nginx] Plugin repository endpoints:"
echo "  Local:      http://localhost/plugins.xml?hake-geodesk=2026"
echo "  Production: http://plugins.haketech.com/plugins.xml?hake-geodesk=2026"
echo "  ZIPs:       http://<host>/plugins/<plugin>.zip"
echo ""
echo "Mirror path: $OPT_LINK -> $REPO_ROOT"
echo "HTTP only (no TLS in this deployment)."
