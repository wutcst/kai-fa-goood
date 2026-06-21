#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/fire-ice/build"

if [ -f "$BUILD_DIR/CMakeCache.txt" ] && ! grep -q 'CMAKE_GENERATOR:INTERNAL=Ninja' "$BUILD_DIR/CMakeCache.txt"; then
  echo "Removing stale CMake cache..."
  rm -rf "$BUILD_DIR"
fi

echo "Configuring native build..."
cmake -S "$ROOT_DIR/fire-ice" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "Building native binaries..."
cmake --build "$BUILD_DIR" --config Release --parallel

test -f "$BUILD_DIR/fireice_server.exe"
echo "Native build completed."
