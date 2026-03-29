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

# Source order = tile indices 0..23 (row-major: 6 columns x 4 rows).
# Prefer canonical Minecraft names when available; fall back to existing project aliases.
resize_tile "${MAT}/grass_block_top.png" "${TMP}/t00.png"
resize_tile "${MAT}/grass_block_side.png" "${TMP}/t01.png"
resize_tile "${MAT}/dirt.png" "${TMP}/t02.png"
resize_tile "${MAT}/stone.png" "${TMP}/t03.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t04.png"
resize_tile "${MAT}/coal_ore.png" "${TMP}/t05.png"
resize_tile "${MAT}/water_still.png" "${TMP}/t06.png"
resize_tile "${MAT}/sand.png" "${TMP}/t07.png"
resize_tile "${MAT}/bedrock.png" "${TMP}/t08.png"
resize_tile "${MAT}/iron_ore.png" "${TMP}/t09.png"
resize_tile "${MAT}/gold_ore.png" "${TMP}/t10.png"
resize_tile "${MAT}/diamond_ore.png" "${TMP}/t11.png"
resize_tile "${MAT}/emerald_ore.png" "${TMP}/t12.png"
resize_tile "${MAT}/lava_still.png" "${TMP}/t13.png"
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
