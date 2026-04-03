#!/usr/bin/env python3
"""
16x16 'locking' terrain tiles: grass fringe on sides, stratified earth, beveled cobble,
biolum moss, and frost ice — less flat-cube, more Portal Knights–style read within the atlas grid.

v7 regolith turf: bright meadow top + deep side fringe (~top third green) for temperate grass blocks.
v8: smoother macro variation (less high-frequency speckle / checker feel on slopes).
"""
from __future__ import annotations

import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "materials"


def write_ppm(path: Path, rows: list[list[tuple[int, int, int]]]) -> None:
    h, w = len(rows), len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == w, (i, len(r), w)
    buf = bytearray(f"P6\n{w} {h}\n255\n".encode("ascii"))
    for row in rows:
        for rgb in row:
            buf.extend(rgb)
    path.write_bytes(buf)


def ppm_to_png(ppm: Path, png: Path) -> None:
    subprocess.run(
        ["magick", str(ppm), "-define", "png:color-type=2", str(png)],
        check=True,
    )
    ppm.unlink(missing_ok=True)


def _hash_rgb(x: int, y: int, seed: int) -> tuple[int, int, int]:
    n = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    r = (n & 255) % 28
    g = ((n >> 8) & 255) % 32
    b = ((n >> 16) & 255) % 24
    return r, g, b


def grass_top_v6() -> list[list[tuple[int, int, int]]]:
    """Mottled teal regolith turf (glow forest fringe)."""
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row: list[tuple[int, int, int]] = []
        for x in range(16):
            hr, hg, hb = _hash_rgb(x, y, 0xA501)
            base_r, base_g, base_b = 52 + hr // 2, 130 + hg, 96 + hb // 2
            if (x + y * 3) % 7 == 0:
                base_g = min(255, base_g + 18)
                base_r = max(0, base_r - 6)
            if (x * y + 11) % 11 == 0:
                base_b = min(255, base_b + 14)
            row.append((base_r, base_g, base_b))
        rows.append(row)
    return rows


