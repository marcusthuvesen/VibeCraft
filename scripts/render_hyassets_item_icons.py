#!/usr/bin/env python3
from __future__ import annotations

import math
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
HY_JSON = ROOT / "third_party" / "HyAssets" / "itemmodels" / "JSON"
HY_OBJ = ROOT / "third_party" / "HyAssets" / "itemmodels" / "OBJ"
OUT = ROOT / "assets" / "textures" / "item"


@dataclass(frozen=True)
class VariantSpec:
    name: str
    tint: str
    colorize: int
    brightness: int = 100
    saturation: int = 100


def ensure_magick() -> str:
    executable = shutil.which("magick")
    if not executable:
        raise RuntimeError("ImageMagick 'magick' executable not found")
    return executable


def magick(*args: str, input_bytes: bytes | None = None) -> bytes:
    executable = ensure_magick()
    completed = subprocess.run(
        [executable, *args],
        input=input_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    return completed.stdout


def image_size(path: Path) -> tuple[int, int]:
    output = magick("identify", "-format", "%w %h", str(path)).decode("utf-8").strip()
    width_text, height_text = output.split()
    return int(width_text), int(height_text)


def load_rgba(path: Path) -> np.ndarray:
    width, height = image_size(path)
    raw = magick(str(path), "-alpha", "on", "-depth", "8", "rgba:-")
    return np.frombuffer(raw, dtype=np.uint8).reshape((height, width, 4))


def save_rgba(path: Path, rgba: np.ndarray) -> None:
    rgba = np.ascontiguousarray(rgba.astype(np.uint8))
    height, width, _ = rgba.shape
    magick("-size", f"{width}x{height}", "-depth", "8", "rgba:-", str(path), input_bytes=rgba.tobytes())


def finalize_icon(path: Path) -> None:
    magick(
        str(path),
        "-trim",
        "+repage",
        "-filter",
        "lanczos",
        "-resize",
        "30x30",
        "-gravity",
        "center",
        "-background",
        "none",
        "-extent",
        "32x32",
        str(path),
    )


def save_variant(source: Path, target: Path, spec: VariantSpec) -> None:
    magick(
        str(source),
        "-alpha",
        "on",
        "-channel",
        "rgb",
        "-fill",
        spec.tint,
        "-colorize",
        f"{spec.colorize}",
        "-modulate",
        f"{spec.brightness},{spec.saturation},100",
        str(target),
    )
    finalize_icon(target)


def parse_obj(path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    vertices: list[list[float]] = []
    uvs: list[list[float]] = []
    triangles: list[tuple[int, int, int, int, int, int]] = []

    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        parts = stripped.split()
        if parts[0] == "v":
            vertices.append([float(parts[1]), float(parts[2]), float(parts[3])])
            continue
        if parts[0] == "vt":
            uvs.append([float(parts[1]), float(parts[2])])
            continue
        if parts[0] != "f":
            continue
        face_entries = [entry.split("/") for entry in parts[1:]]
        for index in range(1, len(face_entries) - 1):
            tri = (face_entries[0], face_entries[index], face_entries[index + 1])
            packed: list[int] = []
            for corner in tri:
                packed.append(int(corner[0]) - 1)
                packed.append(int(corner[1]) - 1)
            triangles.append(tuple(packed))

    return (
        np.asarray(vertices, dtype=np.float32),
        np.asarray(uvs, dtype=np.float32),
        np.asarray(triangles, dtype=np.int32),
    )


def rotation_matrix(rx_deg: float, ry_deg: float, rz_deg: float) -> np.ndarray:
    rx = math.radians(rx_deg)
    ry = math.radians(ry_deg)
    rz = math.radians(rz_deg)

    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)

    mx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]], dtype=np.float32)
    my = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]], dtype=np.float32)
    mz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]], dtype=np.float32)
    return mz @ my @ mx


def sample_texture(texture: np.ndarray, uv: np.ndarray) -> np.ndarray:
    height, width, _ = texture.shape
    u = float(np.clip(uv[0], 0.0, 1.0))
    v = float(np.clip(1.0 - uv[1], 0.0, 1.0))
    x = min(width - 1, max(0, int(round(u * (width - 1)))))
    y = min(height - 1, max(0, int(round(v * (height - 1)))))
    return texture[y, x].astype(np.float32) / 255.0


