#!/usr/bin/env python3
from __future__ import annotations

import math
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "materials"
SIZE = 16


def write_ppm(path: Path, rows: list[list[tuple[int, int, int]]]) -> None:
    buf = bytearray(f"P6\n{SIZE} {SIZE}\n255\n".encode("ascii"))
    for row in rows:
        for r, g, b in row:
            buf.extend((r, g, b))
    path.write_bytes(buf)


def rgb_to_png(rows: list[list[tuple[int, int, int]]], out_path: Path) -> None:
    tmp = out_path.with_suffix(".tmp.ppm")
    write_ppm(tmp, rows)
    subprocess.run(["magick", str(tmp), "-define", "png:color-type=2", str(out_path)], check=True)
    tmp.unlink(missing_ok=True)


def clamp_byte(v: float) -> int:
    return max(0, min(255, int(round(v))))


def blend(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        clamp_byte(a[0] + (b[0] - a[0]) * t),
        clamp_byte(a[1] + (b[1] - a[1]) * t),
        clamp_byte(a[2] + (b[2] - a[2]) * t),
    )


def add_color(a: tuple[int, int, int], d: tuple[float, float, float]) -> tuple[int, int, int]:
    return (
        clamp_byte(a[0] + d[0]),
        clamp_byte(a[1] + d[1]),
        clamp_byte(a[2] + d[2]),
    )


def smooth_noise(x: int, y: int, seed: float) -> float:
    return (
        math.sin(x * 0.71 + y * 0.39 + seed)
        + math.cos(x * 0.33 - y * 0.58 + seed * 1.37)
        + math.sin((x + y) * 0.21 + seed * 2.11)
    ) / 3.0


def blank(color: tuple[int, int, int]) -> list[list[tuple[int, int, int]]]:
    return [[color for _ in range(SIZE)] for _ in range(SIZE)]