def grass_side_v6() -> list[list[tuple[int, int, int]]]:
    """Fringe on top rows + jagged boundary + stratified mauve dirt."""
    grass_dark = (38, 110, 78)
    grass_mid = (52, 142, 102)
    grass_hi = (78, 188, 138)
    grass_tip = (34, 92, 68)
    strata = [
        (86, 58, 96),
        (74, 48, 82),
        (96, 68, 104),
        (68, 44, 76),
        (82, 56, 90),
    ]
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row = []
        for x in range(16):
            boundary = 5 + (x % 4)  # jagged grass/dirt line (Portal-style fringe breakup)
            if y < boundary:
                # Grass blades: lighter toward top, darker at bottom fringe tips
                t = y / max(1, boundary - 1)
                if y == boundary - 1:
                    c = grass_tip
                elif t < 0.35:
                    c = grass_hi
                elif t < 0.7:
                    c = grass_mid
                else:
                    c = grass_dark
                hx, hy, hz = _hash_rgb(x, y, 0xB602)
                c = (
                    min(255, c[0] + hx // 6),
                    min(255, c[1] + hy // 5),
                    min(255, c[2] + hz // 6),
                )
            else:
                dy = y - boundary
                si = (dy // 3) % len(strata)
                base = strata[si]
                hx, hy, hz = _hash_rgb(x, dy, 0xC703)
                # Horizontal seam emphasis
                if dy % 3 == 0:
                    base = (max(0, base[0] - 12), max(0, base[1] - 8), max(0, base[2] - 10))
                c = (
                    min(255, base[0] + hx // 8),
                    min(255, base[1] + hy // 8),
                    min(255, base[2] + hz // 8),
                )
            row.append(c)
        rows.append(row)
    return rows


def grass_top_v7() -> list[list[tuple[int, int, int]]]:
    """Sunny meadow turf — warmer, higher saturation (Portal Knights–like open field)."""
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row: list[tuple[int, int, int]] = []
        for x in range(16):
            hr, hg, hb = _hash_rgb(x, y, 0xB701)
            base_r = 58 + hr // 2
            base_g = 148 + hg // 2
            base_b = 72 + hb // 2
            if (x + y * 2) % 6 == 0:
                base_g = min(255, base_g + 22)
                base_r = min(255, base_r + 10)
            if (x * y + 5) % 9 == 0:
                base_g = min(255, base_g + 14)
            if (x + y) % 7 == 0:
                base_r = max(0, base_r - 8)
                base_b = min(255, base_b + 10)
            row.append((min(255, base_r), min(255, base_g), min(255, base_b)))
        rows.append(row)
    return rows


def grass_side_v7() -> list[list[tuple[int, int, int]]]:
    """
    Side: green hangs into the top ~1/3 of the face (PK), jagged drip line, then stratified soil.
    """
    grass_hi = (88, 205, 125)
    grass_mid = (58, 158, 95)
    grass_dark = (42, 118, 78)
    grass_tip = (36, 98, 64)
    strata = [
        (88, 58, 96),
        (76, 48, 84),
        (94, 64, 102),
        (70, 44, 78),
        (82, 54, 90),
    ]
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row = []
        for x in range(16):
            # ~Top third of face is grass fringe (PK-like); jagged bottom edge into soil.
            boundary = 5 + ((x * 5 + y * 2) % 3)
            if y < boundary:
                t = y / max(1, boundary - 1)
                if y >= boundary - 1:
                    c = grass_tip
                elif t < 0.25:
                    c = grass_hi
                elif t < 0.55:
                    c = grass_mid
                else:
                    c = grass_dark
                # Vertical "blade" dark streaks in the fringe
                if (x + y * 2) % 3 == 0 and y > 1:
                    c = (max(0, c[0] - 18), max(0, c[1] - 22), max(0, c[2] - 12))
                hx, hy, hz = _hash_rgb(x, y, 0xC802)
                c = (
                    min(255, c[0] + hx // 7),
                    min(255, c[1] + hy // 6),
                    min(255, c[2] + hz // 7),
                )
            else:
                dy = y - boundary
                si = (dy // 3) % len(strata)
                base = strata[si]
                if dy % 3 == 0:
                    base = (max(0, base[0] - 12), max(0, base[1] - 8), max(0, base[2] - 10))
                hx, hy, hz = _hash_rgb(x, dy, 0xD903)
                c = (
                    min(255, base[0] + hx // 8),
                    min(255, base[1] + hy // 8),
                    min(255, base[2] + hz // 8),
                )
            row.append(c)
        rows.append(row)
    return rows


def _smooth_variation(x: int, y: int, seed: int) -> tuple[int, int, int]:
    """Low-frequency color wobble (replaces harsh grid hash for v8)."""
    s1 = math.sin(x * 0.42 + y * 0.31 + seed * 0.17)
    s2 = math.cos(x * 0.28 - y * 0.37 + seed * 0.09)
    r = int(10 + s1 * 9 + s2 * 6)
    g = int(12 + s2 * 10 + s1 * 5)
    b = int(9 + s1 * 7 + s2 * 8)
    return r, g, b


def grass_top_v8() -> list[list[tuple[int, int, int]]]:
    """Sunny meadow — v7 palette with smoother micro-variation."""
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row: list[tuple[int, int, int]] = []
        for x in range(16):
            vr, vg, vb = _smooth_variation(x, y, 0xC901)
            base_r = 58 + vr // 3
            base_g = 148 + vg // 3
            base_b = 72 + vb // 3
            sun = math.sin(x * 0.55 + y * 0.48) * 0.5 + 0.5
            if sun > 0.72:
                base_g = min(255, base_g + 18)
                base_r = min(255, base_r + 8)
            elif sun < 0.35:
                base_r = max(0, base_r - 6)
                base_b = min(255, base_b + 8)
            row.append((min(255, base_r), min(255, base_g), min(255, base_b)))
        rows.append(row)
    return rows


def grass_side_v8() -> list[list[tuple[int, int, int]]]:
    """Side fringe + strata; blade streaks use smooth sin instead of % grid."""
    grass_hi = (88, 205, 125)
    grass_mid = (58, 158, 95)
    grass_dark = (42, 118, 78)
    grass_tip = (36, 98, 64)
    strata = [
        (88, 58, 96),
        (76, 48, 84),
        (94, 64, 102),
        (70, 44, 78),
        (82, 54, 90),
    ]
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row = []
        for x in range(16):
            wave = 5.0 + 1.8 * math.sin(x * 0.9 + y * 0.4)
            boundary = int(max(4.0, min(7.0, wave)))
            if y < boundary:
                t = y / max(1, boundary - 1)
                if y >= boundary - 1:
                    c = grass_tip
                elif t < 0.25:
                    c = grass_hi
                elif t < 0.55:
                    c = grass_mid
                else:
                    c = grass_dark
                blade = math.sin(x * 1.1 + y * 2.0)
                if blade > 0.55 and y > 1:
                    c = (max(0, c[0] - 14), max(0, c[1] - 18), max(0, c[2] - 10))
                vr, vg, vb = _smooth_variation(x, y, 0xDA02)
                c = (
                    min(255, c[0] + vr // 5),
                    min(255, c[1] + vg // 5),
                    min(255, c[2] + vb // 5),
                )
            else:
                dy = y - boundary
                si = (dy // 3) % len(strata)
                base = strata[si]
                if dy % 3 == 0:
                    base = (max(0, base[0] - 12), max(0, base[1] - 8), max(0, base[2] - 10))
                vr, vg, vb = _smooth_variation(x, dy, 0xEB03)
                c = (
                    min(255, base[0] + vr // 6),
                    min(255, base[1] + vg // 6),
                    min(255, base[2] + vb // 6),
                )
            row.append(c)
        rows.append(row)
    return rows


def grass_top_v9() -> list[list[tuple[int, int, int]]]:
    """Hytale-leaning meadow turf: sunlit yellow-greens with broad painterly variation."""
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row: list[tuple[int, int, int]] = []
        for x in range(16):
            wave = math.sin(x * 0.44 + y * 0.31 + 0.7) * 0.5 + 0.5
            drift = math.cos(x * 0.19 - y * 0.27 + 1.9) * 0.5 + 0.5
            vr, vg, vb = _smooth_variation(x, y, 0xFA01)
            base_r = 76 + int(wave * 20) + vr // 4
            base_g = 146 + int(drift * 34) + vg // 3
            base_b = 54 + int((1.0 - wave) * 18) + vb // 5
            if wave > 0.72:
                base_r += 9
                base_g += 10
            if drift < 0.34:
                base_b += 8
            row.append((min(255, base_r), min(255, base_g), min(255, base_b)))
        rows.append(row)
    return rows


def grass_side_v9() -> list[list[tuple[int, int, int]]]:
    """Lush overhang fringe and warmer soil bands for brighter adventure-biome cliffs."""
    grass_hi = (122, 214, 108)
    grass_mid = (86, 174, 92)
    grass_dark = (58, 126, 70)
    grass_tip = (48, 104, 60)
    strata = [
        (112, 86, 86),
        (96, 70, 74),
        (120, 94, 92),
        (86, 62, 68),
        (104, 78, 80),
    ]
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row = []
        for x in range(16):
            boundary = int(max(4.0, min(8.0, 5.3 + math.sin(x * 0.85 + 0.6) * 1.6)))
            if y < boundary:
                t = y / max(1, boundary - 1)
                if y >= boundary - 1:
                    c = grass_tip
                elif t < 0.24:
                    c = grass_hi
                elif t < 0.58:
                    c = grass_mid
                else:
                    c = grass_dark
                blade = math.sin(x * 1.2 + y * 1.8 + 0.7)
                if blade > 0.48 and y > 1:
                    c = (max(0, c[0] - 18), max(0, c[1] - 22), max(0, c[2] - 12))
                vr, vg, vb = _smooth_variation(x, y, 0xFB02)
                c = (
                    min(255, c[0] + vr // 5),
                    min(255, c[1] + vg // 5),
                    min(255, c[2] + vb // 6),
                )
            else:
                dy = y - boundary
                base = strata[(dy // 3) % len(strata)]
                if dy % 3 == 0:
                    base = (max(0, base[0] - 14), max(0, base[1] - 10), max(0, base[2] - 10))
                vr, vg, vb = _smooth_variation(x, dy, 0xFC03)
                c = (
                    min(255, base[0] + vr // 6),
                    min(255, base[1] + vg // 6),
                    min(255, base[2] + vb // 7),
                )
            row.append(c)
        rows.append(row)
    return rows


def cobble_portal_v7() -> list[list[tuple[int, int, int]]]:
    """4×4 masonry with softer intra-cell gradients (less harsh brick grid)."""
    img: list[list[tuple[int, int, int]]] = [[(0, 0, 0) for _ in range(16)] for _ in range(16)]
    stone_bases = [
        (108, 108, 118),
        (98, 100, 112),
        (118, 116, 126),
        (92, 94, 104),
    ]
    for by in range(4):
        for bx in range(4):
            sb = stone_bases[(bx + by * 2) % len(stone_bases)]
            for ly in range(4):
                for lx in range(4):
                    y, x = by * 4 + ly, bx * 4 + lx
                    fx = (lx + 0.5) / 4.0
                    fy = (ly + 0.5) / 4.0
                    # Corner bevel read without sharp 2×2 noise.
                    corner = (1.0 - fx) * (1.0 - fy)
                    lift = 18.0 * (1.0 - corner)
                    shade = -16.0 * corner
                    r = int(sb[0] + lift + shade + math.sin(fx * 6.2) * 4.0)
                    g = int(sb[1] + lift + shade + math.sin(fy * 6.2) * 4.0)
                    b = int(sb[2] + lift + shade + math.sin((fx + fy) * 5.0) * 3.0)
                    vr, vg, vb = _smooth_variation(x, y, 0xF104)
                    img[y][x] = (
                        min(255, max(0, r + vr // 12)),
                        min(255, max(0, g + vg // 12)),
                        min(255, max(0, b + vb // 12)),
                    )
    return img


def dirt_portal_v6() -> list[list[tuple[int, int, int]]]:
    """Stratified subsoil with subtle horizontal locking."""
    rows = []
    bands = [
        (88, 62, 98),
        (76, 50, 86),
        (92, 64, 100),
        (70, 46, 78),
        (84, 58, 94),
    ]
    for y in range(16):
        row = []
        bi = (y // 3) % len(bands)
        for x in range(16):
            base = bands[bi]
            if y % 3 == 0:
                base = (max(0, base[0] - 10), max(0, base[1] - 8), max(0, base[2] - 12))
            hx, hy, hz = _hash_rgb(x, y, 0xD804)
            row.append(
                (
                    min(255, base[0] + hx // 7),
                    min(255, base[1] + hy // 7),
                    min(255, base[2] + hz // 7),
                )
            )
        rows.append(row)
    return rows


def cobble_portal_v6() -> list[list[tuple[int, int, int]]]:
    """Per-stone bevel: lighter TL, darker BR within each 4x4 cell."""
    img: list[list[tuple[int, int, int]]] = [[(0, 0, 0)] * 16 for _ in range(16)]
    stone_bases = [
        (108, 108, 118),
        (98, 100, 112),
        (118, 116, 126),
        (92, 94, 104),
    ]
    for by in range(4):
        for bx in range(4):
            sb = stone_bases[(bx + by * 2) % len(stone_bases)]
            for ly in range(4):
                for lx in range(4):
                    y, x = by * 4 + ly, bx * 4 + lx
                    r, g, b = sb
                    if lx <= 1 and ly <= 1:
                        r, g, b = min(255, r + 22), min(255, g + 22), min(255, b + 24)
                    elif lx >= 2 and ly >= 2:
                        r, g, b = max(0, r - 18), max(0, g - 16), max(0, b - 14)
                    elif lx == 0 or ly == 0:
                        r, g, b = min(255, r + 8), min(255, g + 8), min(255, b + 8)
                    hx, hy, hz = _hash_rgb(x, y, 0xE905)
                    img[y][x] = (
                        min(255, r + hx // 10),
                        min(255, g + hy // 10),
                        min(255, b + hz // 10),
                    )
    return img


def moss_top_v6() -> list[list[tuple[int, int, int]]]:
    """Biolum oxygen moss — cyan-green mottle."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            hr, hg, hb = _hash_rgb(x, y, 0xF106)
            r = 32 + hr // 3
            g = 168 + hg // 2
            b = 148 + hb // 2
            if (x + y) % 5 == 0:
                g = min(255, g + 20)
                b = min(255, b + 12)
            row.append((min(255, r), min(255, g), min(255, b)))
        rows.append(row)
    return rows


def moss_side_v6() -> list[list[tuple[int, int, int]]]:
    """Moss fringe + dark wet soil strata."""
    hi = (48, 210, 175)
    mid = (36, 168, 138)
    dk = (28, 120, 102)
    soil = [(44, 62, 58), (36, 52, 50), (50, 68, 64), (32, 48, 46)]
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            bnd = 6 + (x % 3)
            if y < bnd:
                t = y / max(1, bnd - 1)
                if t < 0.4:
                    c = hi
                elif t < 0.75:
                    c = mid
                else:
                    c = dk
                h = _hash_rgb(x, y, 0x1107)
                c = (min(255, c[0] + h[0] // 5), min(255, c[1] + h[1] // 5), min(255, c[2] + h[2] // 5))
            else:
                dy = y - bnd
                sb = soil[(dy // 3) % len(soil)]
                if dy % 3 == 0:
                    sb = (max(0, sb[0] - 8), max(0, sb[1] - 10), max(0, sb[2] - 8))
                h = _hash_rgb(x, dy, 0x2208)
                c = (
                    min(255, sb[0] + h[0] // 8),
                    min(255, sb[1] + h[1] // 8),
                    min(255, sb[2] + h[2] // 8),
                )
            row.append(c)
        rows.append(row)
    return rows


def moss_top_v7() -> list[list[tuple[int, int, int]]]:
    """Richer oxygen grove canopy floor: layered emerald/teal with soft glow pockets."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            wave = math.sin(x * 0.42 + y * 0.34 + 2.1) * 0.5 + 0.5
            pulse = math.cos(x * 0.27 - y * 0.29 + 0.4) * 0.5 + 0.5
            hr, hg, hb = _hash_rgb(x, y, 0x1307)
            r = 26 + hr // 4 + int(pulse * 12)
            g = 132 + hg // 3 + int(wave * 42)
            b = 92 + hb // 4 + int(pulse * 34)
            if wave > 0.72:
                g = min(255, g + 22)
                b = min(255, b + 16)
            row.append((min(255, r), min(255, g), min(255, b)))
        rows.append(row)
    return rows


def moss_side_v7() -> list[list[tuple[int, int, int]]]:
    """Thicker luminous fringe with humid dark strata for denser jungle reads."""
    hi = (60, 224, 166)
    mid = (42, 176, 126)
    dk = (30, 118, 88)
    soil = [(40, 66, 52), (34, 56, 46), (46, 74, 58), (30, 50, 42)]
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            bnd = int(max(5.0, min(8.0, 6.0 + math.sin(x * 0.78) * 1.2)))
            if y < bnd:
                t = y / max(1, bnd - 1)
                if t < 0.33:
                    c = hi
                elif t < 0.72:
                    c = mid
                else:
                    c = dk
                vr, vg, vb = _smooth_variation(x, y, 0x2408)
                c = (
                    min(255, c[0] + vr // 5),
                    min(255, c[1] + vg // 4),
                    min(255, c[2] + vb // 4),
                )
            else:
                dy = y - bnd
                base = soil[(dy // 3) % len(soil)]
                if dy % 3 == 0:
                    base = (max(0, base[0] - 8), max(0, base[1] - 10), max(0, base[2] - 8))
                vr, vg, vb = _smooth_variation(x, dy, 0x2509)
                c = (
                    min(255, base[0] + vr // 7),
                    min(255, base[1] + vg // 7),
                    min(255, base[2] + vb // 7),
                )
            row.append(c)
        rows.append(row)
    return rows


def ice_top_v6() -> list[list[tuple[int, int, int]]]:
    """Frost crystals on pale cyan ice."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            hr, hg, hb = _hash_rgb(x, y, 0x3309)
            r = 200 + hr // 4
            g = 232 + hg // 5
            b = 255
            if (x ^ y) % 4 == 0:
                r, g = max(0, r - 12), max(0, g - 8)
            row.append((min(255, r), min(255, g), min(255, b)))
        rows.append(row)
    return rows


def ice_side_v6() -> list[list[tuple[int, int, int]]]:
    """Snow fringe top, banded crystalline wall."""
    snow = (240, 248, 255)
    snow2 = (220, 235, 250)
    ice_b = [(170, 210, 235), (150, 195, 225), (185, 218, 240), (160, 200, 228)]
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            bnd = 5 + (x % 2)
            if y < bnd:
                c = snow if (x + y) % 2 == 0 else snow2
                h = _hash_rgb(x, y, 0x440A)
                c = (min(255, c[0] + h[0] // 12), min(255, c[1] + h[1] // 12), min(255, c[2] + h[2] // 14))
            else:
                dy = y - bnd
                ib = ice_b[(dy // 3) % len(ice_b)]
                if dy % 3 == 0:
                    ib = (max(0, ib[0] - 14), max(0, ib[1] - 12), max(0, ib[2] - 10))
                h = _hash_rgb(x, dy, 0x550B)
                c = (
                    min(255, ib[0] + h[0] // 7),
                    min(255, ib[1] + h[1] // 7),
                    min(255, ib[2] + h[2] // 7),
                )
            row.append(c)
        rows.append(row)
    return rows


def ice_top_v7() -> list[list[tuple[int, int, int]]]:
    """Cold alpine shelf with sharper blue-white crystal break-up."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            wave = math.sin(x * 0.65 + y * 0.44 + 1.2) * 0.5 + 0.5
            hr, hg, hb = _hash_rgb(x, y, 0x360A)
            r = 208 + hr // 5 - int((1.0 - wave) * 12)
            g = 230 + hg // 6 - int((1.0 - wave) * 10)
            b = 255
            if ((x ^ y) + y) % 5 == 0:
                r = max(0, r - 16)
                g = max(0, g - 10)
            row.append((min(255, r), min(255, g), b))
        rows.append(row)
    return rows


def ice_side_v7() -> list[list[tuple[int, int, int]]]:
    """Deeper shelf ledges and colder blue bands for snowy cliffs."""
    snow = (244, 249, 255)
    snow2 = (224, 238, 252)
    ice_b = [(160, 208, 238), (144, 190, 226), (176, 220, 245), (150, 198, 232)]
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            bnd = 5 + ((x + y) % 2)
            if y < bnd:
                c = snow if (x + y) % 2 == 0 else snow2
                vr, vg, vb = _smooth_variation(x, y, 0x470B)
                c = (min(255, c[0] + vr // 10), min(255, c[1] + vg // 10), min(255, c[2] + vb // 12))
            else:
                dy = y - bnd
                base = ice_b[(dy // 3) % len(ice_b)]
                if dy % 3 == 0:
                    base = (max(0, base[0] - 16), max(0, base[1] - 13), max(0, base[2] - 10))
                vr, vg, vb = _smooth_variation(x, dy, 0x480C)
                c = (
                    min(255, base[0] + vr // 6),
                    min(255, base[1] + vg // 6),
                    min(255, base[2] + vb // 6),
                )
            row.append(c)
        rows.append(row)
    return rows


def red_dust_top_v6() -> list[list[tuple[int, int, int]]]:
    """Wind-locked ripples on alien sand."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            wave = ((x + y * 2) // 2) % 3
            base_r = 190 + wave * 8 + _hash_rgb(x, y, 0x660C)[0] // 6
            base_g = 92 + _hash_rgb(x, y, 0x770D)[1] // 5
            base_b = 68 + _hash_rgb(x, y, 0x880E)[2] // 5
            row.append((min(255, base_r), min(255, base_g), min(255, base_b)))
        rows.append(row)
    return rows


def red_dust_top_v7() -> list[list[tuple[int, int, int]]]:
    """Warm dune crests with broader terracotta ripples for sandy biomes."""
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            ridge = math.sin((x * 0.7 + y * 0.18) * 2.1) * 0.5 + 0.5
            drift = math.cos((x * 0.24 - y * 0.35) * 1.8) * 0.5 + 0.5
            hr, hg, hb = _hash_rgb(x, y, 0x690D)
            base_r = 198 + int(ridge * 22) + hr // 6
            base_g = 102 + int(drift * 14) + hg // 7
            base_b = 72 + int((1.0 - ridge) * 10) + hb // 8
            if ridge > 0.74:
                base_r += 10
                base_g += 6
            row.append((min(255, base_r), min(255, base_g), min(255, base_b)))
        rows.append(row)
    return rows


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    tmp = OUT / ".locking_gen.ppm"
    specs = [
        ("regolith_turf_top_v9.png", grass_top_v9),
        ("regolith_turf_side_v9.png", grass_side_v9),
        ("regolith_turf_top_v8.png", grass_top_v8),
        ("regolith_turf_side_v8.png", grass_side_v8),
        ("regolith_turf_top_v7.png", grass_top_v7),
        ("regolith_turf_side_v7.png", grass_side_v7),
        ("regolith_turf_top_v6.png", grass_top_v6),
        ("regolith_turf_side_v6.png", grass_side_v6),
        ("dirt_portal_v6.png", dirt_portal_v6),
        ("cobble_portal_v7.png", cobble_portal_v7),
        ("cobble_portal_v6.png", cobble_portal_v6),
        ("oxygen_moss_top_v7.png", moss_top_v7),
        ("oxygen_moss_side_v7.png", moss_side_v7),
        ("oxygen_moss_top_v6.png", moss_top_v6),
        ("oxygen_moss_side_v6.png", moss_side_v6),
        ("ice_shelf_top_v7.png", ice_top_v7),
        ("ice_shelf_side_v7.png", ice_side_v7),
        ("ice_shelf_top_v6.png", ice_top_v6),
        ("ice_shelf_side_v6.png", ice_side_v6),
        ("red_dust_top_v7.png", red_dust_top_v7),
        ("red_dust_top_v6.png", red_dust_top_v6),
    ]
    for name, make_rows in specs:
        rows = make_rows()
        write_ppm(tmp, rows)
        ppm_to_png(tmp, OUT / name)
        print("Wrote", OUT / name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
