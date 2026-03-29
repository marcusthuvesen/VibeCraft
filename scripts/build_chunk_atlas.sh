#!/usr/bin/env bash
# Packs selected materials into chunk_atlas.png (96x64, 16x16 tiles, 6x4 grid)
# and chunk_atlas.bgra for the renderer. Tile order must match BlockMetadata tile indices in code.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MAT="${ROOT}/assets/textures/materials"
OUT="${ROOT}/assets/textures"
TMP="${OUT}/.atlas_build"
mkdir -p "${TMP}"

resize_tile() {
  local src="$1" dst="$2"
  magick "${src}" -filter Point -resize 16x16! "${dst}"
}

# Modern Minecraft grass textures are biome-tinted at runtime.
# Bake a sane green tint into atlas tiles so they don't look gray/white.
tint_tile_green() {
  local src="$1" dst="$2" strength="${3:-58}"
  magick "${src}" -filter Point -resize 16x16! -fill "#6aa84f" -colorize "${strength}%" "${dst}"
}

# Minecraft-style *still* fluids are 16-wide vertical animation strips; take the top 16×16 frame only.
# (Resizing the whole strip to 16×16 smears every frame into one broken tile.)
still_fluid_tile() {
  local src="$1" dst="$2"
  magick "${src}" -filter Point -crop 16x16+0+0 +repage "${dst}"
}

# Source order = tile indices 0..23 (row-major: 6 columns x 4 rows).
# Prefer canonical Minecraft names when available; fall back to existing project aliases.
# Water in-game uses BlockMetadata tile 6 ← must match t06 below (water_still.png frame 0, not water.png).
tint_tile_green "${MAT}/grass_block_top.png" "${TMP}/t00.png" 62
if [[ -f "${MAT}/grass_block_side_overlay.png" ]]; then
  resize_tile "${MAT}/dirt.png" "${TMP}/_grass_side_base.png"
  tint_tile_green "${MAT}/grass_block_side_overlay.png" "${TMP}/_grass_side_overlay.png" 72
  magick "${TMP}/_grass_side_base.png" "${TMP}/_grass_side_overlay.png" -compose over -composite "${TMP}/t01.png"
else
  # Fallback if overlay texture is unavailable.
  tint_tile_green "${MAT}/grass_block_side.png" "${TMP}/t01.png" 26
fi
resize_tile "${MAT}/dirt.png" "${TMP}/t02.png"
resize_tile "${MAT}/stone.png" "${TMP}/t03.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t04.png"
resize_tile "${MAT}/coal_ore.png" "${TMP}/t05.png"
still_fluid_tile "${MAT}/water_still.png" "${TMP}/t06.png"
resize_tile "${MAT}/sand.png" "${TMP}/t07.png"
resize_tile "${MAT}/bedrock.png" "${TMP}/t08.png"
resize_tile "${MAT}/iron_ore.png" "${TMP}/t09.png"
resize_tile "${MAT}/gold_ore.png" "${TMP}/t10.png"
resize_tile "${MAT}/diamond_ore.png" "${TMP}/t11.png"
resize_tile "${MAT}/emerald_ore.png" "${TMP}/t12.png"
still_fluid_tile "${MAT}/lava_still.png" "${TMP}/t13.png"
resize_tile "${MAT}/oak_log_top.png" "${TMP}/t14.png"
resize_tile "${MAT}/oak_log.png" "${TMP}/t15.png"
resize_tile "${MAT}/oak_leaves.png" "${TMP}/t16.png"
resize_tile "${MAT}/oak_planks.png" "${TMP}/t17.png"
resize_tile "${MAT}/cobblestone.png" "${TMP}/t18.png"
resize_tile "${MAT}/sandstone.png" "${TMP}/t19.png"
resize_tile "${MAT}/crafting_table_top.png" "${TMP}/t20.png"
resize_tile "${MAT}/crafting_table_front.png" "${TMP}/t21.png"
resize_tile "${MAT}/crafting_table_side.png" "${TMP}/t22.png"
resize_tile "${MAT}/oak_planks.png" "${TMP}/t23.png"

magick montage \
  "${TMP}/t00.png" "${TMP}/t01.png" "${TMP}/t02.png" "${TMP}/t03.png" "${TMP}/t04.png" "${TMP}/t05.png" \
  "${TMP}/t06.png" "${TMP}/t07.png" "${TMP}/t08.png" "${TMP}/t09.png" "${TMP}/t10.png" "${TMP}/t11.png" \
  "${TMP}/t12.png" "${TMP}/t13.png" "${TMP}/t14.png" "${TMP}/t15.png" "${TMP}/t16.png" "${TMP}/t17.png" \
  "${TMP}/t18.png" "${TMP}/t19.png" "${TMP}/t20.png" "${TMP}/t21.png" "${TMP}/t22.png" "${TMP}/t23.png" \
  -tile 6x4 -geometry 16x16+0+0 -background none "${OUT}/chunk_atlas.png"

magick "${OUT}/chunk_atlas.png" -depth 8 "BGRA:${OUT}/chunk_atlas.bgra"
rm -rf "${TMP}"
echo "Wrote ${OUT}/chunk_atlas.png and chunk_atlas.bgra (expected $((96*64*4)) bytes)"
