#!/usr/bin/env bash
# Packs selected materials into chunk_atlas.png (128x160, 16x16 tiles, 8x10 grid)
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

pick_source() {
  local primary="$1" fallback="$2"
  if [[ -f "${primary}" ]]; then
    printf '%s\n' "${primary}"
  else
    printf '%s\n' "${fallback}"
  fi
}

pick_first_existing() {
  for candidate in "$@"; do
    if [[ -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
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

# Source order = tile indices 0..79 (row-major: 8 columns x 10 rows).
# Prefer canonical names when available; fall back to existing project aliases used by older placeholder packs.
# Water in-game uses BlockMetadata tile 6 ← must match t06 below (water_still.png frame 0, not a squeezed strip).
GRASS_TOP_SRC="$(pick_first_existing "${MAT}/regolith_turf_top_v3.png" "${MAT}/regolith_turf_top_v2.png" "${MAT}/regolith_turf_top.png" "${MAT}/grass_block_top.png" "${MAT}/grass_top.png")"
GRASS_SIDE_SRC="$(pick_first_existing "${MAT}/regolith_turf_side_v3.png" "${MAT}/regolith_turf_side_v2.png" "${MAT}/regolith_turf_side.png" "${MAT}/grass_block_side.png" "${MAT}/grass_side.png")"
OXYGEN_GROVE_TOP_SRC="$(pick_first_existing "${MAT}/oxygen_moss_top_v4.png" "${MAT}/oxygen_moss_top_v3.png" "${MAT}/oxygen_moss_top_v2.png" "${MAT}/oxygen_moss_top.png" "${MAT}/moss_block.png" "${MAT}/grass_block_top.png" "${MAT}/grass_top.png")"
OXYGEN_GROVE_SIDE_SRC="$(pick_first_existing "${MAT}/oxygen_moss_side_v4.png" "${MAT}/oxygen_moss_side_v3.png" "${MAT}/oxygen_moss_side_v2.png" "${MAT}/oxygen_moss_side.png" "${MAT}/grass_block_side.png" "${MAT}/grass_side.png")"
ICE_SHELF_TOP_SRC="$(pick_first_existing "${MAT}/ice_shelf_top_v4.png" "${MAT}/ice_shelf_top_v3.png" "${MAT}/ice_shelf_top_v2.png" "${MAT}/ice_shelf_top.png" "${MAT}/packed_ice.png" "${MAT}/powder_snow.png" "${MAT}/sand.png")"
ICE_SHELF_SIDE_SRC="$(pick_first_existing "${MAT}/ice_shelf_side_v4.png" "${MAT}/ice_shelf_side_v3.png" "${MAT}/ice_shelf_side_v2.png" "${MAT}/ice_shelf_side.png" "${MAT}/grass_block_snow.png" "${MAT}/grass_block_side.png" "${MAT}/grass_side.png")"
RED_DUST_TOP_SRC="$(pick_first_existing "${MAT}/red_dust_top_v4.png" "${MAT}/red_dust_top_v3.png" "${MAT}/red_dust_top_v2.png" "${MAT}/red_dust_top.png" "${MAT}/sand.png")"
OXYGEN_RELAY_TOP_SRC="$(pick_first_existing "${MAT}/oxygen_relay_top.png" "${MAT}/glowstone.png")"
OXYGEN_RELAY_SIDE_SRC="$(pick_first_existing "${MAT}/oxygen_relay_side.png" "${MAT}/moss_block.png")"
OXYGEN_RELAY_BOTTOM_SRC="$(pick_first_existing "${MAT}/oxygen_relay_bottom.png" "${MAT}/bricks.png" "${MAT}/stone_bricks.png")"
HABITAT_PANEL_SRC="$(pick_first_existing "${MAT}/habitat_panel.png" "${MAT}/iron_block.png" "${MAT}/bricks.png")"
HABITAT_FLOOR_SRC="$(pick_first_existing "${MAT}/habitat_floor.png" "${MAT}/iron_block.png" "${MAT}/stone_bricks.png")"
HABITAT_FRAME_SRC="$(pick_first_existing "${MAT}/habitat_frame.png" "${MAT}/iron_block.png")"
AIRLOCK_PANEL_SRC="$(pick_first_existing "${MAT}/airlock_panel.png" "${MAT}/habitat_panel.png" "${MAT}/iron_block.png")"
POWER_CONDUIT_SRC="$(pick_first_existing "${MAT}/power_conduit.png" "${MAT}/habitat_frame.png" "${MAT}/iron_block.png")"
GREENHOUSE_GLASS_SRC="$(pick_first_existing "${MAT}/greenhouse_glass.png" "${MAT}/glass.png")"
PLANTER_TRAY_SRC="$(pick_first_existing "${MAT}/planter_tray.png" "${MAT}/habitat_floor.png" "${MAT}/moss_block.png")"
FIBER_SAPLING_SRC="$(pick_first_existing "${MAT}/fiber_sapling.png" "${MAT}/oak_sapling.png" "${MAT}/dandelion.png")"
FIBER_SPROUT_SRC="$(pick_first_existing "${MAT}/fiber_sprout.png" "${MAT}/fiber_sapling.png" "${MAT}/dandelion.png")"
WATER_STILL_SRC="$(pick_source "${MAT}/water_still.png" "${MAT}/water.png")"
LAVA_STILL_SRC="$(pick_source "${MAT}/lava_still.png" "${MAT}/lava.png")"

resize_tile "${GRASS_TOP_SRC}" "${TMP}/t00.png"
resize_tile "${GRASS_SIDE_SRC}" "${TMP}/t01.png"
resize_tile "${MAT}/dirt.png" "${TMP}/t02.png"
resize_tile "$(pick_first_existing "${MAT}/stone_v4.png" "${MAT}/stone_v3.png" "${MAT}/stone_v2.png" "${MAT}/stone.png")" "${TMP}/t03.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t04.png"
resize_tile "${MAT}/coal_ore.png" "${TMP}/t05.png"
still_fluid_tile "${WATER_STILL_SRC}" "${TMP}/t06.png"
resize_tile "${RED_DUST_TOP_SRC}" "${TMP}/t07.png"
resize_tile "${MAT}/bedrock.png" "${TMP}/t08.png"
resize_tile "${MAT}/iron_ore.png" "${TMP}/t09.png"
resize_tile "${MAT}/gold_ore.png" "${TMP}/t10.png"
resize_tile "${MAT}/diamond_ore.png" "${TMP}/t11.png"
resize_tile "${MAT}/emerald_ore.png" "${TMP}/t12.png"
still_fluid_tile "${LAVA_STILL_SRC}" "${TMP}/t13.png"
resize_tile "$(pick_first_existing "${MAT}/oak_log_top_v2.png" "${MAT}/oak_log_top.png")" "${TMP}/t14.png"
resize_tile "$(pick_first_existing "${MAT}/oak_log_v2.png" "${MAT}/oak_log.png")" "${TMP}/t15.png"
tint_tile_green "$(pick_first_existing "${MAT}/oak_leaves_v3.png" "${MAT}/oak_leaves_v2.png" "${MAT}/oak_leaves.png")" "${TMP}/t16.png" 42
resize_tile "${MAT}/oak_planks.png" "${TMP}/t17.png"
resize_tile "${MAT}/cobblestone.png" "${TMP}/t18.png"
resize_tile "$(pick_first_existing "${MAT}/sandstone_v4.png" "${MAT}/sandstone_v3.png" "${MAT}/sandstone_v2.png" "${MAT}/granite.png" "${MAT}/sandstone.png")" "${TMP}/t19.png"
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
resize_tile "${ICE_SHELF_TOP_SRC}" "${TMP}/t30.png"
resize_tile "${ICE_SHELF_SIDE_SRC}" "${TMP}/t31.png"
resize_tile "${OXYGEN_GROVE_TOP_SRC}" "${TMP}/t32.png"
resize_tile "${OXYGEN_GROVE_SIDE_SRC}" "${TMP}/t33.png"
# Reserved/fallback tail tiles.
if [[ -f "${MAT}/jungle_log_top_v2.png" ]]; then
  resize_tile "${MAT}/jungle_log_top_v2.png" "${TMP}/t34.png"
elif [[ -f "${MAT}/jungle_log_top.png" ]]; then
  resize_tile "${MAT}/jungle_log_top.png" "${TMP}/t34.png"
else
  resize_tile "$(pick_first_existing "${MAT}/oak_log_top_v2.png" "${MAT}/oak_log_top.png")" "${TMP}/t34.png"
fi
if [[ -f "${MAT}/jungle_log_v2.png" ]]; then
  resize_tile "${MAT}/jungle_log_v2.png" "${TMP}/t35.png"
elif [[ -f "${MAT}/jungle_log.png" ]]; then
  resize_tile "${MAT}/jungle_log.png" "${TMP}/t35.png"
else
  resize_tile "$(pick_first_existing "${MAT}/oak_log_v2.png" "${MAT}/oak_log.png")" "${TMP}/t35.png"
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
resize_tile "${OXYGEN_RELAY_BOTTOM_SRC}" "${TMP}/t41.png"
if [[ -f "${MAT}/bookshelf.png" ]]; then
  resize_tile "${MAT}/bookshelf.png" "${TMP}/t42.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t42.png"
fi
if [[ -f "${MAT}/glowstone_v3.png" ]]; then
  resize_tile "${MAT}/glowstone_v3.png" "${TMP}/t43.png"
elif [[ -f "${MAT}/glowstone_v2.png" ]]; then
  resize_tile "${MAT}/glowstone_v2.png" "${TMP}/t43.png"
elif [[ -f "${MAT}/glowstone.png" ]]; then
  resize_tile "${MAT}/glowstone.png" "${TMP}/t43.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t43.png"
fi
resize_tile "${OXYGEN_RELAY_TOP_SRC}" "${TMP}/t43.png"

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
if [[ -f "${MAT}/dandelion_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/dandelion_v2.png" "${TMP}/t49.png"
elif [[ -f "${MAT}/dandelion.png" ]]; then
  decorative_cutout_tile "${MAT}/dandelion.png" "${TMP}/t49.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t49.png"
fi
if [[ -f "${MAT}/poppy_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/poppy_v2.png" "${TMP}/t50.png"
elif [[ -f "${MAT}/poppy.png" ]]; then
  decorative_cutout_tile "${MAT}/poppy.png" "${TMP}/t50.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t50.png"
fi
if [[ -f "${MAT}/blue_orchid_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/blue_orchid_v2.png" "${TMP}/t51.png"
elif [[ -f "${MAT}/blue_orchid.png" ]]; then
  decorative_cutout_tile "${MAT}/blue_orchid.png" "${TMP}/t51.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t51.png"
fi
if [[ -f "${MAT}/allium_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/allium_v2.png" "${TMP}/t52.png"
elif [[ -f "${MAT}/allium.png" ]]; then
  decorative_cutout_tile "${MAT}/allium.png" "${TMP}/t52.png"
else
  resize_tile "${MAT}/grass_block_top.png" "${TMP}/t52.png"
fi
if [[ -f "${MAT}/oxeye_daisy_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/oxeye_daisy_v2.png" "${TMP}/t53.png"
elif [[ -f "${MAT}/oxeye_daisy.png" ]]; then
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
if [[ -f "${MAT}/dead_bush_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/dead_bush_v2.png" "${TMP}/t56.png"
elif [[ -f "${MAT}/dead_bush.png" ]]; then
  decorative_cutout_tile "${MAT}/dead_bush.png" "${TMP}/t56.png"
else
  decorative_cutout_tile "${MAT}/brown_mushroom.png" "${TMP}/t56.png"
fi
if [[ -f "${MAT}/vine_v3.png" ]]; then
  decorative_cutout_tile "${MAT}/vine_v3.png" "${TMP}/t57.png"
elif [[ -f "${MAT}/vine_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/vine_v2.png" "${TMP}/t57.png"
elif [[ -f "${MAT}/vine.png" ]]; then
  decorative_cutout_tile "${MAT}/vine.png" "${TMP}/t57.png"
else
  decorative_cutout_tile "${MAT}/grass_block_side_overlay.png" "${TMP}/t57.png"
fi
if [[ -f "${MAT}/cocoa_stage2_v3.png" ]]; then
  resize_tile "${MAT}/cocoa_stage2_v3.png" "${TMP}/t58.png"
elif [[ -f "${MAT}/cocoa_stage2_v2.png" ]]; then
  resize_tile "${MAT}/cocoa_stage2_v2.png" "${TMP}/t58.png"
elif [[ -f "${MAT}/cocoa_stage2.png" ]]; then
  resize_tile "${MAT}/cocoa_stage2.png" "${TMP}/t58.png"
else
  resize_tile "${MAT}/brown_mushroom.png" "${TMP}/t58.png"
fi
if [[ -f "${MAT}/melon_side_v3.png" ]]; then
  resize_tile "${MAT}/melon_side_v3.png" "${TMP}/t59.png"
elif [[ -f "${MAT}/melon_side_v2.png" ]]; then
  resize_tile "${MAT}/melon_side_v2.png" "${TMP}/t59.png"
elif [[ -f "${MAT}/melon_side.png" ]]; then
  resize_tile "${MAT}/melon_side.png" "${TMP}/t59.png"
else
  resize_tile "${MAT}/sandstone.png" "${TMP}/t59.png"
fi
if [[ -f "${MAT}/jungle_leaves_v3.png" ]]; then
  tint_tile_green "${MAT}/jungle_leaves_v3.png" "${TMP}/t60.png" 60
elif [[ -f "${MAT}/jungle_leaves_v2.png" ]]; then
  tint_tile_green "${MAT}/jungle_leaves_v2.png" "${TMP}/t60.png" 60
elif [[ -f "${MAT}/jungle_leaves.png" ]]; then
  tint_tile_green "${MAT}/jungle_leaves.png" "${TMP}/t60.png" 60
else
  tint_tile_green "${MAT}/oak_leaves.png" "${TMP}/t60.png" 60
fi
if [[ -f "${MAT}/spruce_log_top.png" ]]; then
  resize_tile "${MAT}/spruce_log_top.png" "${TMP}/t61.png"
else
  resize_tile "${MAT}/oak_log_top.png" "${TMP}/t61.png"
fi
if [[ -f "${MAT}/spruce_log.png" ]]; then
  resize_tile "${MAT}/spruce_log.png" "${TMP}/t62.png"
else
  resize_tile "${MAT}/oak_log.png" "${TMP}/t62.png"
fi
if [[ -f "${MAT}/spruce_leaves.png" ]]; then
  tint_tile_green "${MAT}/spruce_leaves.png" "${TMP}/t63.png" 28
else
  tint_tile_green "${MAT}/oak_leaves.png" "${TMP}/t63.png" 28
fi

# Row 9 head: bamboo.
if [[ -f "${MAT}/bamboo_stalk_v3.png" ]]; then
  decorative_cutout_tile "${MAT}/bamboo_stalk_v3.png" "${TMP}/t64.png"
elif [[ -f "${MAT}/bamboo_stalk_v2.png" ]]; then
  decorative_cutout_tile "${MAT}/bamboo_stalk_v2.png" "${TMP}/t64.png"
elif [[ -f "${MAT}/bamboo_stalk.png" ]]; then
  decorative_cutout_tile "${MAT}/bamboo_stalk.png" "${TMP}/t64.png"
elif [[ -f "${MAT}/bamboo.png" ]]; then
  decorative_cutout_tile "${MAT}/bamboo.png" "${TMP}/t64.png"
else
  decorative_cutout_tile "${MAT}/vine.png" "${TMP}/t64.png"
fi
if [[ -f "${MAT}/jungle_planks.png" ]]; then
  resize_tile "${MAT}/jungle_planks.png" "${TMP}/t65.png"
else
  resize_tile "${MAT}/oak_planks.png" "${TMP}/t65.png"
fi
if [[ -f "${MAT}/moss_block.png" ]]; then
  resize_tile "${MAT}/moss_block.png" "${TMP}/t66.png"
else
  tint_tile_green "${MAT}/grass_block_top.png" "${TMP}/t66.png" 78
fi
resize_tile "${OXYGEN_RELAY_SIDE_SRC}" "${TMP}/t66.png"
if [[ -f "${MAT}/mossy_cobblestone.png" ]]; then
  resize_tile "${MAT}/mossy_cobblestone.png" "${TMP}/t67.png"
else
  resize_tile "${MAT}/cobblestone.png" "${TMP}/t67.png"
fi
resize_tile "${HABITAT_PANEL_SRC}" "${TMP}/t68.png"
resize_tile "${HABITAT_FLOOR_SRC}" "${TMP}/t69.png"
resize_tile "${HABITAT_FRAME_SRC}" "${TMP}/t70.png"
resize_tile "${GREENHOUSE_GLASS_SRC}" "${TMP}/t71.png"
resize_tile "${PLANTER_TRAY_SRC}" "${TMP}/t72.png"
decorative_cutout_tile "${FIBER_SAPLING_SRC}" "${TMP}/t73.png"
decorative_cutout_tile "${FIBER_SPROUT_SRC}" "${TMP}/t74.png"
resize_tile "${AIRLOCK_PANEL_SRC}" "${TMP}/t75.png"
resize_tile "${POWER_CONDUIT_SRC}" "${TMP}/t76.png"
resize_tile "${MAT}/greenhouse_glass.png" "${TMP}/t77.png"
resize_tile "${MAT}/planter_tray.png" "${TMP}/t78.png"
decorative_cutout_tile "${MAT}/fiber_sapling.png" "${TMP}/t79.png"

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
  "${TMP}/t64.png" "${TMP}/t65.png" "${TMP}/t66.png" "${TMP}/t67.png" "${TMP}/t68.png" "${TMP}/t69.png" "${TMP}/t70.png" "${TMP}/t71.png" \
  "${TMP}/t72.png" "${TMP}/t73.png" "${TMP}/t74.png" "${TMP}/t75.png" "${TMP}/t76.png" "${TMP}/t77.png" "${TMP}/t78.png" "${TMP}/t79.png" \
  -tile 8x10 -geometry 16x16+0+0 -background none "${OUT}/chunk_atlas.png"

magick "${OUT}/chunk_atlas.png" -depth 8 "BGRA:${OUT}/chunk_atlas.bgra"
rm -rf "${TMP}"
echo "Wrote ${OUT}/chunk_atlas.png and chunk_atlas.bgra (expected $((128*160*4)) bytes)"
