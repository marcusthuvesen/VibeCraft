#!/bin/bash
# Fast preview launcher: prefer immediate launch of existing binary.
# If no binary exists yet, do a one-time app-only build.

set -euo pipefail

SCRIPT_PATH="${BASH_SOURCE[0]}"
while [[ -L "${SCRIPT_PATH}" ]]; do
  LINK_TARGET="$(readlink "${SCRIPT_PATH}")"
  if [[ "${LINK_TARGET}" = /* ]]; then
    SCRIPT_PATH="${LINK_TARGET}"
  else
    SCRIPT_PATH="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)/${LINK_TARGET}"
  fi
done
SCRIPT_DIR="$(cd "$(dirname "${SCRIPT_PATH}")" && pwd)"

if [[ -n "${VIBECRAFT_HOME:-}" ]]; then
  REPO_DIR="${VIBECRAFT_HOME}"
elif [[ -f "${SCRIPT_DIR}/../CMakeLists.txt" ]]; then
  REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
elif [[ -f "${HOME}/Documents/VibeCraft/CMakeLists.txt" ]]; then
  REPO_DIR="${HOME}/Documents/VibeCraft"
else
  echo "Could not locate VibeCraft repo. Set VIBECRAFT_HOME to your repo path." >&2
  exit 1
fi

cd "${REPO_DIR}"
export VIBECRAFT_HOME="${REPO_DIR}"

candidates=(
  "${REPO_DIR}/build/default/bin/vibecraft"
  "${REPO_DIR}/build/bin/vibecraft"
  "${REPO_DIR}/build/Release/bin/vibecraft"
  "${REPO_DIR}/build/Debug/bin/vibecraft"
)

EXE=""
BEST_MTIME=0
for path in "${candidates[@]}"; do
  if [[ -x "${path}" ]]; then
    mtime="$(stat -f %m "${path}" 2>/dev/null || stat -c %Y "${path}" 2>/dev/null || echo 0)"
    if [[ "${mtime}" =~ ^[0-9]+$ && "${mtime}" -gt "${BEST_MTIME}" ]]; then
      BEST_MTIME="${mtime}"
      EXE="${path}"
    fi
  fi
done

if [[ -z "${EXE}" ]]; then
  echo "[VibeCraft Preview] no app binary found; doing one-time app build..." >&2
  if [[ -f "${REPO_DIR}/CMakePresets.json" ]]; then
    cmake --build --preset debug --target vibecraft
  elif [[ -d "${REPO_DIR}/build" ]]; then
    cmake --build "${REPO_DIR}/build" --target vibecraft
  else
    echo "No build directory found. Configure first (e.g. cmake --preset default)." >&2
    exit 1
  fi

  for path in "${candidates[@]}"; do
    if [[ -x "${path}" ]]; then
      mtime="$(stat -f %m "${path}" 2>/dev/null || stat -c %Y "${path}" 2>/dev/null || echo 0)"
      if [[ "${mtime}" =~ ^[0-9]+$ && "${mtime}" -gt "${BEST_MTIME}" ]]; then
        BEST_MTIME="${mtime}"
        EXE="${path}"
      fi
    fi
  done
fi

if [[ -z "${EXE}" ]]; then
  echo "Could not find vibecraft under ${REPO_DIR}/build." >&2
  exit 1
fi

echo "[VibeCraft Preview] launching quickly (mtime=${BEST_MTIME}): ${EXE}" >&2
exec "${EXE}"
