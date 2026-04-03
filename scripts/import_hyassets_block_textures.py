#!/usr/bin/env python3
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HY = ROOT / "third_party" / "HyAssets" / "blocktextures"
OUT = ROOT / "assets" / "textures" / "materials"


def run_magick(*args: str) -> None:
    subprocess.run(["magick", *args], check=True)


def src(name: str) -> Path:
    path = HY / name
    if not path.exists():
        raise FileNotFoundError(path)
    return path


def copy_asset(source_name: str, out_name: str) -> None:
    shutil.copyfile(src(source_name), OUT / out_name)


def tint_asset(source_name: str, out_name: str, fill: str, percent: int) -> None:
    run_magick(str(src(source_name)), "-fill", fill, "-colorize", f"{percent}%", str(OUT / out_name))


def modulate_asset(source_name: str, out_name: str, brightness: int, saturation: int, hue: int = 100) -> None:
    run_magick(
        str(src(source_name)),
        "-modulate",
        f"{brightness},{saturation},{hue}",
        str(OUT / out_name),
    )


def overlay_center(base_name: str, overlay_name: str, out_name: str, overlay_scale: str = "65%") -> None:
    run_magick(
        str(src(base_name)),
        "(",
        str(src(overlay_name)),
        "-resize",
        overlay_scale,
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / out_name),
    )


def derive_leaves(out_name: str, fill: str, percent: int) -> None:
    run_magick(
        str(src("grass.png")),
        "(",
        str(src("fern.png")),
        "-resize",
        "92%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        "-fill",
        fill,
        "-colorize",
        f"{percent}%",
        str(OUT / out_name),
    )


def derive_torch(out_name: str) -> None:
    run_magick(
        "-size",
        "32x32",
        "canvas:none",
        "(",
        str(src("wood_small.png")),
        "-resize",
        "44%",
        ")",
        "-gravity",
        "south",
        "-geometry",
        "+0+2",
        "-compose",
        "over",
        "-composite",
        "(",
        str(src("campfire_fire.png")),
        "-resize",
        "70%",
        ")",
        "-gravity",
        "north",
        "-geometry",
        "+0+2",
        "-compose",
        "over",
        "-composite",
        str(OUT / out_name),
    )


def derive_glowstone(out_name: str) -> None:
    run_magick(
        str(src("sandstone.png")),
        "(",
        str(src("pink cave crystal.png")),
        "-resize",
        "72%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "screen",
        "-composite",
        str(OUT / out_name),
    )


def derive_obsidian(out_name: str) -> None:
    run_magick(
        str(src("stone2.png")),
        "(",
        str(src("portal.png")),
        "-resize",
        "100%",
        "-modulate",
        "70,110,120",
        ")",
        "-compose",
        "multiply",
        "-composite",
        "-modulate",
        "55,80,110",
        str(OUT / out_name),
    )


def derive_crafting_variants() -> None:
    run_magick(
        str(src("oak_planks.png")),
        "(",
        str(src("stone_bricks.png")),
        "-resize",
        "55%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / "crafting_table_top_hyassets.png"),
    )
    run_magick(
        str(src("oak_planks.png")),
        "(",
        str(src("chiseled_stone_bricks.png")),
        "-resize",
        "62%",
        ")",
        "-gravity",
        "center",
        "-geometry",
        "+0+2",
        "-compose",
        "over",
        "-composite",
        str(OUT / "crafting_table_front_hyassets.png"),
    )
    run_magick(
        str(src("oak_planks.png")),
        "(",
        str(src("stone_bricks.png")),
        "-resize",
        "52%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "overlay",
        "-composite",
        str(OUT / "crafting_table_side_hyassets.png"),
    )


def derive_furnace_variants() -> None:
    copy_asset("stone_bricks.png", "furnace_top_hyassets.png")
    copy_asset("stone_bricks.png", "furnace_side_hyassets.png")
    run_magick(
        str(src("stone_bricks.png")),
        "(",
        str(src("fancy_window_lit.png")),
        "-resize",
        "64%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / "furnace_front_hyassets.png"),
    )


def derive_chest_variants() -> None:
    copy_asset("oak_planks.png", "chest_top_hyassets.png")
    copy_asset("oak_planks.png", "chest_side_hyassets.png")
    run_magick(
        str(src("oak_planks.png")),
        "(",
        str(src("iron_bars.png")),
        "-resize",
        "38%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / "chest_front_hyassets.png"),
    )


def derive_tnt_variants() -> None:
    copy_asset("red_sandstone_top.png", "tnt_top_hyassets.png")
    copy_asset("red_sandstone_bottom.png", "tnt_bottom_hyassets.png")
    run_magick(
        str(src("red_sandstone.png")),
        "(",
        str(src("redstone_small.png")),
        "-resize",
        "56%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / "tnt_side_hyassets.png"),
    )


def derive_bamboo(out_name: str) -> None:
    run_magick(
        str(src("tall_grass_bottom.png")),
        "(",
        str(src("vine.png")),
        "-resize",
        "80%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / out_name),
    )


def derive_fiber_plants() -> None:
    copy_asset("fern.png", "fiber_sapling_hyassets.png")
    run_magick(
        str(src("tall_grass_bottom.png")),
        "(",
        str(src("tall_grass_top.png")),
        "-gravity",
        "south",
        ")",
        "-layers",
        "merge",
        str(OUT / "fiber_sprout_hyassets.png"),
    )


