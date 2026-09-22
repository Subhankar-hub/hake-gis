# Hake GeoDesk Plugin Server

Remote plugin repository for **Hake GeoDesk – Desktop GIS** (HAKE GEOSPATIAL / Hake Technologies).

**Nginx is the only HTTP server.** There is no Apache and no Python HTTP server.

## Purpose

Mirror the upstream QGIS plugin index, rewrite download URLs to this host, and serve static XML/XSL/ZIP assets to the Hake GeoDesk Plugin Manager.

## Architecture

```
Upstream QGIS Plugin Repository
  https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0
        |
        | sync_plugins.py (+ sync_service.sh / cron every 7 days + flock)
        v
/opt/hake-geodesk/plugin-server/plugins/
        |
        | static files only
        v
NGINX (HTTP)
        |
        v
http://plugins.haketech.com/plugins.xml?hake-geodesk=2026
        |
        v
Hake GeoDesk – Desktop GIS
```

## Repository URLs

| Role | URL |
|------|-----|
| Production | `http://plugins.haketech.com/plugins.xml?hake-geodesk=2026` |
| Local test | `http://localhost/plugins.xml?hake-geodesk=2026` |
| Upstream sync | `https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0` |

HTTP only for now (no TLS / Certbot / HTTPS redirects). Structure the Nginx site so an HTTPS `server` block can be added later.

Do **not** use port 8005 or `server.py`.

## Requirements

- Linux server (Debian/Ubuntu recommended)
- Nginx
- Python 3
- cron
- DNS A/AAAA for `plugins.haketech.com` pointing at the server

## Directory layout

```
plugin-server/
├── README.md
├── sync_plugins.py
├── sync_service.sh          # start|stop|status|sync (7-day schedule)
├── setup_cron.sh            # alias for: sync_service.sh start
├── setup_nginx.sh
├── healthcheck.py
├── scripts/fix_mirror_permissions.sh
├── nginx/hake-geodesk-plugins.conf
├── plugins.xsl          # source stylesheet
├── logo.png             # source logo
├── plugins/             # generated/deployed mirror (xml/xsl/zips not in git)
│   ├── plugins.xml
│   ├── plugins.xsl
│   ├── logo.png
│   └── *.zip
└── logs/
    └── sync.log
```

Production deploy path:

```
/opt/hake-geodesk/plugin-server  ->  <this directory>
```

## Initial server setup

```bash
# 1. Clone / copy this tree onto the server
cd /path/to/hake-gis-windows/plugin-server   # or a dedicated clone

# 2. Install packages
sudo apt update
sudo apt install -y nginx python3 cron

# 3. First sync (production download URLs)
PLUGIN_SERVER_URL=http://plugins.haketech.com python3 sync_plugins.py

# Local machine mirror instead:
# PLUGIN_SERVER_URL=http://localhost python3 sync_plugins.py

# 4. Nginx site
chmod +x setup_nginx.sh
sudo ./setup_nginx.sh

# 5. Health check
PLUGIN_SERVER_URL=http://plugins.haketech.com python3 healthcheck.py
# or locally:
# PLUGIN_SERVER_URL=http://localhost python3 healthcheck.py

# 6. Background sync every 7 days
chmod +x sync_service.sh setup_cron.sh
sudo ./sync_service.sh start
# (setup_cron.sh is the same as: sync_service.sh start)
```

## Nginx setup

`setup_nginx.sh` will:

1. Symlink `/opt/hake-geodesk/plugin-server` → this directory
2. Install `nginx/hake-geodesk-plugins.conf` as a dedicated site
3. Set read permissions for `www-data`
4. Run `nginx -t` and reload/start Nginx
5. Print local and production URLs

Public endpoints only:

- `/plugins.xml`
- `/plugins.xsl`
- `/logo.png`
- `/plugins/*.zip`

Scripts, logs, and `.git` are **not** exposed.

## Configuration

### Sync (`sync_plugins.py`)

| Variable | Default | Description |
|----------|---------|-------------|
| `PLUGIN_SERVER_URL` | `http://plugins.haketech.com` | Base URL written into `download_url` |
| `UPSTREAM_URL` | `https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0` | Upstream metadata |

### Application (`PLUGIN_REPOSITORY_URL`)

Canonical default in Hake GeoDesk:

`http://plugins.haketech.com/plugins.xml`

Plus runtime query: `?hake-geodesk=2026` (from product version label).

Local override without source edits:

```bash
export PLUGIN_REPOSITORY_URL=http://localhost/plugins.xml
```

