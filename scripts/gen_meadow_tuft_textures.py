#!/usr/bin/env python3
"""Generate 16x16 meadow tuft billboard textures for lush grass overlays."""
from __future__ import annotations

import math
import random
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "textures" / "materials"


@dataclass(frozen=True)
class TuftSpec:
    name: str
    seed: int
    min_height: int = 6
    max_height: int = 10
    blade_softness: float = 0.60
    base: tuple[int, int, int] = (40, 110, 78)
    mid: tuple[int, int, int] = (62, 152, 106)
    tip: tuple[int, int, int] = (122, 212, 156)
    shadow: tuple[int, int, int] = (32, 84, 62)
    accent: tuple[int, int, int] = (250, 180, 72)
    accent_chance: float = 0.0
    accent_rows: int = 2
    hue_shift: float = 0.0


def write_ppm(path: Path, rows: list[list[tuple[int, int, int]]]) -> None:
    h = len(rows)
    w = len(rows[0])
    buf = bytearray()
    buf.extend(f"P6\n{w} {h}\n255\n".encode("ascii"))
    for row in rows:
        for r, g, b in row:
            buf.extend((r, g, b))
    path.write_bytes(buf)


def ppm_to_png(ppm: Path, png: Path) -> None:
    subprocess.run(
        ["magick", str(ppm), "-define", "png:color-type=2", str(png)],
        check=True,
    )
    ppm.unlink(missing_ok=True)


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


def mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        int(round(a[0] + (b[0] - a[0]) * t)),
        int(round(a[1] + (b[1] - a[1]) * t)),
        int(round(a[2] + (b[2] - a[2]) * t)),
    )


def hash_float(x: int, seed: int) -> float:
    n = (x * 374761393 + seed * 668265263) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177 & 0xFFFFFFFF
    return (n & 0xFFFFFF) / float(0xFFFFFF)


