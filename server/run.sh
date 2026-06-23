#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/Release"
EXE="$BIN/fireice_server.exe"
PID_FILE="$BIN/fireice_server.pid"
LOG_FILE="$BIN/fireice_server.log"

if [[ ! -f "$EXE" ]]; then
  echo "Please run build.bat or scripts/build.bat first." >&2
  exit 1
fi

if [[ -f "$PID_FILE" ]]; then
  SERVER_PID="$(tr -d '[:space:]' < "$PID_FILE")"
  if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "Server already running (PID $SERVER_PID)."
    echo "Log: $LOG_FILE"
    exit 0
  fi
  rm -f "$PID_FILE"
fi

echo "Starting Fire-Ice server in background..."
cd "$BIN"
nohup "$EXE" --pid-file "$PID_FILE" --log-file "$LOG_FILE" >/dev/null 2>&1 &
disown

sleep 1
if [[ ! -f "$PID_FILE" ]]; then
  echo "Failed to start server. Check $LOG_FILE" >&2
  exit 1
fi

SERVER_PID="$(tr -d '[:space:]' < "$PID_FILE")"
echo "Server started (PID $SERVER_PID)."
echo "UDP port: 24567"
echo "Log: $LOG_FILE"
echo "Stop with: server/stop.sh"
