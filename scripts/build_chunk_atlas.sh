#!/usr/bin/env bash
# Packs assets/textures/materials/*.png into chunk_atlas.png (64x64, 16x16 tiles, 4x4 grid)
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

# Source order = tile indices 0..15 (row-major: 4 columns x 4 rows).
resize_tile "${MAT}/grass_top.png" "${TMP}/t00.png"
resize_tile "${MAT}/grass_side.png" "${TMP}/t01.png"
resize_tile "${MAT}/dirt.png" "${TMP}/t02.png"
resize_tile "${MAT}/stone.png" "${TMP}/t03.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t04.png"
resize_tile "${MAT}/coal_ore.png" "${TMP}/t05.png"
resize_tile "${MAT}/water.png" "${TMP}/t06.png"
resize_tile "${MAT}/sand.png" "${TMP}/t07.png"
resize_tile "${MAT}/bedrock.png" "${TMP}/t08.png"
resize_tile "${MAT}/iron_ore.png" "${TMP}/t09.png"
resize_tile "${MAT}/gold_ore.png" "${TMP}/t10.png"
resize_tile "${MAT}/diamond_ore.png" "${TMP}/t11.png"
resize_tile "${MAT}/emerald_ore.png" "${TMP}/t12.png"
resize_tile "${MAT}/lava.png" "${TMP}/t13.png"
# Unused atlas slots (keep deterministic UVs)
cp "${TMP}/t03.png" "${TMP}/t14.png"
cp "${TMP}/t03.png" "${TMP}/t15.png"

magick montage \
  "${TMP}/t00.png" "${TMP}/t01.png" "${TMP}/t02.png" "${TMP}/t03.png" \
  "${TMP}/t04.png" "${TMP}/t05.png" "${TMP}/t06.png" "${TMP}/t07.png" \
  "${TMP}/t08.png" "${TMP}/t09.png" "${TMP}/t10.png" "${TMP}/t11.png" \
  "${TMP}/t12.png" "${TMP}/t13.png" "${TMP}/t14.png" "${TMP}/t15.png" \
  -tile 4x4 -geometry 16x16+0+0 -background none "${OUT}/chunk_atlas.png"

magick "${OUT}/chunk_atlas.png" -depth 8 "BGRA:${OUT}/chunk_atlas.bgra"
rm -rf "${TMP}"
echo "Wrote ${OUT}/chunk_atlas.png and chunk_atlas.bgra (expected $((64*64*4)) bytes)"
