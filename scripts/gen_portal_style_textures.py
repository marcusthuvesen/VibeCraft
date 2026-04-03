#!/usr/bin/env python3
"""Emit 16x16 Portal-style material PNGs (oxygen relay faces, torch) for chunk_atlas."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "textures" / "materials"


def write_ppm(path: Path, rows: list[list[tuple[int, int, int]]]) -> None:
    h = len(rows)
    w = len(rows[0])
    for i, r in enumerate(rows):
        assert len(r) == w, (i, len(r), w)
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


# Palette keys → RGB (warm teal metal + copper + biolum moss + lumen top)
def relay_bottom() -> list[list[tuple[int, int, int]]]:
    # k base slate, K panel, L highlight, c copper rivet, v vent slot, w grate
    P = {
        "k": (38, 46, 54),
        "K": (52, 62, 72),
        "L": (68, 80, 92),
        "c": (168, 118, 72),
        "v": (28, 34, 40),
        "w": (44, 54, 62),
        "o": (58, 70, 78),
        ".": (46, 56, 64),
    }
    art = [
        "kkkkkkkkkkkkkkkk",
        "kKKKKKKKKKKKKKKk",
        "kKooooooooooooKk",
        "kKo.vvvvvvvv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vwwwwwwv.oKk",
        "kKo.vvvvvvvv.oKk",
        "kKooocLLccooKKkk",
        "kKKKKKKKKKKKKKKk",
        "kc...........ckk",
        "kkkkkkkkkkkkkkkk",
    ]
    return [[P[ch] for ch in row] for row in art]


def relay_side() -> list[list[tuple[int, int, int]]]:
    P = {
        "k": (44, 54, 58),
        "K": (56, 68, 72),
        "m": (52, 96, 64),
        "M": (42, 78, 52),
        "p": (78, 92, 88),
        "P": (58, 72, 68),
        "r": (62, 52, 44),
        "c": (150, 108, 68),
        "L": (72, 86, 90),
        "x": (36, 44, 48),
    }
    art = [
        "mmmmmmkkKKKKkkkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKpPPPKKkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKpPPPKKkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKpPPPKKkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKpPPPKKkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKpPPPKKkk",
        "mMMmmmkKppPPKKkk",
        "mmmmmmkKLLccKKkk",
        "mMMmmmkKKKKKKkkk",
        "mmmmmmkKrxxxKKkk",
        "mmmmmmkkkkkkkkkk",
    ]
    return [[P[ch] for ch in row] for row in art]


def relay_top() -> list[list[tuple[int, int, int]]]:
    P = {
        "k": (36, 44, 52),
        "K": (48, 58, 66),
        "h": (58, 120, 108),
        "H": (72, 168, 152),
        "g": (110, 214, 196),
        "G": (140, 232, 218),
        "e": (200, 248, 240),
        "n": (88, 140, 200),
        "o": (62, 74, 84),
        "l": (52, 64, 74),
    }
    art = [
        "kkkkkkkkkkkkkkkk",
        "kKKooooooooKKkkk",
        "kKolhhhhhhloKkkk",
        "kKoHHHggGGHHoKkk",
        "kKohHggeeGgHokkk",
        "kKoHHggGGggHoKkk",
        "kKohHggGGgGHokkk",
        "kKoHHggGGggHoKkk",
        "kKohHggGGgGHokkk",
        "kKoHHggGGggHoKkk",
        "kKohHggeeGgHokkk",
        "kKoHHHggGGHHoKkk",
        "kKolhhhhhhloKkkk",
        "kKKooooooooKKkkk",
        "knnnnnnnnnnnnnnk",
        "kkkkkkkkkkkkkkkk",
    ]
    return [[P[ch] for ch in row] for row in art]


def torch_portal() -> list[list[tuple[int, int, int]]]:
    # Magenta key — decorative_cutout_tile in build_chunk_atlas.sh strips it to transparency.
    P = {
        "M": (255, 0, 255),
        "o": (218, 82, 36),
        "y": (255, 188, 72),
        "G": (255, 228, 140),
        "b": (88, 56, 34),
    }
    art = [
        "MMMMMMMMMMMMMMMM",
        "MMMMMMooMMMMMMMM",
        "MMMMMoyyyoMMMMMM",
        "MMMMoyyyyoMMMMMM",
        "MMMMoyGyGoMMMMMM",
        "MMMMMoyyoMMMMMMM",
        "MMMMMMooMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMbbMMMMMMMM",
        "MMMMMMMMMMMMMMMM",
    ]
    return [[P[ch] for ch in row] for row in art]


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    tmp = OUT_DIR / ".relay_gen.ppm"
    specs = [
        ("oxygen_relay_bottom.png", relay_bottom),
        ("oxygen_relay_side.png", relay_side),
        ("oxygen_relay_top.png", relay_top),
        ("torch_portal.png", torch_portal),
    ]
    for name, fn in specs:
        write_ppm(tmp, fn())
        ppm_to_png(tmp, OUT_DIR / name)
        print("Wrote", OUT_DIR / name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
