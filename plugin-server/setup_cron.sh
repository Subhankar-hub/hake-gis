#!/bin/bash
# setup_cron.sh — thin wrapper; use sync_service.sh for start/stop/status/sync.
# Default schedule: every 7 days (see sync_service.sh).

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/sync_service.sh" start
