#pragma once

#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/TreeGenerationTuning.hpp"

namespace vibecraft::world::biomes
{
[[nodiscard]] float effectiveTreeSpawnChance(
    SurfaceBiome biome,
    const TreeBiomeSettings& baseSettings,
    const BiomeVariationSample& variation,
    int worldX,
    int worldZ);

[[nodiscard]] float softenTreeSpawnChanceForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    float spawnChance);

[[nodiscard]] TreeBiomeSettings treeVariantSettings(
    SurfaceBiome biome,
    const TreeBiomeSettings& baseSettings,
    const BiomeVariationSample& variation,
    int worldX,
    int worldZ);

[[nodiscard]] TreeBiomeSettings softenTreeSettingsForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    TreeBiomeSettings settings);

[[nodiscard]] float secondaryTreeChance(SurfaceBiome biome, const BiomeVariationSample& variation);

[[nodiscard]] float softenSecondaryTreeChanceForBiomeEdge(
    SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    float extraChance);
}  // namespace vibecraft::world::biomes
