#pragma once

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <cstdint>

namespace vibecraft::world
{
/// Temperate woodlands / taiga: shared ravine-line noise used for height dips and optional air carving.
[[nodiscard]] bool isWoodlandRavineBiome(SurfaceBiome biome);

[[nodiscard]] double woodlandRavineBiomeScale(SurfaceBiome biome);

struct WoodlandRavineSample
{
    /// pow(inRavine, 1.35) — high along narrow crevasse centers.
    double ravineShape = 0.0;
    double regionalMask = 0.0;
    /// 0.5..1.0 depth multiplier (varies along the ravine path).
    double depthNoise = 0.5;
};

[[nodiscard]] WoodlandRavineSample sampleWoodlandRavine(int worldX, int worldZ, std::uint32_t worldSeed);

/// Height delta (can be negative) from micro-roughness minus ravine depth — same as previous woodland pass.
[[nodiscard]] double woodlandSurfaceHeightDelta(SurfaceBiome biome, int worldX, int worldZ, std::uint32_t worldSeed);
}  // namespace vibecraft::world
