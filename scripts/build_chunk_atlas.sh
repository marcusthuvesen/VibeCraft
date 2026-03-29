#!/usr/bin/env bash
# Packs selected materials into chunk_atlas.png (128x128, 16x16 tiles, 8x8 grid)
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

# Cross-plants, torch, etc. often ship as opaque white (or magenta) mats instead of real alpha.
# Strip those so chunk quads show terrain through transparent pixels (fs_chunk uses atlas alpha).
decorative_cutout_tile() {
  local src="$1" dst="$2"
  magick "${src}" -filter Point -resize 16x16! \
    -alpha set -channel RGBA \
    -fuzz 14% -transparent white \
    -fuzz 18% -transparent "#ff00ff" \
    -fuzz 18% -transparent "#fe00fe" \
    "${dst}"
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

# Source order = tile indices 0..63 (row-major: 8 columns x 8 rows).
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

# Row 5: furnace (oven) — same names as Minecraft block textures (see mcasset.cloud assets).
if [[ -f "${MAT}/furnace_top.png" ]]; then
  resize_tile "${MAT}/furnace_top.png" "${TMP}/t24.png"
else
  resize_tile "${MAT}/cobblestone.png" "${TMP}/t24.png"
fi
if [[ -f "${MAT}/furnace_side.png" ]]; then
  resize_tile "${MAT}/furnace_side.png" "${TMP}/t25.png"
else
  resize_tile "${MAT}/cobblestone.png" "${TMP}/t25.png"
fi
if [[ -f "${MAT}/furnace_front.png" ]]; then
  resize_tile "${MAT}/furnace_front.png" "${TMP}/t26.png"
else
  resize_tile "${MAT}/cobblestone.png" "${TMP}/t26.png"
fi
# Row 5 tail: chest textures.
if [[ -f "${MAT}/chest_top.png" ]]; then
  resize_tile "${MAT}/chest_top.png" "${TMP}/t27.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t27.png"
fi
if [[ -f "${MAT}/chest_side.png" ]]; then
  resize_tile "${MAT}/chest_side.png" "${TMP}/t28.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t28.png"
fi
if [[ -f "${MAT}/chest_front.png" ]]; then
  resize_tile "${MAT}/chest_front.png" "${TMP}/t29.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t29.png"
fi

# Row 6: biome-specialized surface tiles.
if [[ -f "${MAT}/powder_snow.png" ]]; then
  resize_tile "${MAT}/powder_snow.png" "${TMP}/t30.png"
else
  resize_tile "${MAT}/sand.png" "${TMP}/t30.png"
fi
if [[ -f "${MAT}/grass_block_snow.png" ]]; then
  resize_tile "${MAT}/grass_block_snow.png" "${TMP}/t31.png"
else
  resize_tile "${MAT}/grass_block_side.png" "${TMP}/t31.png"
fi
# Jungle-like lush grass variants (Minecraft grass textures with stronger green tint).
tint_tile_green "${MAT}/grass_block_top.png" "${TMP}/t32.png" 82
if [[ -f "${MAT}/grass_block_side_overlay.png" ]]; then
  resize_tile "${MAT}/dirt.png" "${TMP}/_jungle_side_base.png"
  tint_tile_green "${MAT}/grass_block_side_overlay.png" "${TMP}/_jungle_side_overlay.png" 90
  magick "${TMP}/_jungle_side_base.png" "${TMP}/_jungle_side_overlay.png" -compose over -composite "${TMP}/t33.png"
else
  tint_tile_green "${MAT}/grass_block_side.png" "${TMP}/t33.png" 48
fi
# Reserved/fallback tail tiles.
if [[ -f "${MAT}/jungle_log_top.png" ]]; then
  resize_tile "${MAT}/jungle_log_top.png" "${TMP}/t34.png"
else
  resize_tile "${MAT}/oak_log_top.png" "${TMP}/t34.png"
fi
if [[ -f "${MAT}/jungle_log.png" ]]; then
  resize_tile "${MAT}/jungle_log.png" "${TMP}/t35.png"
else
  resize_tile "${MAT}/oak_log.png" "${TMP}/t35.png"
fi

# Row 7: utility/decorative set.
if [[ -f "${MAT}/torch.png" ]]; then
  decorative_cutout_tile "${MAT}/torch.png" "${TMP}/t36.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t36.png"
fi
if [[ -f "${MAT}/tnt_top.png" ]]; then
  resize_tile "${MAT}/tnt_top.png" "${TMP}/t37.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t37.png"
fi
if [[ -f "${MAT}/tnt_bottom.png" ]]; then
  resize_tile "${MAT}/tnt_bottom.png" "${TMP}/t38.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t38.png"
fi
if [[ -f "${MAT}/tnt_side.png" ]]; then
  resize_tile "${MAT}/tnt_side.png" "${TMP}/t39.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t39.png"
fi
if [[ -f "${MAT}/glass.png" ]]; then
  resize_tile "${MAT}/glass.png" "${TMP}/t40.png"
else
  resize_tile "${MAT}/stone.png" "${TMP}/t40.png"
fi
if [[ -f "${MAT}/bricks.png" ]]; then
  resize_tile "${MAT}/bricks.png" "${TMP}/t41.png"
else
  resize_tile "${MAT}/stone_bricks.png" "${TMP}/t41.png"
fi
if [[ -f "${MAT}/bookshelf.png" ]]; then
  resize_tile "${MAT}/bookshelf.png" "${TMP}/t42.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t42.png"
fi
if [[ -f "${MAT}/glowstone.png" ]]; then
  resize_tile "${MAT}/glowstone.png" "${TMP}/t43.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t43.png"
fi

# Row 8: minerals, plants, and cactus faces.
if [[ -f "${MAT}/obsidian.png" ]]; then
  resize_tile "${MAT}/obsidian.png" "${TMP}/t44.png"
else
  resize_tile "${MAT}/deepslate.png" "${TMP}/t44.png"
fi
if [[ -f "${MAT}/gravel.png" ]]; then
  resize_tile "${MAT}/gravel.png" "${TMP}/t45.png"
else
  resize_tile "${MAT}/sand.png" "${TMP}/t45.png"
fi
if [[ -f "${MAT}/cactus_top.png" ]]; then
  resize_tile "${MAT}/cactus_top.png" "${TMP}/t46.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t46.png"
fi
if [[ -f "${MAT}/cactus_bottom.png" ]]; then
  resize_tile "${MAT}/cactus_bottom.png" "${TMP}/t47.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t47.png"
fi
if [[ -f "${MAT}/cactus_side.png" ]]; then
  resize_tile "${MAT}/cactus_side.png" "${TMP}/t48.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t48.png"
fi
if [[ -f "${MAT}/dandelion.png" ]]; then
  decorative_cutout_tile "${MAT}/dandelion.png" "${TMP}/t49.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t49.png"
fi
if [[ -f "${MAT}/poppy.png" ]]; then
  decorative_cutout_tile "${MAT}/poppy.png" "${TMP}/t50.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t50.png"
fi
if [[ -f "${MAT}/blue_orchid.png" ]]; then
  decorative_cutout_tile "${MAT}/blue_orchid.png" "${TMP}/t51.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t51.png"
fi
if [[ -f "${MAT}/allium.png" ]]; then
  decorative_cutout_tile "${MAT}/allium.png" "${TMP}/t52.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t52.png"
fi
if [[ -f "${MAT}/oxeye_daisy.png" ]]; then
  decorative_cutout_tile "${MAT}/oxeye_daisy.png" "${TMP}/t53.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t53.png"
fi
if [[ -f "${MAT}/brown_mushroom.png" ]]; then
  decorative_cutout_tile "${MAT}/brown_mushroom.png" "${TMP}/t54.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t54.png"
fi
if [[ -f "${MAT}/red_mushroom.png" ]]; then
  decorative_cutout_tile "${MAT}/red_mushroom.png" "${TMP}/t55.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t55.png"
fi
resize_tile "${MAT}/stone_bricks.png" "${TMP}/t56.png"
resize_tile "${MAT}/granite.png" "${TMP}/t57.png"
resize_tile "${MAT}/diorite.png" "${TMP}/t58.png"
resize_tile "${MAT}/andesite.png" "${TMP}/t59.png"
if [[ -f "${MAT}/jungle_leaves.png" ]]; then
  resize_tile "${MAT}/jungle_leaves.png" "${TMP}/t60.png"
else
  resize_tile "${MAT}/oak_leaves.png" "${TMP}/t60.png"
fi
resize_tile "${MAT}/mossy_cobblestone.png" "${TMP}/t61.png"
resize_tile "${MAT}/sandstone.png" "${TMP}/t62.png"
resize_tile "${MAT}/stone.png" "${TMP}/t63.png"

magick montage \
  "${TMP}/t00.png" "${TMP}/t01.png" "${TMP}/t02.png" "${TMP}/t03.png" "${TMP}/t04.png" "${TMP}/t05.png" \
  "${TMP}/t06.png" "${TMP}/t07.png" "${TMP}/t08.png" "${TMP}/t09.png" "${TMP}/t10.png" "${TMP}/t11.png" \
  "${TMP}/t12.png" "${TMP}/t13.png" "${TMP}/t14.png" "${TMP}/t15.png" "${TMP}/t16.png" "${TMP}/t17.png" \
  "${TMP}/t18.png" "${TMP}/t19.png" "${TMP}/t20.png" "${TMP}/t21.png" "${TMP}/t22.png" "${TMP}/t23.png" \
  "${TMP}/t24.png" "${TMP}/t25.png" "${TMP}/t26.png" "${TMP}/t27.png" "${TMP}/t28.png" "${TMP}/t29.png" "${TMP}/t30.png" "${TMP}/t31.png" \
  "${TMP}/t32.png" "${TMP}/t33.png" "${TMP}/t34.png" "${TMP}/t35.png" "${TMP}/t36.png" "${TMP}/t37.png" "${TMP}/t38.png" "${TMP}/t39.png" \
  "${TMP}/t40.png" "${TMP}/t41.png" "${TMP}/t42.png" "${TMP}/t43.png" "${TMP}/t44.png" "${TMP}/t45.png" "${TMP}/t46.png" "${TMP}/t47.png" \
  "${TMP}/t48.png" "${TMP}/t49.png" "${TMP}/t50.png" "${TMP}/t51.png" "${TMP}/t52.png" "${TMP}/t53.png" "${TMP}/t54.png" "${TMP}/t55.png" \
  "${TMP}/t56.png" "${TMP}/t57.png" "${TMP}/t58.png" "${TMP}/t59.png" "${TMP}/t60.png" "${TMP}/t61.png" "${TMP}/t62.png" "${TMP}/t63.png" \
  -tile 8x8 -geometry 16x16+0+0 -background none "${OUT}/chunk_atlas.png"

magick "${OUT}/chunk_atlas.png" -depth 8 "BGRA:${OUT}/chunk_atlas.bgra"
rm -rf "${TMP}"
echo "Wrote ${OUT}/chunk_atlas.png and chunk_atlas.bgra (expected $((128*128*4)) bytes)"
