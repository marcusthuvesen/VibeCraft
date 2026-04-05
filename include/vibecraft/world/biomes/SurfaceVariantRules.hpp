#pragma once

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"

#include <cstdint>
#include <optional>

namespace vibecraft::world::biomes
{
struct SurfaceVariantDecision
{
    std::optional<BlockType> surfaceBlock;
    std::optional<BlockType> subsurfaceBlock;
    int topsoilDepthDelta = 0;
};

[[nodiscard]] double localSurfaceHeightDelta(
    SurfaceBiome biome,
    const BiomeVariationSample& variation,
    int worldX,
    int worldZ,
    std::uint32_t worldSeed);

[[nodiscard]] SurfaceVariantDecision evaluateSurfaceVariantRules(
    SurfaceBiome biome,
    const BiomeVariationSample& variation,
    int surfaceHeight,
    int maxNeighborSurfaceDelta,
    double moisturePocket,
    BlockType baseSurfaceBlock,
    BlockType baseSubsurfaceBlock);

void softenSurfaceVariantForBiomeEdge(
    const BiomeProfile& profile,
    const BiomeTransitionSample& transition,
    BlockType& surfaceBlockType,
    BlockType& subsurfaceBlockType,
    SurfaceVariantDecision& surfaceVariant);
}  // namespace vibecraft::world::biomes
