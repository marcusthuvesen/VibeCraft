#!/usr/bin/env python3
"""Generate a lower, clumpier 16x16 short-grass texture."""

from __future__ import annotations

import math
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "materials" / "short_grass_dense.png"
TMP = OUT.with_suffix(".ppm")


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def mix(a: int, b: int, t: float) -> int:
    return int(round(a + (b - a) * clamp(t, 0.0, 1.0)))


def draw_blade(pixels: list[list[tuple[int, int, int]]], base_x: float, height: int, lean: float, width: float) -> None:
    for step in range(height):
        t = step / max(1, height - 1)
        y = 15 - step
        center_x = base_x + lean * t + math.sin((t + base_x) * 4.2) * 0.12
        half_width = max(0.42, width * (1.0 - t * 0.78))
        left = int(math.floor(center_x - half_width))
        right = int(math.ceil(center_x + half_width))
        shade = mix(92, 216, t ** 0.8)
        for x in range(left, right + 1):
            if 0 <= x < 16:
                edge = abs(x - center_x) / max(half_width, 0.001)
                if edge > 1.08:
                    continue
                softness = 1.0 - clamp(edge, 0.0, 1.0)
                value = mix(int(shade * 0.62), shade, softness)
                old = pixels[y][x]
                if value < old[0]:
                    pixels[y][x] = (value, value, value)


def main() -> int:
    pixels = [[(255, 255, 255) for _ in range(16)] for _ in range(16)]

    # Broad low base so the tuft reads like Minecraft's grounded short grass instead
    # of a few tall spikes.
    for y in range(12, 16):
        for x in range(2, 14):
            mound = 1.0 - abs(x - 7.5) / 6.8
            if mound <= 0.0:
                continue
            ridge = clamp((y - 11) / 4.0, 0.0, 1.0)
            shade = mix(116, 176, mound * (1.0 - ridge * 0.45))
            if y == 12 and abs(x - 7.5) > 4.8:
                continue
            pixels[y][x] = (shade, shade, shade)

    blades = [
        (1.8, 5, -0.28, 0.90),
        (3.0, 7, -0.18, 1.15),
        (4.6, 6, 0.08, 1.05),
        (5.7, 8, -0.10, 1.18),
        (7.0, 7, 0.12, 1.25),
        (8.1, 9, -0.04, 1.20),
        (9.4, 7, 0.18, 1.14),
        (10.8, 8, 0.10, 1.16),
        (12.0, 6, -0.12, 1.00),
        (13.2, 5, 0.24, 0.88),
    ]
    for blade in blades:
        draw_blade(pixels, *blade)

    buf = [f"P3\n16 16\n255\n"]
    for row in pixels:
        buf.append(" ".join(f"{r} {g} {b}" for r, g, b in row))
        buf.append("\n")
    TMP.write_text("".join(buf), encoding="ascii")

    subprocess.run(
        ["magick", str(TMP), "-transparent", "white", "-define", "png:color-type=6", str(OUT)],
        check=True,
    )
    TMP.unlink(missing_ok=True)
    print(f"Wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
