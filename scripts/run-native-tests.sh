#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/fire-ice/build"

test -f "$BUILD_DIR/fireice_tests.exe"
test -f "$BUILD_DIR/fireice_server.exe"

echo "Running native unit tests..."
cd "$BUILD_DIR"
./fireice_tests.exe

echo "Running server smoke test..."
./fireice_server.exe &
SERVER_PID=$!
sleep 3
if kill -0 "$SERVER_PID" 2>/dev/null; then
  kill "$SERVER_PID" 2>/dev/null || true
  wait "$SERVER_PID" 2>/dev/null || true
  echo "Server smoke test passed."
else
  echo "Server exited prematurely during smoke test."
  exit 1
fi
