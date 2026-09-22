#!/bin/bash
# sync_service.sh — control Hake GeoDesk plugin mirror sync
#
#   ./sync_service.sh start    # enable background schedule (every 7 days)
#   ./sync_service.sh stop     # ONLY way to disable the scheduled service
#   ./sync_service.sh status
#   ./sync_service.sh sync     # one-shot foreground run (Ctrl+C exits this run only)
#
# Ctrl+C during "sync" does NOT stop the cron schedule.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SYNC_SCRIPT="$SCRIPT_DIR/sync_plugins.py"
LOG_DIR="$SCRIPT_DIR/logs"
LOG_FILE="$LOG_DIR/sync.log"
LOCK_FILE="/var/run/hake-geodesk-plugin-sync.lock"
SYNC_PID_FILE="$LOG_DIR/sync-oneshot.pid"
ENV_DIR="/etc/hake-geodesk"
ENV_FILE="$ENV_DIR/plugin-sync.env"
CRON_D="/etc/cron.d/hake-geodesk-plugin-sync"
DEFAULT_SCHEDULE="0 0 */7 * *"

usage() {
    cat <<EOF
Usage: $0 {start|stop|status|sync}

  start   Install/enable cron schedule (every 7 days) and ensure logging
  stop    Disable scheduled sync (removes cron.d); stop any running one-shot
  status  Show schedule, lock, and recent log lines
  sync    Run one sync now in the foreground (Ctrl+C aborts only this run)

Logs: $LOG_FILE
EOF
}

need_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "[sync-service] Re-running with sudo ..."
        exec sudo "$0" "$@"
    fi
}

write_env_file() {
    mkdir -p "$ENV_DIR"
    cat > "$ENV_FILE" <<EOF
# Hake GeoDesk plugin sync environment
# Sourced by cron and sync_service.sh. Keep values quoted where they contain * or ?.
PLUGIN_SERVER_URL="http://plugins.haketech.com"
# Local machine testing:
# PLUGIN_SERVER_URL="http://localhost"
# UPSTREAM_URL="https://plugins.qgis.org/plugins/plugins.xml?qgis=4.0"
SYNC_CRON_SCHEDULE="$DEFAULT_SCHEDULE"
EOF
    chmod 0644 "$ENV_FILE"
}

load_env() {
    SCHEDULE="$DEFAULT_SCHEDULE"
    if [ -f "$ENV_FILE" ]; then
        set -f
        set -a
        # shellcheck disable=SC1090
        # shellcheck disable=SC1091
        source "$ENV_FILE"
        set +a
        set +f
        SCHEDULE="${SYNC_CRON_SCHEDULE:-$DEFAULT_SCHEDULE}"
    fi
}

resolve_sync_user() {
    SYNC_USER="root"
    if [ -n "${SUDO_USER:-}" ] && id "$SUDO_USER" >/dev/null 2>&1; then
        SYNC_USER="$SUDO_USER"
    elif [ -d "$SCRIPT_DIR" ]; then
        owner="$(stat -c '%U' "$SCRIPT_DIR" 2>/dev/null || true)"
        if [ -n "$owner" ] && [ "$owner" != "root" ] && id "$owner" >/dev/null 2>&1; then
            SYNC_USER="$owner"
        fi
    fi
}

cmd_start() {
    need_root start
    resolve_sync_user
    mkdir -p "$LOG_DIR"
    touch "$LOG_FILE"
    chown "$SYNC_USER":"$SYNC_USER" "$LOG_FILE" "$LOG_DIR" 2>/dev/null || true

    if [ ! -f "$ENV_FILE" ] || grep -qE '^SYNC_CRON_SCHEDULE=[^"].*\*' "$ENV_FILE" 2>/dev/null; then
        write_env_file
        echo "[sync-service] Ensured $ENV_FILE"
    else
        # Always refresh schedule to the current default (7 days) on start
        write_env_file
        echo "[sync-service] Refreshed $ENV_FILE (schedule every 7 days)"
    fi
    load_env

    CRON_CMD="/usr/bin/flock -n $LOCK_FILE /usr/bin/env -i PATH=/usr/bin:/bin HOME=/tmp /bin/bash -c 'set -f; set -a; source $ENV_FILE; set +a; set +f; cd $SCRIPT_DIR && /usr/bin/python3 $SYNC_SCRIPT >> $LOG_FILE 2>&1'"

    cat > "$CRON_D" <<EOF
# Hake GeoDesk plugin repository sync
# Managed by $SCRIPT_DIR/sync_service.sh — use: ./sync_service.sh stop
SHELL=/bin/bash
PATH=/usr/bin:/bin
$SCHEDULE $SYNC_USER $CRON_CMD
EOF
    chmod 0644 "$CRON_D"

    {
        echo "===== $(date -Is) sync-service start ====="
        echo "schedule=$SCHEDULE user=$SYNC_USER cron=$CRON_D"
    } >> "$LOG_FILE"

    echo "[sync-service] Scheduled sync enabled."
    echo "  Schedule: $SCHEDULE (every 7 days at 00:00)"
    echo "  User:     $SYNC_USER"
    echo "  Cron:     $CRON_D"
    echo "  Log:      $LOG_FILE"
    echo "  Lock:     $LOCK_FILE"
    echo ""
    echo "Ctrl+C on './sync_service.sh sync' will NOT disable this schedule."
    echo "To disable: ./sync_service.sh stop"
}