def grass_top(
    dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    light: tuple[int, int, int],
    flower: tuple[int, int, int] | None = None,
) -> list[list[tuple[int, int, int]]]:
    """Painterly turf: large-scale variation only — avoids grid/dot tiling from integer modulo accents."""
    rows = blank(mid)
    for y in range(SIZE):
        for x in range(SIZE):
            broad = smooth_noise(x // 4, y // 4, 0.41)
            med = smooth_noise(x // 2, y // 2, 0.86)
            sweep = 0.5 + 0.5 * math.sin(x * 0.17 + y * 0.10 + smooth_noise(y, x, 1.15) * 1.05)
            patch = 0.5 + 0.5 * math.cos((x - y) * 0.13 + smooth_noise(x, y, 2.35) * 0.85)
            ripple = smooth_noise(x, y, 3.6) * 0.06
            t = 0.40 + broad * 0.09 + med * 0.05 + sweep * 0.12 + patch * 0.07 + ripple
            color = blend(dark, light, t)
            # Sparse irregular specks (hash from smooth noise, not row/column grids)
            speck = smooth_noise(x + y * 3, x * 5 + 17, 4.2)
            if speck > 0.62:
                color = blend(color, light, (speck - 0.62) * 0.35)
            elif speck < -0.55:
                color = blend(color, dark, (-speck - 0.55) * 0.28)
            rows[y][x] = color

    if flower is not None:
        accents = [(3, 4), (11, 5), (6, 10), (13, 12)]
        for cx, cy in accents:
            rows[cy][cx] = flower
            if cx + 1 < SIZE:
                rows[cy][cx + 1] = blend(flower, light, 0.35)
    return rows


def grass_side(
    top_dark: tuple[int, int, int],
    top_mid: tuple[int, int, int],
    top_light: tuple[int, int, int],
    soil_a: tuple[int, int, int],
    soil_b: tuple[int, int, int],
    root: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(soil_a)
    for y in range(SIZE):
        for x in range(SIZE):
            fringe = 4 + int((smooth_noise(x, 0, 1.1) * 0.5 + 0.5) * 2.99)
            if y < fringe:
                t = y / max(1, fringe - 1)
                base = blend(top_light, top_dark, t * 0.78)
                if y == fringe - 1:
                    base = blend(top_dark, root, 0.32)
                bladeNoise = smooth_noise(x // 2, y, 0.8)
                blade = smooth_noise(x, y, 2.0)
                if blade > 0.35 and y > 0:
                    base = blend(base, top_light, 0.08 + blade * 0.06)
                rows[y][x] = add_color(base, (bladeNoise * 5, smooth_noise(x, y, 1.4) * 5, 0))
            else:
                dy = y - fringe
                band = soil_a if (dy // 5) % 2 == 0 else soil_b
                if dy % 5 == 0:
                    band = add_color(band, (-8, -7, -5))
                color = add_color(
                    band,
                    (
                        smooth_noise(x // 2, y, 1.9) * 5,
                        smooth_noise(x, y // 2, 2.5) * 4,
                        smooth_noise(x, y, 3.2) * 3,
                    ),
                )
                rootBlend = smooth_noise(x, dy, 2.7)
                if rootBlend > 0.45:
                    color = blend(color, root, (rootBlend - 0.45) * 0.55)
                rows[y][x] = color
    return rows


def layered_stone(
    base_dark: tuple[int, int, int],
    base_mid: tuple[int, int, int],
    base_light: tuple[int, int, int],
    crack: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(base_mid)
    for y in range(SIZE):
        for x in range(SIZE):
            n = smooth_noise(x, y, 1.6)
            large = smooth_noise(x // 2, y // 2, 2.7)
            color = blend(base_dark, base_light, 0.46 + n * 0.14 + large * 0.12)
            vein = smooth_noise(x + 11, y - 7, 3.1)
            if vein > 0.42:
                color = add_color(color, (6, 6, 7))
            if vein < -0.38:
                color = blend(color, crack, 0.38 + (-vein - 0.38) * 0.25)
            rows[y][x] = color
    return rows


def cobble(
    mortar: tuple[int, int, int],
    stones: list[tuple[int, int, int]],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(mortar)
    cells = [
        (1, 1, 5, 4, stones[0]),
        (6, 1, 10, 5, stones[1]),
        (11, 1, 14, 4, stones[2]),
        (1, 5, 4, 9, stones[2]),
        (5, 6, 9, 10, stones[0]),
        (10, 5, 14, 9, stones[1]),
        (2, 10, 6, 14, stones[1]),
        (7, 11, 11, 14, stones[2]),
        (12, 10, 14, 14, stones[0]),
    ]
    for x0, y0, x1, y1, stone in cells:
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                edge = x in (x0, x1) or y in (y0, y1)
                color = add_color(stone, (smooth_noise(x, y, 3.1) * 7, smooth_noise(x, y, 4.1) * 7, smooth_noise(x, y, 5.1) * 7))
                rows[y][x] = blend(color, mortar, 0.18) if edge else color
    return rows


def wood_bark(
    dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    light: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(mid)
    for y in range(SIZE):
        for x in range(SIZE):
            stripe = 0.5 + 0.5 * math.sin(x * 1.35 + smooth_noise(0, y, 0.7) * 1.5)
            vertical = 0.5 + 0.5 * math.sin(y * 0.42 + x * 0.15)
            color = blend(dark, light, stripe * 0.55 + vertical * 0.10)
            if x in (0, 15):
                color = add_color(color, (-18, -14, -10))
            if x in (1, 14):
                color = add_color(color, (10, 7, 4))
            if (x + y) % 9 == 0:
                color = add_color(color, (-8, -6, -4))
            rows[y][x] = color
    return rows


def wood_top(
    ring_dark: tuple[int, int, int],
    ring_mid: tuple[int, int, int],
    ring_light: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(ring_mid)
    cx = cy = 7.5
    for y in range(SIZE):
        for x in range(SIZE):
            dx = x - cx
            dy = y - cy
            r = math.sqrt(dx * dx + dy * dy)
            wave = 0.5 + 0.5 * math.sin(r * 2.1 + smooth_noise(x, y, 2.3) * 1.1)
            color = blend(ring_dark, ring_light, wave * 0.7)
            if r > 6.6:
                color = blend(color, ring_dark, 0.42)
            rows[y][x] = color
    return rows


def leaves(
    shadow: tuple[int, int, int],
    base: tuple[int, int, int],
    light: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(base)
    for y in range(SIZE):
        for x in range(SIZE):
            major = smooth_noise(x // 3, y // 3, 0.72)
            puffA = 0.5 + 0.5 * math.sin((x + y) * 0.22 + smooth_noise(y, x, 1.3) * 1.2)
            puffB = 0.5 + 0.5 * math.cos((x - y) * 0.18 + smooth_noise(x, y, 2.1) * 1.1)
            cluster = 0.5 + 0.5 * math.sin(x * 0.38 + y * 0.16 + smooth_noise(x // 2, y // 2, 2.9) * 1.6)
            t = 0.26 + major * 0.17 + puffA * 0.15 + puffB * 0.10 + cluster * 0.11
            color = blend(shadow, light, t)
            acc = smooth_noise(x, y, 5.1)
            if acc > 0.5:
                color = blend(color, light, (acc - 0.5) * 0.28)
            elif acc < -0.48:
                color = blend(color, shadow, (-acc - 0.48) * 0.32)
            rows[y][x] = color
    return rows


def planks(
    dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    light: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(mid)
    seams = {0, 5, 10, 15}
    for y in range(SIZE):
        for x in range(SIZE):
            board = x // 5
            base = [mid, light, dark, mid][board]
            grain = 0.5 + 0.5 * math.sin(y * 0.7 + board * 1.4 + smooth_noise(x, y, 4.4))
            color = blend(dark, base, 0.52 + grain * 0.26)
            if x in seams:
                color = add_color(color, (-25, -18, -10))
            elif x - 1 in seams:
                color = add_color(color, (18, 12, 8))
            rows[y][x] = color
    return rows


def ice_top() -> list[list[tuple[int, int, int]]]:
    rows = blank((210, 232, 244))
    for y in range(SIZE):
        for x in range(SIZE):
            n = smooth_noise(x, y, 1.2)
            frost = 0.5 + 0.5 * math.sin((x - y) * 0.45)
            color = blend((184, 212, 230), (244, 248, 252), 0.48 + n * 0.12 + frost * 0.16)
            if (x + y) % 6 == 0:
                color = add_color(color, (-10, 6, 12))
            rows[y][x] = color
    for i in range(2, 14):
        rows[i][(i * 5) % 13 + 1] = (164, 220, 244)
    return rows


def ice_side() -> list[list[tuple[int, int, int]]]:
    rows = blank((198, 224, 238))
    for y in range(SIZE):
        for x in range(SIZE):
            top = blend((245, 248, 252), (214, 234, 242), y / 15.0)
            color = add_color(
                top,
                (
                    smooth_noise(x, y, 0.4) * 7,
                    smooth_noise(x, y, 1.1) * 7,
                    smooth_noise(x, y, 1.8) * 10,
                ),
            )
            if y > 9 and (x + y) % 5 == 0:
                color = blend(color, (158, 198, 220), 0.30)
            rows[y][x] = color
    return rows


def red_dust() -> list[list[tuple[int, int, int]]]:
    rows = blank((154, 74, 56))
    for y in range(SIZE):
        for x in range(SIZE):
            n = smooth_noise(x, y, 2.2)
            color = blend((124, 56, 42), (196, 106, 76), 0.45 + n * 0.18)
            if (x + y) % 4 == 0:
                color = add_color(color, (12, 6, 0))
            if (x * 2 + y * 3) % 9 == 0:
                color = blend(color, (232, 152, 92), 0.22)
            rows[y][x] = color
    return rows


def bricks(
    dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    light: tuple[int, int, int],
    mortar: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(mortar)
    bands = [(1, 4), (6, 9), (11, 14)]
    offsets = [0, 3, 1]
    for row_index, (y0, y1) in enumerate(bands):
        offset = offsets[row_index]
        x = 1 - offset
        brick_index = 0
        while x < SIZE - 1:
            width = 4 if brick_index % 2 == 0 else 5
            x0 = max(1, x)
            x1 = min(SIZE - 2, x + width - 1)
            if x1 >= x0:
                for yy in range(y0, y1 + 1):
                    for xx in range(x0, x1 + 1):
                        shade = 0.42 + smooth_noise(xx, yy, 0.9 + row_index * 0.7) * 0.10
                        color = blend(dark, light, shade + (brick_index % 3) * 0.08)
                        if xx in (x0, x1) or yy in (y0, y1):
                            color = blend(color, mortar, 0.20)
                        rows[yy][xx] = color
            x += width + 1
            brick_index += 1
    return rows


def metal_plate(
    dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    light: tuple[int, int, int],
    accent: tuple[int, int, int],
) -> list[list[tuple[int, int, int]]]:
    rows = blank(mid)
    for y in range(SIZE):
        for x in range(SIZE):
            color = blend(dark, light, 0.40 + smooth_noise(x, y, 1.5) * 0.12 + (x / 15.0) * 0.10)
            if x in (1, 14) or y in (1, 14):
                color = blend(color, dark, 0.35)
            if x in (3, 12) or y in (3, 12):
                color = blend(color, light, 0.18)
            rows[y][x] = color
    for px, py in ((3, 3), (12, 3), (3, 12), (12, 12)):
        rows[py][px] = accent
    return rows


def furnace_face(front: bool) -> list[list[tuple[int, int, int]]]:
    rows = metal_plate((72, 78, 86), (102, 110, 118), (148, 154, 160), (178, 168, 128))
    for y in range(4, 12):
        for x in range(4, 12):
            rows[y][x] = (54, 58, 64)
    if front:
        for y in range(5, 11):
            for x in range(5, 11):
                ember = 0.5 + 0.5 * math.sin((x + y) * 0.9)
                rows[y][x] = blend((206, 92, 42), (255, 194, 86), ember)
        for y in range(4, 12):
            rows[y][3] = (138, 112, 86)
            rows[y][12] = (82, 70, 58)
    else:
        for y in range(5, 11):
            for x in range(5, 11):
                rows[y][x] = blend((78, 86, 96), (126, 136, 148), 0.45 + smooth_noise(x, y, 2.2) * 0.10)
    return rows


def glowstone() -> list[list[tuple[int, int, int]]]:
    rows = blank((110, 86, 42))
    for y in range(SIZE):
        for x in range(SIZE):
            color = blend((118, 86, 38), (196, 152, 74), 0.44 + smooth_noise(x, y, 1.1) * 0.18)
            rows[y][x] = color
    for cx, cy in ((4, 4), (11, 5), (6, 10), (12, 12)):
        for y in range(cy - 2, cy + 3):
            for x in range(cx - 2, cx + 3):
                if 0 <= x < SIZE and 0 <= y < SIZE:
                    dist = abs(x - cx) + abs(y - cy)
                    rows[y][x] = blend((246, 190, 92), (255, 230, 156), max(0.0, 1.0 - dist / 4.0))
    return rows


def obsidian() -> list[list[tuple[int, int, int]]]:
    rows = blank((44, 36, 58))
    for y in range(SIZE):
        for x in range(SIZE):
            color = blend((36, 28, 48), (84, 70, 110), 0.36 + smooth_noise(x, y, 3.3) * 0.16)
            if (x + y) % 7 == 0:
                color = blend(color, (142, 110, 172), 0.20)
            rows[y][x] = color
    return rows


def chest_top() -> list[list[tuple[int, int, int]]]:
    rows = planks((104, 68, 40), (154, 104, 62), (198, 144, 94))
    for x in range(2, 14):
        rows[2][x] = (214, 172, 98)
        rows[13][x] = (86, 58, 38)
    return rows


def chest_side(front: bool) -> list[list[tuple[int, int, int]]]:
    rows = planks((96, 62, 36), (144, 96, 56), (188, 134, 84))
    for y in range(3, 13):
        rows[y][2] = (92, 58, 34)
        rows[y][13] = (198, 152, 98)
    if front:
        for y in range(5, 11):
            for x in range(6, 10):
                rows[y][x] = (214, 178, 102)
        rows[7][7] = (86, 60, 38)
        rows[8][7] = (86, 60, 38)
        rows[7][8] = (242, 220, 148)
        rows[8][8] = (242, 220, 148)
    return rows


def bookshelf() -> list[list[tuple[int, int, int]]]:
    rows = planks((82, 54, 34), (122, 82, 50), (166, 118, 72))
    for y0, y1 in ((2, 6), (9, 13)):
        for y in range(y0, y1 + 1):
            for x in range(2, 13):
                book_idx = (x - 2) // 2
                palette = [
                    ((124, 68, 54), (178, 110, 92)),
                    ((64, 112, 98), (108, 164, 148)),
                    ((96, 94, 142), (146, 144, 198)),
                    ((184, 154, 88), (220, 194, 122)),
                    ((88, 126, 64), (132, 176, 96)),
                    ((128, 86, 44), (176, 126, 72)),
                ][book_idx % 6]
                rows[y][x] = blend(palette[0], palette[1], ((y - y0) / max(1, y1 - y0)) * 0.4 + 0.3)
    for x in range(SIZE):
        rows[1][x] = (82, 54, 34)
        rows[7][x] = (74, 48, 30)
        rows[8][x] = (154, 112, 70)
        rows[14][x] = (70, 44, 28)
    return rows


def crafting_top() -> list[list[tuple[int, int, int]]]:
    rows = planks((112, 78, 46), (154, 106, 60), (196, 148, 92))
    for y in range(3, 13):
        for x in range(3, 13):
            if x in (3, 7, 11) or y in (3, 7, 11):
                rows[y][x] = (92, 64, 40)
    return rows


def crafting_side(front: bool) -> list[list[tuple[int, int, int]]]:
    rows = planks((94, 60, 34), (136, 92, 52), (178, 126, 78))
    for y in range(3, 13):
        for x in range(3, 13):
            rows[y][x] = (118, 84, 54)
    if front:
        for y in range(4, 12):
            for x in range(5, 11):
                rows[y][x] = (146, 108, 68)
        for y in range(5, 11):
            rows[y][7] = (84, 58, 36)
            rows[y][8] = (214, 168, 92)
        for x in range(5, 11):
            rows[7][x] = blend(rows[7][x], (72, 52, 36), 0.35)
    else:
        for y in range(4, 12):
            for x in range(4, 12):
                if (x + y) % 3 == 0:
                    rows[y][x] = (166, 124, 82)
    return rows


def render_all() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    textures: dict[str, list[list[tuple[int, int, int]]]] = {
        "regolith_turf_top_storybook.png": grass_top((68, 102, 50), (96, 136, 68), (142, 182, 98)),
        "regolith_turf_side_storybook.png": grass_side((60, 92, 46), (88, 126, 62), (130, 166, 90), (122, 96, 66), (148, 120, 84), (96, 76, 54)),
        "oxygen_moss_top_storybook.png": grass_top((56, 106, 82), (76, 136, 104), (112, 168, 132)),
        "oxygen_moss_side_storybook.png": grass_side((54, 104, 78), (76, 132, 102), (116, 172, 138), (104, 90, 72), (128, 112, 88), (72, 94, 82)),
        "dirt_storybook.png": layered_stone((118, 88, 58), (152, 118, 82), (186, 148, 108), (96, 72, 50)),
        "stone_storybook.png": layered_stone((132, 126, 102), (168, 160, 130), (200, 192, 162), (108, 102, 82)),
        "deepslate_storybook.png": layered_stone((54, 68, 82), (74, 90, 106), (102, 118, 136), (42, 52, 64)),
        "red_dust_top_storybook.png": red_dust(),
        "ice_shelf_top_storybook.png": ice_top(),
        "ice_shelf_side_storybook.png": ice_side(),
        "oak_log_top_storybook.png": wood_top((102, 64, 42), (136, 88, 60), (176, 118, 84)),
        "oak_log_storybook.png": wood_bark((72, 40, 28), (106, 64, 44), (146, 92, 64)),
        "oak_leaves_storybook.png": leaves((42, 78, 48), (68, 110, 66), (112, 154, 92)),
        "oak_planks_storybook.png": planks((84, 46, 30), (122, 70, 46), (160, 98, 68)),
        "jungle_log_top_storybook.png": wood_top((88, 56, 38), (120, 78, 54), (160, 106, 74)),
        "jungle_log_storybook.png": wood_bark((62, 34, 24), (92, 54, 38), (128, 76, 54)),
        "jungle_leaves_storybook.png": leaves((44, 80, 60), (68, 114, 86), (104, 148, 118)),
        "jungle_planks_storybook.png": planks((74, 44, 28), (110, 66, 42), (146, 92, 62)),
        "spruce_log_top_storybook.png": wood_top((96, 62, 46), (126, 82, 60), (160, 110, 82)),
        "spruce_log_storybook.png": wood_bark((58, 40, 34), (84, 60, 48), (118, 86, 70)),
        "spruce_leaves_storybook.png": leaves((70, 100, 110), (94, 128, 140), (136, 164, 174)),
        "cobblestone_storybook.png": cobble((54, 68, 84), [(88, 104, 120), (102, 120, 138), (122, 138, 154)]),
        "sandstone_storybook.png": layered_stone((172, 152, 112), (204, 184, 138), (232, 212, 168), (142, 126, 96)),
        "gravel_storybook.png": cobble((102, 100, 96), [(146, 142, 132), (164, 158, 146), (180, 172, 160)]),
        "moss_block_storybook.png": grass_top((66, 112, 64), (92, 140, 80), (132, 174, 104), (176, 212, 142)),
        "mossy_cobblestone_storybook.png": cobble((62, 74, 68), [(110, 122, 106), (128, 140, 120), (150, 158, 136)]),
        "furnace_top_storybook.png": furnace_face(front=False),
        "furnace_side_storybook.png": furnace_face(front=False),
        "furnace_front_storybook.png": furnace_face(front=True),
        "glowstone_storybook.png": glowstone(),
        "obsidian_storybook.png": obsidian(),
        "chest_top_storybook.png": chest_top(),
        "chest_side_storybook.png": chest_side(front=False),
        "chest_front_storybook.png": chest_side(front=True),
        "bookshelf_storybook.png": bookshelf(),
        "crafting_table_top_storybook.png": crafting_top(),
        "crafting_table_side_storybook.png": crafting_side(front=False),
        "crafting_table_front_storybook.png": crafting_side(front=True),
        "bricks_storybook.png": bricks((84, 96, 110), (112, 126, 142), (144, 158, 176), (62, 74, 88)),
    }
    for filename, rows in textures.items():
        rgb_to_png(rows, OUT / filename)
        print(f"wrote {OUT / filename}")


if __name__ == "__main__":
    render_all()