def render_icon(
    obj_path: Path,
    texture_path: Path,
    output_path: Path,
    *,
    size: int = 96,
    rotate_x: float = -18.0,
    rotate_y: float = -42.0,
    rotate_z: float = 18.0,
) -> None:
    vertices, texcoords, triangles = parse_obj(obj_path)
    texture = load_rgba(texture_path)
    rotation = rotation_matrix(rotate_x, rotate_y, rotate_z)

    center = (vertices.min(axis=0) + vertices.max(axis=0)) * 0.5
    rotated = (vertices - center) @ rotation.T
    mins = rotated.min(axis=0)
    maxs = rotated.max(axis=0)
    extent = max(maxs[0] - mins[0], maxs[1] - mins[1], 1e-5)
    scale = (size * 0.72) / extent

    screen = np.zeros((len(vertices), 3), dtype=np.float32)
    screen[:, 0] = (rotated[:, 0] - (mins[0] + maxs[0]) * 0.5) * scale + size * 0.5
    screen[:, 1] = size * 0.60 - (rotated[:, 1] - mins[1]) * scale
    screen[:, 2] = rotated[:, 2]

    canvas = np.zeros((size, size, 4), dtype=np.float32)
    zbuffer = np.full((size, size), -1e9, dtype=np.float32)
    light_dir = np.array([0.45, 0.85, 0.30], dtype=np.float32)
    light_dir /= np.linalg.norm(light_dir)

    for triangle in triangles:
        vertex_indices = [triangle[0], triangle[2], triangle[4]]
        uv_indices = [triangle[1], triangle[3], triangle[5]]
        points = screen[vertex_indices]
        uv_points = texcoords[uv_indices]

        edge_a = points[1, :3] - points[0, :3]
        edge_b = points[2, :3] - points[0, :3]
        normal = np.cross(edge_a, edge_b)
        normal_len = np.linalg.norm(normal)
        if normal_len < 1e-6:
            continue
        normal /= normal_len
        shade = float(np.clip(0.35 + 0.65 * np.dot(normal, light_dir), 0.28, 1.0))

        min_x = max(0, int(math.floor(np.min(points[:, 0]))))
        max_x = min(size - 1, int(math.ceil(np.max(points[:, 0]))))
        min_y = max(0, int(math.floor(np.min(points[:, 1]))))
        max_y = min(size - 1, int(math.ceil(np.max(points[:, 1]))))
        if min_x > max_x or min_y > max_y:
            continue

        p0 = points[0, :2]
        p1 = points[1, :2]
        p2 = points[2, :2]
        denom = (p1[1] - p2[1]) * (p0[0] - p2[0]) + (p2[0] - p1[0]) * (p0[1] - p2[1])
        if abs(denom) < 1e-6:
            continue

        for py in range(min_y, max_y + 1):
            for px in range(min_x, max_x + 1):
                sample_point = np.array([px + 0.5, py + 0.5], dtype=np.float32)
                w0 = ((p1[1] - p2[1]) * (sample_point[0] - p2[0]) + (p2[0] - p1[0]) * (sample_point[1] - p2[1])) / denom
                w1 = ((p2[1] - p0[1]) * (sample_point[0] - p2[0]) + (p0[0] - p2[0]) * (sample_point[1] - p2[1])) / denom
                w2 = 1.0 - w0 - w1
                if w0 < 0.0 or w1 < 0.0 or w2 < 0.0:
                    continue

                depth = w0 * points[0, 2] + w1 * points[1, 2] + w2 * points[2, 2]
                if depth <= zbuffer[py, px]:
                    continue

                uv = w0 * uv_points[0] + w1 * uv_points[1] + w2 * uv_points[2]
                texel = sample_texture(texture, uv)
                if texel[3] <= 0.02:
                    continue

                color = texel.copy()
                color[:3] *= shade
                color[3] *= 0.98
                canvas[py, px] = color
                zbuffer[py, px] = depth

    rgba = np.clip(np.round(canvas * 255.0), 0, 255).astype(np.uint8)
    save_rgba(output_path, rgba)
    finalize_icon(output_path)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    iron_texture = HY_JSON / "iron_axe.png"
    render_icon(HY_OBJ / "iron_pickaxe.obj", iron_texture, OUT / "iron_pickaxe.png", rotate_x=-20.0, rotate_y=-48.0, rotate_z=20.0)
    render_icon(HY_OBJ / "iron_axe.obj", iron_texture, OUT / "iron_axe.png", rotate_x=-16.0, rotate_y=-36.0, rotate_z=12.0)

    pickaxe_variants = [
        VariantSpec("wooden_pickaxe.png", "#8b6038", 70, brightness=96, saturation=92),
        VariantSpec("stone_pickaxe.png", "#8e949d", 68, brightness=92, saturation=18),
        VariantSpec("golden_pickaxe.png", "#f0c448", 74, brightness=110, saturation=135),
        VariantSpec("diamond_pickaxe.png", "#6ed6d4", 72, brightness=108, saturation=122),
    ]
    axe_variants = [
        VariantSpec("wooden_axe.png", "#8b6038", 70, brightness=96, saturation=92),
        VariantSpec("stone_axe.png", "#8e949d", 68, brightness=92, saturation=18),
        VariantSpec("golden_axe.png", "#f0c448", 74, brightness=110, saturation=135),
        VariantSpec("diamond_axe.png", "#6ed6d4", 72, brightness=108, saturation=122),
    ]

    for variant in pickaxe_variants:
        save_variant(OUT / "iron_pickaxe.png", OUT / variant.name, variant)
    for variant in axe_variants:
        save_variant(OUT / "iron_axe.png", OUT / variant.name, variant)


if __name__ == "__main__":
    main()
