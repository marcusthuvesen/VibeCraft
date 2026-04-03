#!/usr/bin/env python3
"""
16×16 tree textures: cylindrical bark + soft leaf billboards (Portal Knights–style).
v4 logs: smoother bark (less speckle). v5 leaves: RGBA with soft rim alpha for crossed-quad canopies.
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


def write_pgm_gray(path: Path, rows: list[list[int]]) -> None:
    h, w = len(rows), len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == w, (i, len(r), w)
    buf = bytearray(f"P5\n{w} {h}\n255\n".encode("ascii"))
    for row in rows:
        buf.extend(bytes(row))
    path.write_bytes(buf)


def ppm_to_png(ppm: Path, png: Path) -> None:
    subprocess.run(
        ["magick", str(ppm), "-define", "png:color-type=2", str(png)],
        check=True,
    )
    ppm.unlink(missing_ok=True)


def rgb_and_alpha_to_png(
    rgb_ppm: Path,
    alpha_pgm: Path,
    png: Path,
) -> None:
    subprocess.run(
        [
            "magick",
            str(rgb_ppm),
            str(alpha_pgm),
            "-compose",
            "CopyOpacity",
            "-composite",
            str(png),
        ],
        check=True,
    )
    rgb_ppm.unlink(missing_ok=True)
    alpha_pgm.unlink(missing_ok=True)


def _h(x: int, y: int, s: int) -> int:
    n = (x * 1315423911 + y * 1140671485 + s) & 0x7FFFFFFF
    return n % 17


def _h_soft(x: int, y: int, s: int) -> tuple[int, int, int]:
    """Lower amplitude grain than _h (less checkerboard on bark)."""
    n = (x * 1315423911 + y * 1140671485 + s) & 0xFFFFFFFF
    return (n % 7) - 3, ((n >> 8) % 7) - 3, ((n >> 16) % 7) - 3


def log_top_rounded(
    ring_light: tuple[int, int, int],
    ring_dark: tuple[int, int, int],
    bark: tuple[int, int, int],
    soft_noise: bool = False,
) -> list[list[tuple[int, int, int]]]:
    """Concentric growth rings + soft radial falloff (rounded end cap)."""
    cx, cy = 7.5, 7.5
    rows: list[list[tuple[int, int, int]]] = []
    for y in range(16):
        row = []
        for x in range(16):
            dx, dy = x - cx, y - cy
            dist = math.sqrt(dx * dx + dy * dy) / 11.2
            ring = 0.5 + 0.5 * abs(math.sin(dist * 8.5))
            edge = max(0.0, 1.0 - dist * 1.1)
            t = max(0.0, min(1.0, ring * 0.65 + edge * 0.35))
            c = (
                int(bark[0] + (ring_dark[0] - bark[0]) * t * 0.85 + (ring_light[0] - ring_dark[0]) * ring * 0.35),
                int(bark[1] + (ring_dark[1] - bark[1]) * t * 0.85 + (ring_light[1] - ring_dark[1]) * ring * 0.35),
                int(bark[2] + (ring_dark[2] - bark[2]) * t * 0.85 + (ring_light[2] - ring_dark[2]) * ring * 0.35),
            )
            if soft_noise:
                hs = _h_soft(x, y, 1)
                c = (
                    max(0, min(255, c[0] + hs[0])),
                    max(0, min(255, c[1] + hs[1])),
                    max(0, min(255, c[2] + hs[2])),
                )
            else:
                h = _h(x, y, 1)
                c = (
                    max(0, min(255, c[0] + h - 8)),
                    max(0, min(255, c[1] + h - 8)),
                    max(0, min(255, c[2] + h - 8)),
                )
            row.append(c)
        rows.append(row)
    return rows


def log_side_cylindrical(
    edge_dark: tuple[int, int, int],
    mid: tuple[int, int, int],
    highlight: tuple[int, int, int],
    seed: int,
    soft_noise: bool = False,
) -> list[list[tuple[int, int, int]]]:
    """Vertical cylinder fake: darker at UV edges, highlight strip + gentle streaks."""
    rows: list[list[tuple[int, int, int]]] = []
    cx = 7.5
    for y in range(16):
        row = []
        for x in range(16):
            d = abs(x - cx) / 7.5
            cyl = 1.0 - math.pow(min(1.0, d), 1.35)
            vert = 0.95 + 0.05 * math.sin(y * 0.48 + seed * 0.007)
            streak = 1.0 + 0.045 * math.sin((x * 1.9 + y * 3.1 + seed) * 0.28)
            t = max(0.0, min(1.0, cyl * vert * streak))
            if t < 0.5:
                u = t * 2.0
                c = (
                    int(edge_dark[0] + (mid[0] - edge_dark[0]) * u),
                    int(edge_dark[1] + (mid[1] - edge_dark[1]) * u),
                    int(edge_dark[2] + (mid[2] - edge_dark[2]) * u),
                )
            else:
                u = (t - 0.5) * 2.0
                c = (
                    int(mid[0] + (highlight[0] - mid[0]) * u),
                    int(mid[1] + (highlight[1] - mid[1]) * u),
                    int(mid[2] + (highlight[2] - mid[2]) * u),
                )
            if soft_noise:
                hs = _h_soft(x, y, seed)
                c = (
                    max(0, min(255, c[0] + hs[0])),
                    max(0, min(255, c[1] + hs[1])),
                    max(0, min(255, c[2] + hs[2])),
                )
            else:
                n = _h(x, y, seed)
                c = (
                    max(0, min(255, c[0] + n - 8)),
                    max(0, min(255, c[1] + n - 8)),
                    max(0, min(255, c[2] + n - 8)),
                )
            row.append(c)
        rows.append(row)
    return rows


def leaves_blob_rgba(
    deep: tuple[int, int, int],
    mid: tuple[int, int, int],
    hi: tuple[int, int, int],
    centers: list[tuple[float, float, float]],
    seed: int,
) -> tuple[list[list[tuple[int, int, int]]], list[list[int]]]:
    """
    Overlapping foliage blobs + elliptical alpha for soft billboard silhouettes.
    """
    rgb_rows: list[list[tuple[int, int, int]]] = []
    alpha_rows: list[list[int]] = []
    tcx, tcy = 7.5, 7.5
    for y in range(16):
        rrow: list[tuple[int, int, int]] = []
        arow: list[int] = []
        for x in range(16):
            fx, fy = float(x) + 0.5, float(y) + 0.5
            blob = 0.0
            for cx, cy, rad in centers:
                dx, dy = fx - cx, fy - cy
                d = math.sqrt(dx * dx + dy * dy) / rad
                blob = max(blob, max(0.0, 1.0 - d * d))
            corner = math.sqrt(min(fx, 16.0 - fx) * min(fy, 16.0 - fy)) / 8.0
            vignette = 0.74 + 0.26 * min(1.0, corner)
            t = blob * vignette
            dx, dy = fx - tcx, fy - tcy
            rim = math.sqrt(dx * dx + dy * dy) / 7.85
            # Soften outer tile edge (round clump when alpha-tested on billboards).
            edge_f = max(0.0, 1.0 - rim * rim)
            a = max(0.0, min(1.0, t * (0.88 + 0.12 * edge_f)))
            hs = _h_soft(x, y, seed)
            c = (
                int(deep[0] + (mid[0] - deep[0]) * t + (hi[0] - mid[0]) * t * t + hs[0]),
                int(deep[1] + (mid[1] - deep[1]) * t + (hi[1] - mid[1]) * t * t + hs[1]),
                int(deep[2] + (mid[2] - deep[2]) * t + (hi[2] - mid[2]) * t * t + hs[2]),
            )
            c = (
                max(0, min(255, c[0])),
                max(0, min(255, c[1])),
                max(0, min(255, c[2])),
            )
            rrow.append(c)
            arow.append(int(max(0, min(255, a * 255.0))))
        rgb_rows.append(rrow)
        alpha_rows.append(arow)
    return rgb_rows, alpha_rows


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    tmp_rgb = OUT / ".tree_gen_rgb.ppm"
    tmp_a = OUT / ".tree_gen_a.pgm"

    # v4 logs: softer grain (still voxel cubes; reads as rounded wood).
    log_specs: list[tuple[str, list[list[tuple[int, int, int]]]]] = [
        (
            "oak_log_top_v4.png",
            log_top_rounded((128, 102, 72), (88, 68, 48), (52, 40, 30), soft_noise=True),
        ),
        (
            "oak_log_v4.png",
            log_side_cylindrical((44, 34, 24), (86, 68, 48), (118, 92, 62), 0x4A01, soft_noise=True),
        ),
        (
            "jungle_log_top_v4.png",
            log_top_rounded((118, 88, 58), (82, 58, 40), (48, 36, 26), soft_noise=True),
        ),
        (
            "jungle_log_v4.png",
            log_side_cylindrical((48, 34, 24), (92, 62, 44), (124, 88, 58), 0x6C03, soft_noise=True),
        ),
        (
            "spruce_log_top_v4.png",
            log_top_rounded((118, 112, 108), (88, 84, 82), (54, 52, 50), soft_noise=True),
        ),
        (
            "spruce_log_v4.png",
            log_side_cylindrical((46, 44, 42), (82, 76, 72), (108, 102, 98), 0x8E05, soft_noise=True),
        ),
    ]
    for name, rows in log_specs:
        write_ppm(tmp_rgb, rows)
        ppm_to_png(tmp_rgb, OUT / name)
        print("Wrote", OUT / name)

    # v5 leaves: RGBA — tuned for ChunkMesher crossed quads + fs_chunk alpha cutout.
    leaf_jobs: list[tuple[str, tuple, tuple, tuple, list[tuple[float, float, float]], int]] = [
        (
            "oak_leaves_v5.png",
            (34, 72, 38),
            (52, 118, 64),
            (88, 168, 96),
            [(4.2, 5.1, 5.5), (11.0, 4.5, 5.2), (7.5, 11.0, 6.0), (2.0, 11.5, 4.8)],
            0x5B02,
        ),
        (
            "jungle_leaves_v5.png",
            (22, 88, 72),
            (38, 142, 118),
            (62, 195, 168),
            [(5.5, 6.0, 6.2), (10.5, 5.0, 5.8), (7.0, 10.5, 5.5), (3.0, 3.5, 5.0)],
            0x7D04,
        ),
        (
            "spruce_leaves_v5.png",
            (48, 78, 62),
            (72, 118, 98),
            (118, 168, 148),
            [(5.0, 5.5, 6.0), (11.0, 6.0, 5.5), (8.0, 11.0, 5.8), (2.5, 10.0, 4.5)],
            0x9F06,
        ),
    ]
    for name, deep, mid, hi, centers, sid in leaf_jobs:
        rgb, alpha = leaves_blob_rgba(deep, mid, hi, centers, sid)
        write_ppm(tmp_rgb, rgb)
        write_pgm_gray(tmp_a, alpha)
        rgb_and_alpha_to_png(tmp_rgb, tmp_a, OUT / name)
        print("Wrote", OUT / name)

    # Legacy v3 outputs (no alpha) kept for atlas fallback if v4/v5 missing.
    legacy: list[tuple[str, list[list[tuple[int, int, int]]]]] = [
        ("oak_log_top_v3.png", log_top_rounded((128, 102, 72), (88, 68, 48), (52, 40, 30), soft_noise=False)),
        ("oak_log_v3.png", log_side_cylindrical((44, 34, 24), (86, 68, 48), (118, 92, 62), 0x4A01, soft_noise=False)),
        ("jungle_log_top_v3.png", log_top_rounded((118, 88, 58), (82, 58, 40), (48, 36, 26), soft_noise=False)),
        ("jungle_log_v3.png", log_side_cylindrical((48, 34, 24), (92, 62, 44), (124, 88, 58), 0x6C03, soft_noise=False)),
        ("spruce_log_top_v3.png", log_top_rounded((118, 112, 108), (88, 84, 82), (54, 52, 50), soft_noise=False)),
        ("spruce_log_v3.png", log_side_cylindrical((46, 44, 42), (82, 76, 72), (108, 102, 98), 0x8E05, soft_noise=False)),
    ]
    for name, rows in legacy:
        write_ppm(tmp_rgb, rows)
        ppm_to_png(tmp_rgb, OUT / name)
        print("Wrote", OUT / name)

    return 0


if __name__ == "__main__":
    sys.exit(main())