def derive_scifi_variants() -> None:
    copy_asset("fancy_window_lit.png", "habitat_panel_hyassets.png")
    copy_asset("smooth_stone.png", "habitat_floor_hyassets.png")
    copy_asset("chiseled_stone_bricks.png", "habitat_frame_hyassets.png")
    copy_asset("fancy_window.png", "greenhouse_glass_hyassets.png")
    copy_asset("sandstone_bricks.png", "planter_tray_hyassets.png")
    copy_asset("bigdoor.png", "airlock_panel_hyassets.png")
    run_magick(
        str(src("smooth_stone.png")),
        "(",
        str(src("iron_bars.png")),
        "-resize",
        "55%",
        ")",
        "-gravity",
        "center",
        "-compose",
        "over",
        "-composite",
        str(OUT / "power_conduit_hyassets.png"),
    )
    copy_asset("smooth_stone.png", "oxygen_relay_bottom_hyassets.png")
    copy_asset("fancy_window_lit.png", "oxygen_relay_side_hyassets.png")
    copy_asset("smooth_stone.png", "oxygen_relay_top_hyassets.png")


def derive_misc() -> None:
    copy_asset("fancy_window.png", "glass_hyassets.png")
    copy_asset("sandgravel.png", "gravel_hyassets.png")
    copy_asset("cactus.png", "cactus_top_hyassets.png")
    copy_asset("cactus.png", "cactus_bottom_hyassets.png")
    copy_asset("cactus.png", "cactus_side_hyassets.png")
    copy_asset("vine.png", "vine_hyassets.png")
    tint_asset("grass_block_top.png", "moss_block_hyassets.png", "#4a8c42", 38)
    copy_asset("mossy_cobblestone.png", "mossy_cobblestone_hyassets.png")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    copy_asset("grass_block_top.png", "regolith_turf_top_hyassets.png")
    copy_asset("grass_block_side.png", "regolith_turf_side_hyassets.png")
    copy_asset("podzol_top.png", "oxygen_moss_top_hyassets.png")
    copy_asset("podzol_side.png", "oxygen_moss_side_hyassets.png")
    copy_asset("blue_ice.png", "ice_shelf_top_hyassets.png")
    copy_asset("packed_ice.png", "ice_shelf_side_hyassets.png")
    copy_asset("red_sand.png", "red_dust_top_hyassets.png")
    copy_asset("dirt.png", "dirt_hyassets.png")
    copy_asset("stone.png", "stone_hyassets.png")
    modulate_asset("stone2.png", "deepslate_hyassets.png", brightness=52, saturation=85, hue=100)
    modulate_asset("cobblestone.png", "bedrock_hyassets.png", brightness=38, saturation=50, hue=100)
    overlay_center("stone.png", "coal_small.png", "coal_ore_hyassets.png", "68%")
    overlay_center("stone.png", "iron_small.png", "iron_ore_hyassets.png", "68%")
    overlay_center("stone.png", "gold_small.png", "gold_ore_hyassets.png", "68%")
    overlay_center("stone.png", "diamond_small.png", "diamond_ore_hyassets.png", "68%")
    overlay_center("stone.png", "emerald_small.png", "emerald_ore_hyassets.png", "68%")
    copy_asset("water_still.png", "water_still_hyassets.png")
    copy_asset("lava_still.png", "lava_still_hyassets.png")
    copy_asset("oak_log_top.png", "oak_log_top_hyassets.png")
    copy_asset("oak_log.png", "oak_log_hyassets.png")
    derive_leaves("oak_leaves_hyassets.png", "#4d9a42", 26)
    copy_asset("oak_planks.png", "oak_planks_hyassets.png")
    copy_asset("cobblestone.png", "cobblestone_hyassets.png")
    copy_asset("sandstone.png", "sandstone_hyassets.png")
    derive_crafting_variants()
    derive_furnace_variants()
    derive_chest_variants()
    copy_asset("snow.png", "snow_grass_top_hyassets.png")
    copy_asset("grass_block_snow.png", "snow_grass_side_hyassets.png")
    copy_asset("jungle_log_top.png", "jungle_log_top_hyassets.png")
    copy_asset("jungle_log.png", "jungle_log_hyassets.png")
    derive_torch("torch_hyassets.png")
    derive_tnt_variants()
    copy_asset("bricks.png", "bricks_hyassets.png")
    copy_asset("bookshelf.png", "bookshelf_hyassets.png")
    derive_glowstone("glowstone_hyassets.png")
    derive_obsidian("obsidian_hyassets.png")
    derive_misc()
    derive_leaves("jungle_leaves_hyassets.png", "#2d8f3b", 36)
    copy_asset("spruce_log_top.png", "spruce_log_top_hyassets.png")
    copy_asset("spruce_log.png", "spruce_log_hyassets.png")
    derive_leaves("spruce_leaves_hyassets.png", "#3e6f5c", 42)
    derive_bamboo("bamboo_stalk_hyassets.png")
    copy_asset("jungle_planks.png", "jungle_planks_hyassets.png")
    derive_fiber_plants()
    derive_scifi_variants()


if __name__ == "__main__":
    main()
