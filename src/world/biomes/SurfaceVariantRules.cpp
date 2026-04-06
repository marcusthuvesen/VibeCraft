#include "vibecraft/world/biomes/SurfaceVariantRules.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
[[nodiscard]] constexpr std::uint32_t mixedSeed(const std::uint32_t baseSeed, const std::uint32_t worldSeed)
{
    std::uint32_t mixed = baseSeed ^ (worldSeed + 0x9e3779b9U + (baseSeed << 6U) + (baseSeed >> 2U));
    mixed ^= mixed >> 16U;
    mixed *= 0x7feb352dU;
    mixed ^= mixed >> 15U;
    mixed *= 0x846ca68bU;
    mixed ^= mixed >> 16U;
    return mixed;
}

[[nodiscard]] bool isWoodlandFloorBlock(const BlockType blockType)
{
    return blockType == BlockType::Grass || blockType == BlockType::SnowGrass;
}
}  // namespace

double localSurfaceHeightDelta(
    const SurfaceBiome biome,
    const BiomeVariationSample& variation,
    const int worldX,
    const int worldZ,
    const std::uint32_t worldSeed)
{
    if (!supportsWoodlandVariation(biome))
    {
        return 0.0;
    }

    const double wx = static_cast<double>(worldX);
    const double wz = static_cast<double>(worldZ);
    const double knollNoise = std::clamp(
        noise::ridgeNoise2d(wx + 17.0, wz - 33.0, 58.0, mixedSeed(0x1a2b3c4dU, worldSeed)),
        0.0,
        1.0);
    const double shoulderNoise = std::clamp(
        noise::fbmNoise2d(wx + 91.0, wz - 67.0, 148.0, 2, mixedSeed(0x3c4d5e6fU, worldSeed)) * 0.5 + 0.5,
        0.0,
        1.0);
    const double hollowNoise = std::clamp(
        noise::fbmNoise2d(wx - 49.0, wz + 27.0, 96.0, 2, mixedSeed(0x2b3c4d5eU, worldSeed)) * 0.5 + 0.5,
        0.0,
        1.0);

    double heightDelta = std::max(0.0, knollNoise - 0.42) * (variation.roughness * 2.6 + 0.30);
    heightDelta += std::max(0.0, shoulderNoise - 0.58) * (variation.roughness * 1.35 + variation.canopyDensity * 0.40 + 0.10);
    heightDelta -= std::max(0.0, 0.60 - hollowNoise) * (variation.lushness * 1.8 + 0.20);
    heightDelta -= std::max(0.0, variation.dryness - 0.62) * std::max(0.0, 0.56 - variation.canopyDensity) * 1.8;

    if (variation.primaryVariant == WoodlandVariant::RockyRise)
    {
        heightDelta += 1.15;
    }
    else if (variation.primaryVariant == WoodlandVariant::DryClearing
             || variation.primaryVariant == WoodlandVariant::MossyHollow)
    {
        heightDelta -= 0.75;
    }

    return std::clamp(heightDelta, -3.0, 3.0);
}

SurfaceVariantDecision evaluateSurfaceVariantRules(
    const SurfaceBiome biome,
    const BiomeVariationSample& variation,
    const int surfaceHeight,
    const int maxNeighborSurfaceDelta,
    const double moisturePocket,
    const BlockType baseSurfaceBlock,
    const BlockType baseSubsurfaceBlock)
{
    if (!supportsWoodlandVariation(biome)
        || !isWoodlandFloorBlock(baseSurfaceBlock)
        || surfaceHeight >= 110
        || maxNeighborSurfaceDelta >= 12)
    {
        return SurfaceVariantDecision{
            .subsurfaceBlock = baseSubsurfaceBlock,
        };
    }

    SurfaceVariantDecision decision{
        .subsurfaceBlock = baseSubsurfaceBlock,
    };

    const bool elevatedRockyArea = surfaceHeight >= 92;
    const bool steepRockyArea = maxNeighborSurfaceDelta >= 8;
    if (variation.primaryVariant == WoodlandVariant::RockyRise
        && variation.roughness > 0.84
        && (elevatedRockyArea || steepRockyArea))
    {
        decision.surfaceBlock = BlockType::Stone;
        decision.subsurfaceBlock = BlockType::Stone;
        decision.topsoilDepthDelta = -1;
        return decision;
    }

    if (variation.primaryVariant == WoodlandVariant::MossyHollow
        && variation.lushness > 0.68
        && (moisturePocket > 0.16 || variation.canopyDensity > 0.64))
    {
        decision.surfaceBlock = BlockType::MossBlock;
        decision.topsoilDepthDelta = 1;
        return decision;
    }

    return decision;
}

void softenSurfaceVariantForBiomeEdge(
    const BiomeProfile& profile,
    const BiomeTransitionSample& transition,
    BlockType& surfaceBlockType,
    BlockType& subsurfaceBlockType,
    SurfaceVariantDecision& surfaceVariant)
{
    if (transition.neighboringBiome == profile.biome || transition.edgeStrength <= 0.26f)
    {
        return;
    }

    const float edgeBlend = std::clamp((transition.edgeStrength - 0.26f) / 0.74f, 0.0f, 1.0f);
    if ((surfaceBlockType == BlockType::MossBlock
            || surfaceBlockType == BlockType::Podzol
            || surfaceBlockType == BlockType::CoarseDirt)
        && !profile.usesSandStrata
        && edgeBlend > 0.34f)
    {
        surfaceBlockType = profile.canonicalSurfaceBlock;
        subsurfaceBlockType = profile.canonicalSubsurfaceBlock;
        surfaceVariant.topsoilDepthDelta = 0;
    }
}
}  // namespace vibecraft::world::biomes
