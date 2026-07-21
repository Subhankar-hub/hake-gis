# Hake Geospatial Plugin Server

A local mirror of the QGIS plugin repository for Hake Geospatial.

## Quick Start

### 1. Sync plugins from upstream
```bash
python3 sync_plugins.py
```
This downloads `plugins.xml` and all plugin `.zip` files into `plugins/`, rewriting download URLs to point to `http://localhost:8005`.

### 2. Start the server
```bash
python3 server.py
```
The plugin XML will be available at:
```
http://localhost:8005/plugins.xml
```

### 3. Set up automatic sync (cron job every 6 hours)
```bash
chmod +x setup_cron.sh
./setup_cron.sh
```

## Configuration

| Environment Variable  | Default                | Description                          |
|-----------------------|------------------------|--------------------------------------|
| `PLUGIN_SERVER_URL`   | `http://localhost:8005` | Base URL written into plugins.xml    |
| `PLUGIN_SERVER_PORT`  | `8005`                 | Port the HTTP server listens on      |

For production, set `PLUGIN_SERVER_URL` to your public domain:
```bash
PLUGIN_SERVER_URL=https://plugins.hakegeospatial.com python3 sync_plugins.py
```

## Directory Structure
```
plugin-server/
├── sync_plugins.py     # Downloads plugins and rewrites XML
├── server.py           # HTTP server
├── setup_cron.sh       # Cron job installer
├── README.md
└── plugins/            # Created by sync_plugins.py
    ├── plugins.xml     # Rewritten XML with local URLs
    ├── QuickWKT.3.4.zip
    ├── ...
    └── <plugin>.zip
```