cmd_stop() {
    need_root stop
    mkdir -p "$LOG_DIR"
    touch "$LOG_FILE" 2>/dev/null || true
    {
        echo "===== $(date -Is) sync-service stop ====="
    } >> "$LOG_FILE" 2>/dev/null || true

    if [ -f "$CRON_D" ]; then
        rm -f "$CRON_D"
        echo "[sync-service] Removed $CRON_D"
    else
        echo "[sync-service] No cron schedule installed ($CRON_D)."
    fi

    if [ -f "$SYNC_PID_FILE" ]; then
        pid="$(cat "$SYNC_PID_FILE" 2>/dev/null || true)"
        if [ -n "${pid:-}" ] && kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
            echo "[sync-service] Stopped running sync pid=$pid"
        fi
        rm -f "$SYNC_PID_FILE"
    fi

    echo "[sync-service] Scheduled sync disabled. Nginx unchanged."
}

cmd_status() {
    load_env
    echo "[sync-service] status"
    if [ -f "$CRON_D" ]; then
        echo "  schedule: ENABLED"
        echo "  cron.d:   $CRON_D"
        grep -v '^#' "$CRON_D" | grep -v '^$' | head -3 | sed 's/^/    /' || true
    else
        echo "  schedule: DISABLED (run: ./sync_service.sh start)"
    fi
    echo "  expected: $SCHEDULE"
    echo "  log:      $LOG_FILE"
    if [ -f "$LOCK_FILE" ] && ! flock -n "$LOCK_FILE" true 2>/dev/null; then
        echo "  lock:     HELD (sync in progress)"
    else
        echo "  lock:     free"
    fi
    if [ -f "$LOG_FILE" ]; then
        echo "  --- last log lines ---"
        tail -n 15 "$LOG_FILE" | sed 's/^/  /'
    else
        echo "  log:      (none yet)"
    fi
}

cmd_sync() {
    load_env
    mkdir -p "$LOG_DIR"
    touch "$LOG_FILE"

    if [ ! -f "$SYNC_SCRIPT" ]; then
        echo "[sync-service] ERROR: missing $SYNC_SCRIPT"
        exit 1
    fi

    # Ctrl+C / SIGTERM: abort this run only; do not touch cron.d
    on_interrupt() {
        echo ""
        echo "[sync-service] Interrupted (Ctrl+C). Leaving scheduled service unchanged."
        echo "[sync-service] Live plugins.xml retained if already valid."
        echo "[sync-service] Use './sync_service.sh stop' to disable the 7-day schedule."
        rm -f "$SYNC_PID_FILE"
        exit 130
    }
    trap on_interrupt INT TERM

    echo "[sync-service] Starting one-shot sync (log: $LOG_FILE) ..."
    echo "===== $(date -Is) sync-service sync begin =====" >> "$LOG_FILE"

    set +e
    (
        echo $$ > "$SYNC_PID_FILE"
        cd "$SCRIPT_DIR" || exit 1
        export PLUGIN_SERVER_URL="${PLUGIN_SERVER_URL:-http://plugins.haketech.com}"
        if [ -n "${UPSTREAM_URL:-}" ]; then
            export UPSTREAM_URL
        fi
        flock -n "$LOCK_FILE" /usr/bin/python3 "$SYNC_SCRIPT"
    )
    ec=$?
    set -e

    trap - INT TERM
    rm -f "$SYNC_PID_FILE"
    echo "===== $(date -Is) sync-service sync end exit=$ec =====" >> "$LOG_FILE"

    if [ "$ec" -eq 0 ]; then
        echo "[sync-service] Sync finished OK."
    else
        echo "[sync-service] Sync finished with exit=$ec (see $LOG_FILE)."
    fi
    exit "$ec"
}

main() {
    local cmd="${1:-}"
    case "$cmd" in
        start)  shift || true; cmd_start ;;
        stop)   shift || true; cmd_stop ;;
        status) shift || true; cmd_status ;;
        sync)   shift || true; cmd_sync ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        "")
            usage
            exit 1
            ;;
        *)
            echo "[sync-service] Unknown command: $cmd"
            usage
            exit 1
            ;;
    esac
}

main "$@"
