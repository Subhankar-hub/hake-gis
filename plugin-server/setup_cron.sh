#!/bin/bash
# setup_cron.sh
# Sets up a cron job to automatically sync plugins every 6 hours.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SYNC_SCRIPT="$SCRIPT_DIR/sync_plugins.py"
LOG_FILE="$SCRIPT_DIR/sync.log"

# Cron expression: every 6 hours
CRON_SCHEDULE="0 */6 * * *"
CRON_CMD="cd $SCRIPT_DIR && /usr/bin/python3 $SYNC_SCRIPT >> $LOG_FILE 2>&1"

# Check if cron job already exists
EXISTING=$(crontab -l 2>/dev/null | grep -F "$SYNC_SCRIPT")
if [ -n "$EXISTING" ]; then
    echo "[cron] Sync cron job already exists:"
    echo "  $EXISTING"
    echo ""
    echo "To remove it, run:"
    echo "  crontab -l | grep -v '$SYNC_SCRIPT' | crontab -"
    exit 0
fi

# Add the cron job
(crontab -l 2>/dev/null; echo "$CRON_SCHEDULE $CRON_CMD") | crontab -

echo "[cron] Cron job installed successfully."
echo "  Schedule: $CRON_SCHEDULE (every 6 hours)"
echo "  Command:  $CRON_CMD"
echo "  Log file: $LOG_FILE"
echo ""
echo "To verify: crontab -l"
echo "To remove: crontab -l | grep -v '$SYNC_SCRIPT' | crontab -"
