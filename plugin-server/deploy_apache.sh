#!/bin/bash
# deploy_apache.sh
# Sets up Apache2 to serve the local Hake Geospatial plugins repository.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLUGINS_DIR="$SCRIPT_DIR/plugins"

echo "[Apache Deploy] Setting up Hake Geospatial plugin server..."

# Create a symlink in /var/www/html
if [ -L "/var/www/html/plugins" ] || [ -d "/var/www/html/plugins" ]; then
    echo "[Apache Deploy] /var/www/html/plugins already exists. Updating symlink..."
    sudo rm -rf "/var/www/html/plugins"
fi

sudo ln -s "$PLUGINS_DIR" /var/www/html/plugins
echo "[Apache Deploy] Symlink created at /var/www/html/plugins"

# Enable CORS (optional but good for web requests)
sudo a2enmod headers >/dev/null 2>&1
if [ ! -f /etc/apache2/conf-available/cors.conf ]; then
    echo "Header set Access-Control-Allow-Origin '*'" | sudo tee /etc/apache2/conf-available/cors.conf >/dev/null
    sudo a2enconf cors >/dev/null 2>&1
fi

# Reload Apache
sudo systemctl reload apache2
echo "[Apache Deploy] Apache2 reloaded."
echo ""
echo "Plugins repository is now available at:"
echo "http://localhost/plugins/plugins.xml"
