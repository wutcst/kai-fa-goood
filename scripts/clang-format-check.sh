#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FAILED=0

echo "Checking C++ source formatting..."
while IFS= read -r file; do
  if ! clang-format --dry-run --Werror "$file"; then
    echo "Formatting differs: $file"
    echo "Fix with: clang-format -i $file"
    FAILED=1
  fi
done < <(find "$ROOT_DIR/client/src" "$ROOT_DIR/server/src" "$ROOT_DIR/shared/src" -name '*.cpp' -o -name '*.hpp' | sort)

if [ "$FAILED" -ne 0 ]; then
  echo "Code formatting check failed."
  exit 1
fi

echo "All source files passed format check."