### Sync schedule (`sync_service.sh`)

```bash
./sync_service.sh start     # enable cron: every 7 days at 00:00
./sync_service.sh stop      # ONLY way to disable the schedule
./sync_service.sh status
./sync_service.sh sync      # one-shot now; Ctrl+C aborts this run only
```

Ctrl+C during `sync` does **not** disable the 7-day cron. Use `stop` for that.

### Env (`/etc/hake-geodesk/plugin-sync.env`)

```bash
PLUGIN_SERVER_URL="http://plugins.haketech.com"
# PLUGIN_SERVER_URL="http://localhost"
SYNC_CRON_SCHEDULE="0 0 */7 * *"
```

Quote values that contain `*`.

## Manual synchronization

```bash
./sync_service.sh sync
# or:
flock -n /var/run/hake-geodesk-plugin-sync.lock \
  env PLUGIN_SERVER_URL=http://plugins.haketech.com \
  python3 sync_plugins.py
```

### Safety

- ZIP: temp file → `chmod 0644` → atomic rename
- `plugins.xml`: write `.tmp` → validate → `os.replace`
- Failed sync retains last known-good `plugins.xml`
- `flock -n` prevents concurrent syncs
- Missing ZIP downloads are logged; index still uses local URLs; healthcheck warns

If downloads return **403 Forbidden**, file mode is often `600`. Fix:

```bash
sudo ./scripts/fix_mirror_permissions.sh
```

## Health check

```bash
PLUGIN_SERVER_URL=http://plugins.haketech.com python3 healthcheck.py
```

Exit `0` = healthy (missing ZIPs may warn). Exit `1` = hard failure.

## Testing

```bash
curl -sI "http://localhost/plugins.xml?hake-geodesk=2026"
curl -f "http://localhost/plugins.xml?hake-geodesk=2026" | head
curl -f "http://localhost/plugins.xsl" | head
curl -f "http://localhost/logo.png" -o /dev/null

# Substitute a real filename from plugins/
curl -sI "http://localhost/plugins/SomePlugin.1.0.zip"
```

Production (when DNS points here):

```bash
curl -sI "http://plugins.haketech.com/plugins.xml?hake-geodesk=2026"
curl -f "http://plugins.haketech.com/plugins.xml?hake-geodesk=2026" | head
```

Open the XML URL in a browser to view the modern XSL presentation.

## DNS

Create A (and optional AAAA) records:

```
plugins.haketech.com  ->  <server public IP>
```

No HTTPS in this deployment.

## Permissions

| Actor | Access |
|-------|--------|
| Sync user | read/write `plugins/` |
| Nginx (`www-data`) | read-only `plugins/` |

Do not make `plugins/` world-writable. Prefer group `www-data` + mode `0644` files / `0755` dirs.

## Security

- Static content only; no script execution
- GET/HEAD only
- Directory listing off
- No credentials in repo, cron env, or Nginx config
- Clients cannot upload plugins

## Plugin XML compatibility

Machine schema stays QGIS-compatible (`pyqgis_plugin`, `qgis_minimum_version`, …).  
Branding and presentation live in `plugins.xsl` only.

## GitHub / CI integration

The desktop app repository URL is defined in:

`python/pyplugin_installer/installer_data.py`

GitHub Actions workflows do **not** hard-code the plugin repository URL. Rebuilding Windows/Linux/macOS packages picks up the Python change through the normal `pyplugin_installer` install path.

## Future HTTPS

Add a second Nginx `server { listen 443 ssl; ... }` with certificates when ready. Do not enable redirects or HSTS until certificates are live.

## Troubleshooting

| Symptom | Check |
|---------|-------|
| 404 `/plugins.xml` | `sudo ./setup_nginx.sh`; `nginx -t`; site enabled |
| `nginx.service is not active` | `sudo systemctl enable --now nginx` |
| 403 on ZIP | `sudo ./scripts/fix_mirror_permissions.sh` |
| Stale index | Re-run sync; `Cache-Control: no-cache` on XML |
| Sync exits immediately | Another sync holds the flock lock; or run `./sync_service.sh status` |
| Disable scheduled sync | `./sync_service.sh stop` (Ctrl+C on sync does not disable cron) |
| App hits wrong host | Set/unset `PLUGIN_REPOSITORY_URL` |
| Wrong download host in XML | Re-sync with correct `PLUGIN_SERVER_URL` |

## Rollback

Failed sync leaves the previous valid `plugins.xml`. Incomplete `.partial` ZIPs are never published as finished packages.
