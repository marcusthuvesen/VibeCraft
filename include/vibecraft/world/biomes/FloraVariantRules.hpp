#pragma once

#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/FloraGenerationTuning.hpp"

namespace vibecraft::world::biomes
{
[[nodiscard]] GrassTuftTuning applyGrassTuftVariant(
    SurfaceBiome biome,
    const BiomeVariationSample& variation,
    GrassTuftTuning tuning);

[[nodiscard]] FloraPatchTuning applyFloraPatchVariant(
    SurfaceBiome biome,
    const BiomeVariationSample& variation,
    FloraPatchTuning tuning);

[[nodiscard]] TemperateForestDecorProfile applyTemperateForestDecorVariant(
    SurfaceBiome biome,
    const BiomeVariationSample& variation,
    TemperateForestDecorProfile profile);

[[nodiscard]] GrassTuftTuning softenGrassTuftForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    GrassTuftTuning tuning);

[[nodiscard]] FloraPatchTuning softenFloraPatchForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    FloraPatchTuning tuning);

[[nodiscard]] TemperateForestDecorProfile softenTemperateForestDecorForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    TemperateForestDecorProfile profile);

[[nodiscard]] float softenSpecialFloraChanceForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    float chance);

[[nodiscard]] bool isTemperateForestDecorSurface(BlockType blockType);
}  // namespace vibecraft::world::biomes
