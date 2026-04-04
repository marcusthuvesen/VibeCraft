#!/usr/bin/env bash
# Packs selected materials into chunk_atlas.png (128x240, 16x16 tiles, 8x15 grid)
# and chunk_atlas.bgra for the renderer. Tile order must match BlockMetadata tile indices in code.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECT_MAT="${ROOT}/assets/textures/materials"
# Default to repo-owned canonical Minecraft-style textures so atlas builds are self-contained.
# You can still override MC_MAT explicitly when experimenting with an external pack.
MC_MAT_DEFAULT="${PROJECT_MAT}"
MC_MAT="${MC_MAT:-${MC_MAT_DEFAULT}}"
STRICT_TEXTURES="${STRICT_TEXTURES:-1}"
MAT="${PROJECT_MAT}"
OUT="${ROOT}/assets/textures"
TMP="${OUT}/.atlas_build"
mkdir -p "${TMP}"

resize_tile() {
  local src="$1" dst="$2"
  magick "${src}" -filter Point -resize 16x16! "${dst}"
}

crop_tile() {
  local src="$1" geometry="$2" dst="$3"
  magick "${src}" -crop "${geometry}" +repage -filter Point -resize 16x16! "${dst}"
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

sample_colormap_hex() {
  local src="$1" x="$2" y="$3" fallback="$4"
  if [[ -f "${src}" ]]; then
    local rgb r g b
    rgb="$(magick "${src}" -format "%[fx:int(255*p{${x},${y}}.r)],%[fx:int(255*p{${x},${y}}.g)],%[fx:int(255*p{${x},${y}}.b)]" info: 2>/dev/null || true)"
    IFS=',' read -r r g b <<< "${rgb}"
    if [[ "${r}" =~ ^[0-9]+$ && "${g}" =~ ^[0-9]+$ && "${b}" =~ ^[0-9]+$ ]]; then
      if (( r < 0 )); then r=0; elif (( r > 255 )); then r=255; fi
      if (( g < 0 )); then g=0; elif (( g > 255 )); then g=255; fi
      if (( b < 0 )); then b=0; elif (( b > 255 )); then b=255; fi
      # Minecraft colormap PNGs contain a white unused region; treat that as invalid and fall back.
      if (( r > 248 && g > 248 && b > 248 )); then
        printf '%s\n' "${fallback}"
        return 0
      fi
      printf '#%02x%02x%02x\n' "${r}" "${g}" "${b}"
      return 0
    fi
  fi
  printf '%s\n' "${fallback}"
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

# Opaque grayscale masks (e.g. grass block tops) should tint fully solid.
opaque_tint_tile() {
  local src="$1" dst="$2" dark_tint="$3" light_tint="$4"
  magick "${src}" -filter Point -resize 16x16! -colorspace Gray -auto-level \
    \( -size 1x256 "gradient:${dark_tint}-${light_tint}" \) \
    -alpha off \
    -clut "${dst}"
}

# Vanilla grass tops read better when the grayscale source detail is multiplied by a biome tint,
# instead of being fully remapped into a bright gradient.
multiply_tint_tile() {
  local src="$1" dst="$2" tint="$3"
  magick "${src}" -filter Point -resize 16x16! -colorspace Gray -colorspace sRGB -alpha off \
    \( -size 16x16 "xc:${tint}" \) \
    -compose Multiply -composite "${dst}"
}

# Preserve authored alpha when tinting foliage textures like leaves.
preserve_alpha_tint_tile() {
  local src="$1" dst="$2" dark_tint="$3" light_tint="$4"
  local mask tinted
  mask="$(mktemp "${TMP}/mask.XXXXXX.png")"
  tinted="$(mktemp "${TMP}/tinted.XXXXXX.png")"
  magick "${src}" -filter Point -resize 16x16! -alpha extract "${mask}"
  magick "${src}" -filter Point -resize 16x16! -alpha off -colorspace Gray -auto-level \
    \( -size 1x256 "gradient:${dark_tint}-${light_tint}" \) -clut "${tinted}"
  magick "${tinted}" "${mask}" -alpha off -compose CopyOpacity -composite "${dst}"
  rm -f "${mask}" "${tinted}"
}

# Some foliage cutouts ship as white/grayscale sprites without real alpha.
cutout_tint_tile() {
  local src="$1" dst="$2" dark_tint="$3" light_tint="$4"
  local prepared mask tinted
  prepared="$(mktemp "${TMP}/prepared.XXXXXX.png")"
  mask="$(mktemp "${TMP}/mask.XXXXXX.png")"
  tinted="$(mktemp "${TMP}/tinted.XXXXXX.png")"
  magick "${src}" -filter Point -resize 16x16! \
    -alpha set -channel RGBA \
    -fuzz 14% -transparent white \
    -fuzz 18% -transparent "#ff00ff" \
    -fuzz 18% -transparent "#fe00fe" \
    "${prepared}"
  magick "${prepared}" -alpha extract "${mask}"
  magick "${prepared}" -alpha off -colorspace Gray -auto-level \
    \( -size 1x256 "gradient:${dark_tint}-${light_tint}" \) -clut "${tinted}"
  magick "${tinted}" "${mask}" -alpha off -compose CopyOpacity -composite "${dst}"
  rm -f "${prepared}" "${mask}" "${tinted}"
}

overlay_tinted_tile() {
  local base="$1" overlay="$2" dst="$3" dark_tint="$4" light_tint="$5"
  magick "${base}" -filter Point -resize 16x16! \
    \( "${overlay}" -filter Point -resize 16x16! -colorspace Gray -auto-level \
       \( -size 1x256 "gradient:${dark_tint}-${light_tint}" \) -clut \) \
    -compose Over -composite "${dst}"
}

# Minecraft-style *still* fluids are 16-wide vertical animation strips; take the top 16×16 frame only.
# (Resizing the whole strip to 16×16 smears every frame into one broken tile.)
still_fluid_tile() {
  local src="$1" dst="$2"
  magick "${src}" -filter Point -crop 16x16+0+0 +repage "${dst}"
}

# Source order = tile indices 0..87 (row-major: 8 columns x 11 rows).
# Standard Minecraft blocks should use the canonical imported project files directly.
# Water in-game uses BlockMetadata tile 6 <- must match t06 below (water_still.png frame 0).
# Canonical grass sources (prefer Prismarine 1.10-style names first to avoid ambiguous duplicates).
GRASS_BLOCK_TOP_SRC="$(pick_first_existing "${MAT}/grass_top.png" "${MAT}/grass_block_top.png")"
GRASS_BLOCK_SIDE_SRC="$(pick_first_existing "${MAT}/grass_side.png" "${MAT}/grass_block_side.png")"
GRASS_BLOCK_SIDE_OVERLAY_SRC="$(pick_first_existing "${MAT}/grass_side_overlay.png" "${MAT}/grass_block_side_overlay.png")"
MOSS_GRASS_TOP_SRC="${GRASS_BLOCK_TOP_SRC}"
MOSS_GRASS_SIDE_SRC="${GRASS_BLOCK_SIDE_SRC}"
SNOW_GRASS_TOP_SRC="${GRASS_BLOCK_TOP_SRC}"
SNOW_GRASS_SIDE_SRC="${MAT}/grass_block_snow.png"
SAND_SRC="${MAT}/sand.png"
DEEPSLATE_TOP_SRC="${MAT}/deepslate_top.png"
SANDSTONE_TOP_SRC="${MAT}/sandstone_top.png"
SANDSTONE_BOTTOM_SRC="${MAT}/sandstone_bottom.png"
MELON_TOP_SRC="${MAT}/melon_top.png"
CHEST_ATLAS_SRC="${MAT}/normal.png"
OXYGEN_RELAY_TOP_SRC="$(pick_first_existing "${MAT}/oxygen_relay_top_hytale.png" "${MAT}/oxygen_relay_top.png" "${MC_MAT}/glowstone.png" "${MAT}/glowstone.png")"
OXYGEN_RELAY_SIDE_SRC="$(pick_first_existing "${MAT}/oxygen_relay_side_hytale.png" "${MAT}/oxygen_relay_side.png" "${MC_MAT}/moss_block.png" "${MAT}/moss_block.png")"
OXYGEN_RELAY_BOTTOM_SRC="$(pick_first_existing "${MAT}/oxygen_relay_bottom_hytale.png" "${MAT}/oxygen_relay_bottom.png" "${MC_MAT}/bricks.png" "${MC_MAT}/stone_bricks.png" "${MAT}/bricks.png" "${MAT}/stone_bricks.png")"
HABITAT_PANEL_SRC="$(pick_first_existing "${MAT}/habitat_panel_hytale.png" "${MAT}/habitat_panel.png" "${MAT}/iron_block.png" "${MAT}/bricks.png")"
HABITAT_FLOOR_SRC="$(pick_first_existing "${MAT}/habitat_floor_hytale.png" "${MAT}/habitat_floor.png" "${MAT}/iron_block.png" "${MAT}/stone_bricks.png")"
HABITAT_FRAME_SRC="$(pick_first_existing "${MAT}/habitat_frame_hytale.png" "${MAT}/habitat_frame.png" "${MAT}/iron_block.png")"
AIRLOCK_PANEL_SRC="$(pick_first_existing "${MAT}/airlock_panel_hytale.png" "${MAT}/airlock_panel.png" "${MAT}/habitat_panel.png" "${MAT}/iron_block.png")"
POWER_CONDUIT_SRC="$(pick_first_existing "${MAT}/power_conduit_hytale.png" "${MAT}/power_conduit.png" "${MAT}/habitat_frame.png" "${MAT}/iron_block.png")"
GREENHOUSE_GLASS_SRC="$(pick_first_existing "${MAT}/greenhouse_glass_hytale.png" "${MAT}/greenhouse_glass.png" "${MAT}/glass.png")"
PLANTER_TRAY_SRC="$(pick_first_existing "${MAT}/planter_tray_hytale.png" "${MAT}/planter_tray.png" "${MAT}/habitat_floor.png" "${MAT}/moss_block.png")"
FIBER_SAPLING_SRC="$(pick_first_existing "${MAT}/fiber_sapling_hytale.png" "${MAT}/fiber_sapling.png" "${MAT}/oak_sapling.png" "${MAT}/dandelion.png")"
FIBER_SPROUT_SRC="$(pick_first_existing "${MAT}/fiber_sprout_hytale.png" "${MAT}/fiber_sprout.png" "${MAT}/fiber_sapling.png" "${MAT}/dandelion.png")"
WATER_STILL_SRC="${MAT}/water_still.png"
LAVA_STILL_SRC="${MAT}/lava_still.png"
FURNACE_TOP_SRC="${MAT}/furnace_top.png"
FURNACE_SIDE_SRC="${MAT}/furnace_side.png"
FURNACE_FRONT_SRC="${MAT}/furnace_front.png"
GLASS_PORTAL_SRC="${MAT}/glass.png"
GLOWSTONE_SRC="${MAT}/glowstone.png"
COAL_ORE_SRC="${MAT}/coal_ore.png"
BEDROCK_SRC="${MAT}/bedrock.png"
IRON_ORE_SRC="${MAT}/iron_ore.png"
GOLD_ORE_SRC="${MAT}/gold_ore.png"
DIAMOND_ORE_SRC="${MAT}/diamond_ore.png"
EMERALD_ORE_SRC="${MAT}/emerald_ore.png"
CACTUS_TOP_SRC="${MAT}/cactus_top.png"
CACTUS_BOTTOM_SRC="${MAT}/cactus_bottom.png"
CACTUS_SIDE_SRC="${MAT}/cactus_side.png"
DANDELION_SRC="${MAT}/dandelion.png"
POPPY_SRC="${MAT}/poppy.png"
BLUE_ORCHID_SRC="${MAT}/blue_orchid.png"
ALLIUM_SRC="${MAT}/allium.png"
OXEYE_DAISY_SRC="${MAT}/oxeye_daisy.png"
BROWN_MUSHROOM_SRC="${MAT}/brown_mushroom.png"
RED_MUSHROOM_SRC="${MAT}/red_mushroom.png"
DEAD_BUSH_SRC="$(pick_first_existing "${MAT}/dead_bush.png" "${MAT}/deadbush.png")"
VINE_SRC="${MAT}/vine.png"
LADDER_SRC="${MAT}/ladder.png"
OAK_DOOR_LOWER_SRC="$(pick_first_existing "${MAT}/door_wood_lower.png" "${MAT}/oak_door_lower.png" "${MAT}/oak_door_bottom.png")"
OAK_DOOR_UPPER_SRC="$(pick_first_existing "${MAT}/door_wood_upper.png" "${MAT}/oak_door_upper.png" "${MAT}/oak_door_top.png")"
JUNGLE_DOOR_LOWER_SRC="$(pick_first_existing "${MAT}/door_jungle_lower.png" "${MAT}/jungle_door_lower.png" "${MAT}/jungle_door_bottom.png")"
JUNGLE_DOOR_UPPER_SRC="$(pick_first_existing "${MAT}/door_jungle_upper.png" "${MAT}/jungle_door_upper.png" "${MAT}/jungle_door_top.png")"
IRON_DOOR_LOWER_SRC="$(pick_first_existing "${MAT}/door_iron_lower.png" "${MAT}/iron_door_lower.png" "${MAT}/iron_door_bottom.png")"
IRON_DOOR_UPPER_SRC="$(pick_first_existing "${MAT}/door_iron_upper.png" "${MAT}/iron_door_upper.png" "${MAT}/iron_door_top.png")"
COCOA_SRC="${MAT}/cocoa_stage2.png"
MELON_SIDE_SRC="${MAT}/melon_side.png"
BAMBOO_STALK_SRC="${MAT}/bamboo_stalk.png"
JUNGLE_PLANKS_SRC="${MAT}/jungle_planks.png"
MOSS_BLOCK_SRC="${MAT}/moss_block.png"
MOSSY_COBBLE_SRC="${MAT}/mossy_cobblestone.png"
OAK_LEAVES_SRC="${MAT}/oak_leaves.png"
JUNGLE_LEAVES_SRC="${MAT}/jungle_leaves.png"
SPRUCE_LEAVES_SRC="${MAT}/spruce_leaves.png"
SHORT_GRASS_SRC="${MAT}/short_grass.png"
FERN_SRC="${MAT}/fern.png"
PODZOL_TOP_SRC="$(pick_first_existing "${MAT}/podzol_top.png" "${MAT}/dirt_podzol_top.png")"
PODZOL_SIDE_SRC="$(pick_first_existing "${MAT}/podzol_side.png" "${MAT}/dirt_podzol_side.png")"
COARSE_DIRT_SRC="${MAT}/coarse_dirt.png"
DARK_OAK_LEAVES_SRC="${MAT}/dark_oak_leaves.png"
SCULK_SRC="${MAT}/sculk.png"
DRIPSTONE_BLOCK_SRC="${MAT}/dripstone_block.png"
GRASS_COLORMAP_SRC="${ROOT}/assets/textures/colormap/grass.png"
FOLIAGE_COLORMAP_SRC="${ROOT}/assets/textures/colormap/foliage.png"

# Vanilla-like biome tints sampled from Minecraft 1.10 colormap maps.
TEMPERATE_GRASS_DARK="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 80 144 "#567d32")"
TEMPERATE_GRASS_LIGHT="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 48 176 "#8ebf4c")"
TEMPERATE_GRASS_TINT="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 64 160 "#7aa84a")"
FOREST_GRASS_DARK="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 96 128 "#3b631f")"
FOREST_GRASS_LIGHT="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 64 160 "#7fbc45")"
DRY_GRASS_DARK="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 72 184 "#6a6121")"
DRY_GRASS_LIGHT="$(sample_colormap_hex "${GRASS_COLORMAP_SRC}" 32 208 "#b79a49")"
TEMPERATE_FOLIAGE_DARK="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 80 144 "#295b22")"
TEMPERATE_FOLIAGE_LIGHT="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 48 176 "#6fbf4b")"
JUNGLE_FOLIAGE_DARK="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 96 128 "#1d4b1b")"
JUNGLE_FOLIAGE_LIGHT="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 64 160 "#4fa43c")"
SPRUCE_FOLIAGE_DARK="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 72 184 "#213b1d")"
SPRUCE_FOLIAGE_LIGHT="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 56 168 "#4b7a38")"
BIRCH_FOLIAGE_DARK="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 96 128 "#4c742e")"
BIRCH_FOLIAGE_LIGHT="$(sample_colormap_hex "${FOLIAGE_COLORMAP_SRC}" 64 160 "#9fca63")"

# Temperate grass must read green in-game even when source tops are grayscale.
# Keep the block side on the authored vanilla tile so the dirt wall stays brown instead of over-tinted.
multiply_tint_tile "${GRASS_BLOCK_TOP_SRC}" "${TMP}/t00.png" "${TEMPERATE_GRASS_TINT}"
resize_tile "${GRASS_BLOCK_SIDE_SRC}" "${TMP}/t01.png"
resize_tile "${MAT}/dirt.png" "${TMP}/t02.png"
resize_tile "${MAT}/stone.png" "${TMP}/t03.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t04.png"
resize_tile "${COAL_ORE_SRC}" "${TMP}/t05.png"
still_fluid_tile "${WATER_STILL_SRC}" "${TMP}/t06.png"
resize_tile "${SAND_SRC}" "${TMP}/t07.png"
resize_tile "${BEDROCK_SRC}" "${TMP}/t08.png"
resize_tile "${IRON_ORE_SRC}" "${TMP}/t09.png"
resize_tile "${GOLD_ORE_SRC}" "${TMP}/t10.png"
resize_tile "${DIAMOND_ORE_SRC}" "${TMP}/t11.png"
resize_tile "${EMERALD_ORE_SRC}" "${TMP}/t12.png"
still_fluid_tile "${LAVA_STILL_SRC}" "${TMP}/t13.png"
resize_tile "${MAT}/oak_log_top.png" "${TMP}/t14.png"
resize_tile "${MAT}/oak_log.png" "${TMP}/t15.png"
preserve_alpha_tint_tile "${OAK_LEAVES_SRC}" "${TMP}/t16.png" "${TEMPERATE_FOLIAGE_DARK}" "${TEMPERATE_FOLIAGE_LIGHT}"
resize_tile "${MAT}/oak_planks.png" "${TMP}/t17.png"
resize_tile "${MAT}/cobblestone.png" "${TMP}/t18.png"
resize_tile "${MAT}/sandstone.png" "${TMP}/t19.png"
resize_tile "${MAT}/crafting_table_top.png" "${TMP}/t20.png"
resize_tile "${MAT}/crafting_table_side.png" "${TMP}/t21.png"
resize_tile "${MAT}/crafting_table_front.png" "${TMP}/t22.png"
resize_tile "${MAT}/oak_planks.png" "${TMP}/t23.png"

# Row 5: furnace.
resize_tile "${FURNACE_TOP_SRC}" "${TMP}/t24.png"
resize_tile "${FURNACE_SIDE_SRC}" "${TMP}/t25.png"
resize_tile "${FURNACE_FRONT_SRC}" "${TMP}/t26.png"
# Row 5 tail: chest textures.
resize_tile "${MAT}/chest_top.png" "${TMP}/t27.png"
resize_tile "${MAT}/chest_front.png" "${TMP}/t28.png"
resize_tile "${MAT}/chest_side.png" "${TMP}/t29.png"

# Row 6: biome-specialized surface tiles.
resize_tile "${MAT}/snow.png" "${TMP}/t30.png"
resize_tile "${SNOW_GRASS_SIDE_SRC}" "${TMP}/t31.png"
opaque_tint_tile "${MOSS_GRASS_TOP_SRC}" "${TMP}/t32.png" "#1f340f" "#4f772c"
overlay_tinted_tile "${MOSS_GRASS_SIDE_SRC}" "${GRASS_BLOCK_SIDE_OVERLAY_SRC}" "${TMP}/t33.png" "#1f340f" "#4f772c"
resize_tile "${MAT}/jungle_log_top.png" "${TMP}/t34.png"
resize_tile "${MAT}/jungle_log.png" "${TMP}/t35.png"

# Row 7: utility/decorative set.
decorative_cutout_tile "${MAT}/torch.png" "${TMP}/t36.png"
resize_tile "${MAT}/tnt_top.png" "${TMP}/t37.png"
resize_tile "${MAT}/tnt_bottom.png" "${TMP}/t38.png"
resize_tile "${MAT}/tnt_side.png" "${TMP}/t39.png"
resize_tile "${GLASS_PORTAL_SRC}" "${TMP}/t40.png"
resize_tile "${MAT}/bricks.png" "${TMP}/t41.png"
resize_tile "${MAT}/bookshelf.png" "${TMP}/t42.png"
resize_tile "${GLOWSTONE_SRC}" "${TMP}/t43.png"

# Row 8: minerals, plants, and cactus faces.
resize_tile "${MAT}/obsidian.png" "${TMP}/t44.png"
resize_tile "${MAT}/gravel.png" "${TMP}/t45.png"
resize_tile "${CACTUS_TOP_SRC}" "${TMP}/t46.png"
resize_tile "${CACTUS_BOTTOM_SRC}" "${TMP}/t47.png"
resize_tile "${CACTUS_SIDE_SRC}" "${TMP}/t48.png"
decorative_cutout_tile "${DANDELION_SRC}" "${TMP}/t49.png"
decorative_cutout_tile "${POPPY_SRC}" "${TMP}/t50.png"
decorative_cutout_tile "${BLUE_ORCHID_SRC}" "${TMP}/t51.png"
decorative_cutout_tile "${ALLIUM_SRC}" "${TMP}/t52.png"
decorative_cutout_tile "${OXEYE_DAISY_SRC}" "${TMP}/t53.png"
decorative_cutout_tile "${BROWN_MUSHROOM_SRC}" "${TMP}/t54.png"
decorative_cutout_tile "${RED_MUSHROOM_SRC}" "${TMP}/t55.png"
decorative_cutout_tile "${DEAD_BUSH_SRC}" "${TMP}/t56.png"
cutout_tint_tile "${VINE_SRC}" "${TMP}/t57.png" "#24491b" "#5a9a3d"
resize_tile "${COCOA_SRC}" "${TMP}/t58.png"
resize_tile "${MELON_SIDE_SRC}" "${TMP}/t59.png"
preserve_alpha_tint_tile "${JUNGLE_LEAVES_SRC}" "${TMP}/t60.png" "${JUNGLE_FOLIAGE_DARK}" "${JUNGLE_FOLIAGE_LIGHT}"
resize_tile "${MAT}/spruce_log_top.png" "${TMP}/t61.png"
resize_tile "${MAT}/spruce_log.png" "${TMP}/t62.png"
preserve_alpha_tint_tile "${SPRUCE_LEAVES_SRC}" "${TMP}/t63.png" "${SPRUCE_FOLIAGE_DARK}" "${SPRUCE_FOLIAGE_LIGHT}"

# Row 9 head: bamboo.
decorative_cutout_tile "${BAMBOO_STALK_SRC}" "${TMP}/t64.png"
resize_tile "${JUNGLE_PLANKS_SRC}" "${TMP}/t65.png"
resize_tile "${MOSS_BLOCK_SRC}" "${TMP}/t66.png"
resize_tile "${MOSSY_COBBLE_SRC}" "${TMP}/t67.png"
resize_tile "$(pick_first_existing "${MAT}/habitat_panel_hyassets.png" "${HABITAT_PANEL_SRC}")" "${TMP}/t68.png"
resize_tile "$(pick_first_existing "${MAT}/habitat_floor_hyassets.png" "${HABITAT_FLOOR_SRC}")" "${TMP}/t69.png"
resize_tile "$(pick_first_existing "${MAT}/habitat_frame_hyassets.png" "${HABITAT_FRAME_SRC}")" "${TMP}/t70.png"
resize_tile "$(pick_first_existing "${MAT}/greenhouse_glass_hyassets.png" "${GREENHOUSE_GLASS_SRC}")" "${TMP}/t71.png"
resize_tile "$(pick_first_existing "${MAT}/planter_tray_hyassets.png" "${PLANTER_TRAY_SRC}")" "${TMP}/t72.png"
decorative_cutout_tile "$(pick_first_existing "${MAT}/fiber_sapling_hyassets.png" "${FIBER_SAPLING_SRC}")" "${TMP}/t73.png"
decorative_cutout_tile "$(pick_first_existing "${MAT}/fiber_sprout_hyassets.png" "${FIBER_SPROUT_SRC}")" "${TMP}/t74.png"
resize_tile "$(pick_first_existing "${MAT}/airlock_panel_hyassets.png" "${AIRLOCK_PANEL_SRC}")" "${TMP}/t75.png"
resize_tile "$(pick_first_existing "${MAT}/power_conduit_hyassets.png" "${POWER_CONDUIT_SRC}")" "${TMP}/t76.png"
# Oxygen relay uses dedicated tiles so bricks (41), glowstone (43), and moss (66) are not overwritten.
resize_tile "$(pick_first_existing "${MAT}/oxygen_relay_bottom_hyassets.png" "${OXYGEN_RELAY_BOTTOM_SRC}")" "${TMP}/t77.png"
resize_tile "$(pick_first_existing "${MAT}/oxygen_relay_side_hyassets.png" "${OXYGEN_RELAY_SIDE_SRC}")" "${TMP}/t78.png"
resize_tile "$(pick_first_existing "${MAT}/oxygen_relay_top_hyassets.png" "${OXYGEN_RELAY_TOP_SRC}")" "${TMP}/t79.png"

# Row 11: use imported Minecraft-style short grass / fern sources for natural surface clutter.
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t80.png" "${FOREST_GRASS_DARK}" "${FOREST_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t81.png" "${TEMPERATE_GRASS_DARK}" "${TEMPERATE_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t82.png" "${TEMPERATE_GRASS_DARK}" "${FOREST_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t83.png" "${DRY_GRASS_DARK}" "${DRY_GRASS_LIGHT}"
cutout_tint_tile "${FERN_SRC}" "${TMP}/t84.png" "${TEMPERATE_FOLIAGE_DARK}" "${TEMPERATE_FOLIAGE_LIGHT}"
cutout_tint_tile "${FERN_SRC}" "${TMP}/t85.png" "${FOREST_GRASS_DARK}" "${FOREST_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t86.png" "${TEMPERATE_FOLIAGE_DARK}" "${TEMPERATE_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t87.png" "${TEMPERATE_GRASS_DARK}" "${FOREST_GRASS_LIGHT}"

# Row 12: exact extra face variants for supported cube blocks.
resize_tile "${DEEPSLATE_TOP_SRC}" "${TMP}/t88.png"
resize_tile "${SANDSTONE_TOP_SRC}" "${TMP}/t89.png"
resize_tile "${SANDSTONE_BOTTOM_SRC}" "${TMP}/t90.png"
resize_tile "${MELON_TOP_SRC}" "${TMP}/t91.png"
resize_tile "${MAT}/birch_log_top.png" "${TMP}/t92.png"
resize_tile "${MAT}/birch_log.png" "${TMP}/t93.png"
preserve_alpha_tint_tile "${MAT}/birch_leaves.png" "${TMP}/t94.png" "${BIRCH_FOLIAGE_DARK}" "${BIRCH_FOLIAGE_LIGHT}"
cutout_tint_tile "${FERN_SRC}" "${TMP}/t95.png" "${TEMPERATE_FOLIAGE_DARK}" "${TEMPERATE_GRASS_LIGHT}"

# Row 13: forest-floor and dark-forest support.
resize_tile "${PODZOL_TOP_SRC}" "${TMP}/t96.png"
resize_tile "${PODZOL_SIDE_SRC}" "${TMP}/t97.png"
resize_tile "${COARSE_DIRT_SRC}" "${TMP}/t98.png"
resize_tile "${MAT}/dark_oak_log_top.png" "${TMP}/t99.png"
resize_tile "${MAT}/dark_oak_log.png" "${TMP}/t100.png"
preserve_alpha_tint_tile "${DARK_OAK_LEAVES_SRC}" "${TMP}/t101.png" "${TEMPERATE_FOLIAGE_DARK}" "${FOREST_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t102.png" "${TEMPERATE_GRASS_DARK}" "${FOREST_GRASS_LIGHT}"
cutout_tint_tile "${SHORT_GRASS_SRC}" "${TMP}/t103.png" "${DRY_GRASS_DARK}" "${DRY_GRASS_LIGHT}"

# Row 14: cave-biome support, ladder, and door tiles.
resize_tile "${SCULK_SRC}" "${TMP}/t104.png"
resize_tile "${DRIPSTONE_BLOCK_SRC}" "${TMP}/t105.png"
resize_tile "${MOSS_BLOCK_SRC}" "${TMP}/t106.png"
decorative_cutout_tile "${LADDER_SRC}" "${TMP}/t107.png"
decorative_cutout_tile "${OAK_DOOR_LOWER_SRC}" "${TMP}/t108.png"
decorative_cutout_tile "${OAK_DOOR_UPPER_SRC}" "${TMP}/t109.png"
decorative_cutout_tile "${JUNGLE_DOOR_LOWER_SRC}" "${TMP}/t110.png"
decorative_cutout_tile "${JUNGLE_DOOR_UPPER_SRC}" "${TMP}/t111.png"

# Row 15: iron door plus reserved tail tiles.
decorative_cutout_tile "${IRON_DOOR_LOWER_SRC}" "${TMP}/t112.png"
decorative_cutout_tile "${IRON_DOOR_UPPER_SRC}" "${TMP}/t113.png"
resize_tile "${MAT}/deepslate.png" "${TMP}/t114.png"
resize_tile "${MAT}/stone.png" "${TMP}/t115.png"
resize_tile "${MOSSY_COBBLE_SRC}" "${TMP}/t116.png"
resize_tile "${COARSE_DIRT_SRC}" "${TMP}/t117.png"
resize_tile "${MOSS_BLOCK_SRC}" "${TMP}/t118.png"
resize_tile "${LADDER_SRC}" "${TMP}/t119.png"

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
  "${TMP}/t72.png" "${TMP}/t73.png" "${TMP}/t74.png" "${TMP}/t75.png" "${TMP}/t76.png" "${TMP}/t77.png" "${TMP}/t78.png" "${TMP}/t79.png" "${TMP}/t80.png" "${TMP}/t81.png" "${TMP}/t82.png" "${TMP}/t83.png" "${TMP}/t84.png" "${TMP}/t85.png" "${TMP}/t86.png" "${TMP}/t87.png" \
  "${TMP}/t88.png" "${TMP}/t89.png" "${TMP}/t90.png" "${TMP}/t91.png" "${TMP}/t92.png" "${TMP}/t93.png" "${TMP}/t94.png" "${TMP}/t95.png" \
  "${TMP}/t96.png" "${TMP}/t97.png" "${TMP}/t98.png" "${TMP}/t99.png" "${TMP}/t100.png" "${TMP}/t101.png" "${TMP}/t102.png" "${TMP}/t103.png" \
  "${TMP}/t104.png" "${TMP}/t105.png" "${TMP}/t106.png" "${TMP}/t107.png" "${TMP}/t108.png" "${TMP}/t109.png" "${TMP}/t110.png" "${TMP}/t111.png" \
  "${TMP}/t112.png" "${TMP}/t113.png" "${TMP}/t114.png" "${TMP}/t115.png" "${TMP}/t116.png" "${TMP}/t117.png" "${TMP}/t118.png" "${TMP}/t119.png" \
  -tile 8x15 -geometry 16x16+0+0 -background none "${OUT}/chunk_atlas.png"

magick "${OUT}/chunk_atlas.png" -depth 8 "BGRA:${OUT}/chunk_atlas.bgra"
rm -rf "${TMP}"
echo "Wrote ${OUT}/chunk_atlas.png and chunk_atlas.bgra (expected $((128*240*4)) bytes)"