def tuft_pixels(spec: TuftSpec) -> list[list[tuple[int, int, int]]]:
    rng = random.Random(spec.seed)
    rows = [[(255, 255, 255) for _ in range(16)] for _ in range(16)]
    heights: list[float] = []
    offsets: list[float] = []
    for x in range(16):
        noise = math.sin((x + spec.seed * 0.07) * 0.42) * 1.0
        noise += math.cos((x * 0.5 + spec.seed * 0.13)) * 0.6
        noise += (hash_float(x, spec.seed) - 0.5) * 1.1
        h = spec.min_height + (spec.max_height - spec.min_height) * clamp01(0.56 + noise * 0.08)
        heights.append(h)
        offsets.append((hash_float(x, spec.seed + 91) - 0.5) * 0.7)

    for y in range(16):
        for x in range(16):
            height = heights[x]
            base_line = 15 - height
            feather = spec.blade_softness + hash_float(x, spec.seed + y * 31) * 0.25
            feather *= 1.0 + offsets[x] * 0.08
            coverage = clamp01((y - base_line) / max(1.0, height))
            if coverage <= 0.0:
                continue
            shade = coverage ** (1.2 + hash_float(x + y, spec.seed + 7) * 0.4)
            body = mix(spec.shadow, spec.mid, shade)
            tip_mix = clamp01((coverage - 0.35) * 1.4)
            color = mix(body, spec.tip, tip_mix)
            hue_shift = spec.hue_shift
            if hue_shift != 0.0:
                color = (
                    int(clamp01(color[0] / 255.0 + hue_shift) * 255),
                    int(clamp01(color[1] / 255.0 + hue_shift * 0.4) * 255),
                    int(clamp01(color[2] / 255.0 + hue_shift * -0.4) * 255),
                )
            noise = hash_float(x * 17 + y * 13, spec.seed + 19)
            jitter = int(noise * 4 - 2)
            r = clamp01((color[0] + jitter) / 255.0)
            g = clamp01((color[1] + jitter) / 255.0)
            b = clamp01((color[2] + jitter * 0.7) / 255.0)
            rows[y][x] = (int(r * 255), int(g * 255), int(b * 255))
            if coverage > 0.20 and x > 0 and rows[y][x - 1] == (255, 255, 255):
                if hash_float(x + y * 19, spec.seed + 37) > 0.52:
                    rows[y][x - 1] = mix(rows[y][x], spec.shadow, 0.14)
            if coverage > 0.26 and x + 1 < 16 and rows[y][x + 1] == (255, 255, 255):
                if hash_float(x * 23 + y * 7, spec.seed + 53) > 0.64:
                    rows[y][x + 1] = mix(rows[y][x], spec.tip, 0.10)

    if spec.accent_chance > 0.0:
        attempts = int(16 * spec.accent_chance) + 1
        for _ in range(attempts):
            ax = rng.randrange(0, 16)
            ay = rng.randrange(max(0, 16 - spec.accent_rows), 16)
            if hash_float(ax + ay * 17, spec.seed + 211) > spec.accent_chance:
                continue
            rows[ay][ax] = spec.accent
    return rows


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    tmp = OUT / ".meadow_tuft.ppm"
    specs = [
        TuftSpec(
            "meadow_tuft_dense.png",
            seed=0x1001,
            min_height=8,
            max_height=11,
            base=(58, 122, 54),
            mid=(88, 150, 72),
            tip=(134, 182, 98),
            shadow=(42, 88, 42),
            accent=(228, 210, 146),
            accent_chance=0.06,
        ),
        TuftSpec(
            "meadow_tuft_sparse.png",
            seed=0x1002,
            min_height=6,
            max_height=9,
            blade_softness=0.78,
            base=(62, 124, 60),
            mid=(92, 150, 80),
            tip=(140, 184, 106),
            shadow=(46, 92, 50),
        ),
        TuftSpec(
            "meadow_tuft_flowers.png",
            seed=0x1003,
            accent=(247, 136, 196),
            accent_chance=0.14,
            accent_rows=3,
            min_height=7,
            max_height=10,
            base=(60, 126, 62),
            mid=(94, 154, 84),
            tip=(142, 186, 110),
        ),
        TuftSpec(
            "meadow_tuft_gold.png",
            seed=0x1004,
            accent=(248, 196, 92),
            accent_chance=0.28,
            base=(64, 132, 88),
            mid=(92, 172, 122),
            tip=(142, 226, 168),
        ),
        TuftSpec(
            "meadow_tuft_teal.png",
            seed=0x1005,
            base=(30, 120, 140),
            mid=(46, 178, 192),
            tip=(120, 236, 228),
            shadow=(24, 96, 120),
            accent=(180, 248, 255),
            accent_chance=0.18,
        ),
        TuftSpec(
            "meadow_tuft_cyan_bloom.png",
            seed=0x1006,
            base=(30, 132, 136),
            mid=(52, 188, 178),
            tip=(140, 246, 224),
            accent=(210, 220, 255),
            accent_chance=0.26,
            hue_shift=0.04,
        ),
        TuftSpec(
            "meadow_tuft_clover.png",
            seed=0x1007,
            min_height=6,
            max_height=9,
            base=(62, 126, 68),
            mid=(92, 156, 92),
            tip=(144, 188, 118),
            shadow=(44, 96, 54),
            accent=(255, 255, 255),
            accent_chance=0.08,
            accent_rows=4,
        ),
        TuftSpec(
            "meadow_tuft_sprout.png",
            seed=0x1008,
            min_height=6,
            max_height=9,
            base=(66, 130, 70),
            mid=(98, 160, 96),
            tip=(150, 194, 122),
            accent=(248, 200, 142),
            accent_chance=0.08,
            accent_rows=2,
        ),
    ]
    for spec in specs:
        rows = tuft_pixels(spec)
        write_ppm(tmp, rows)
        ppm_to_png(tmp, OUT / spec.name)
        print("Wrote", OUT / spec.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
