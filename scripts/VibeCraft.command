#!/bin/bash
# Copy or symlink this to ~/Desktop/VibeCraft.command for double-click launch.
# By default this builds before launch so you always run the latest code/assets.

set -euo pipefail

# Resolve the real script path even when launched through a Desktop symlink.
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
unset VIBECRAFT_AUTOSTART_SINGLEPLAYER
unset VIBECRAFT_AUTOSTART_NEW_WORLD

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

atlas_png="${REPO_DIR}/assets/textures/chunk_atlas.png"
atlas_bgra="${REPO_DIR}/assets/textures/chunk_atlas.bgra"
atlas_script="${REPO_DIR}/scripts/build_chunk_atlas.sh"
atlas_rebuild_policy="${VIBECRAFT_REBUILD_ATLAS:-auto}"  # auto|always|never

should_rebuild_atlas=0
if [[ "${atlas_rebuild_policy}" == "always" ]]; then
  should_rebuild_atlas=1
elif [[ "${atlas_rebuild_policy}" == "auto" ]]; then
  if [[ ! -f "${atlas_png}" || ! -f "${atlas_bgra}" ]]; then
    should_rebuild_atlas=1
  elif [[ "${atlas_script}" -nt "${atlas_png}" || "${atlas_script}" -nt "${atlas_bgra}" ]]; then
    should_rebuild_atlas=1
  else
    for path in "${REPO_DIR}"/assets/textures/materials/*.png "${REPO_DIR}"/assets/textures/colormap/*.png; do
      [[ -e "${path}" ]] || continue
      if [[ "${path}" -nt "${atlas_png}" || "${path}" -nt "${atlas_bgra}" ]]; then
        should_rebuild_atlas=1
        break
      fi
    done
  fi
fi

if [[ "${should_rebuild_atlas}" == "1" ]]; then
  echo "[VibeCraft] rebuilding chunk atlas from project textures..." >&2
  "${REPO_DIR}/scripts/build_chunk_atlas.sh"
else
  echo "[VibeCraft] chunk atlas up to date (policy=${atlas_rebuild_policy})." >&2
fi

if [[ "${VIBECRAFT_SKIP_BUILD:-0}" != "1" ]]; then
  build_policy="${VIBECRAFT_BUILD_POLICY:-auto}"  # auto|always|never
  should_build=1

  if [[ "${build_policy}" == "never" ]]; then
    should_build=0
  elif [[ "${build_policy}" == "auto" && -n "${EXE}" ]]; then
    should_build=0
    if command -v git >/dev/null 2>&1 && [[ -d "${REPO_DIR}/.git" ]]; then
      repo_head_time="$(git -C "${REPO_DIR}" log -1 --format=%ct 2>/dev/null || echo 0)"
      if [[ "${repo_head_time}" =~ ^[0-9]+$ && "${BEST_MTIME}" -lt "${repo_head_time}" ]]; then
        should_build=1
      fi
      if [[ "${should_build}" == "0" ]]; then
        watch_paths=(CMakeLists.txt CMakePresets.json cmake include src assets/shaders)

        while IFS= read -r relpath; do
          [[ -n "${relpath}" ]] || continue
          abspath="${REPO_DIR}/${relpath}"
          if [[ ! -e "${abspath}" || "${abspath}" -nt "${EXE}" ]]; then
            should_build=1
            break
          fi
        done < <(
          {
            git -C "${REPO_DIR}" diff --name-only -- "${watch_paths[@]}"
            git -C "${REPO_DIR}" diff --cached --name-only -- "${watch_paths[@]}"
            git -C "${REPO_DIR}" ls-files --others --exclude-standard -- "${watch_paths[@]}"
          } | awk '!seen[$0]++'
        )
      fi
    fi
  fi

  if [[ "${should_build}" == "1" ]]; then
    echo "[VibeCraft] building latest changes..." >&2
    if [[ -f "${REPO_DIR}/CMakePresets.json" ]]; then
      cmake --build --preset debug --target vibecraft
    elif [[ -d "${REPO_DIR}/build" ]]; then
      cmake --build "${REPO_DIR}/build" --target vibecraft
    else
      echo "No build directory found. Configure first (e.g. cmake --preset debug)." >&2
      exit 1
    fi
  else
    echo "[VibeCraft] build up to date (policy=${build_policy})." >&2
  fi
fi

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
  echo "Could not find vibecraft under ${REPO_DIR}/build. Build failed or missing output." >&2
  exit 1
fi

echo "[VibeCraft] launching (mtime=${BEST_MTIME}): ${EXE}" >&2
exec "${EXE}"
