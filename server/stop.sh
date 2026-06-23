#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PID_FILE="$ROOT/build/Release/fireice_server.pid"

if [[ ! -f "$PID_FILE" ]]; then
  echo "Server is not running (no pid file)."
  exit 0
fi

SERVER_PID="$(tr -d '[:space:]' < "$PID_FILE")"
if [[ -z "$SERVER_PID" ]]; then
  rm -f "$PID_FILE"
  echo "Removed empty pid file."
  exit 0
fi

if kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "Stopping server (PID $SERVER_PID)..."
  kill "$SERVER_PID" 2>/dev/null || true
  for _ in $(seq 1 20); do
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
      break
    fi
    sleep 0.2
  done
  if kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -9 "$SERVER_PID" 2>/dev/null || true
  fi
  echo "Server stopped."
else
  echo "Process $SERVER_PID not found. Cleaning stale pid file."
fi

rm -f "$PID_FILE"
