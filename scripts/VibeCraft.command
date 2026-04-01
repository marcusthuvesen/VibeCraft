#!/bin/bash
# Copy or symlink this to ~/Desktop/VibeCraft.command for double-click launch.
# Picks the newest vibecraft binary among build/ and build/default/ so CMake preset
# builds and plain `cmake --build build` never leave you on a stale binary.

set -euo pipefail

VIBECRAFT_HOME="$(cd "$(dirname "$0")/.." && pwd)"

candidates=(
  "${VIBECRAFT_HOME}/build/default/bin/vibecraft"
  "${VIBECRAFT_HOME}/build/bin/vibecraft"
  "${VIBECRAFT_HOME}/build/Release/bin/vibecraft"
  "${VIBECRAFT_HOME}/build/Debug/bin/vibecraft"
)

EXE=""
BEST_MTIME=0
for path in "${candidates[@]}"; do
  if [[ -x "$path" ]]; then
    mtime=$(stat -f %m "$path" 2>/dev/null || stat -c %Y "$path" 2>/dev/null || echo 0)
    if [[ "$mtime" =~ ^[0-9]+$ && "$mtime" -gt "$BEST_MTIME" ]]; then
      BEST_MTIME=$mtime
      EXE="$path"
    fi
  fi
done

if [[ -z "$EXE" ]]; then
  echo "Could not find vibecraft under ${VIBECRAFT_HOME}/build. Build first." >&2
  exit 1
fi

echo "[VibeCraft] launching: $EXE" >&2
cd "$(dirname "$EXE")"
exec "./$(basename "$EXE")"
