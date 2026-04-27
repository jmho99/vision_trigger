#!/bin/bash
set -euo pipefail

cleanup() {
    kill "$PID1" "$PID2" "$PID3" 2>/dev/null || true
    wait "$PID1" "$PID2" "$PID3" 2>/dev/null || true
}
trap cleanup SIGINT SIGTERM

/usr/bin/python3 /home/antlab/main_ws/io_manager/antlab_led_manager.py &
PID1=$!

/usr/bin/python3 /home/antlab/main_ws/io_manager/antlab_watchdog.py &
PID2=$!

/usr/bin/python3 /home/antlab/main_ws/io_manager/antlab_trigger.py &
PID3=$!

wait "$PID1" "$PID2" "$PID3"


