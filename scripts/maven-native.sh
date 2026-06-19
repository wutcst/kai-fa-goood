#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ACTION="${1:-}"

case "$ACTION" in
  format)
    exec bash "$ROOT_DIR/scripts/clang-format-check.sh"
    ;;
  build)
    exec bash "$ROOT_DIR/scripts/build-native.sh"
    ;;
  test)
    exec bash "$ROOT_DIR/scripts/run-native-tests.sh"
    ;;
  *)
    echo "Usage: $0 {format|build|test}"
    exit 1
    ;;
esac
