#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
if [[ $# -eq 0 ]]; then
  exec python3 "$ROOT_DIR/tools/bump_version.py" --read
fi
exec python3 "$ROOT_DIR/tools/bump_version.py" "$1"
