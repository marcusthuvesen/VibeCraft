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

[[nodiscard]] double positiveBand(const double value, const double threshold, const double width)
{
    if (value <= threshold)
    {
        return 0.0;
    }
    return std::clamp((value - threshold) / width, 0.0, 1.0);
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
    const double hillNoise = std::clamp(
        noise::fbmNoise2d(wx + 137.0, wz - 121.0, 188.0, 3, mixedSeed(0x4d5e6f70U, worldSeed)) * 0.5 + 0.5,
        0.0,
        1.0);
    const double mountainNoise = std::clamp(
        noise::ridgeNoise2d(wx - 87.0, wz + 119.0, 118.0, mixedSeed(0x5e6f7081U, worldSeed)),
        0.0,
        1.0);

    const double knollLift = positiveBand(knollNoise, 0.42, 0.42);
    const double shoulderLift = positiveBand(shoulderNoise, 0.58, 0.28);
    const double hillLift = positiveBand(hillNoise, 0.54, 0.30);
    const double mountainLift = positiveBand(mountainNoise, 0.46, 0.32);
    const double hillMask = std::clamp(variation.hilliness * 1.12 - variation.canopyDensity * 0.12, 0.0, 1.0);
    const double mountainMask = std::clamp(variation.mountainness * 1.10 + variation.roughness * 0.20, 0.0, 1.0);

    double heightDelta = knollLift * knollLift * (variation.roughness * 1.35 + 0.16);
    heightDelta += shoulderLift * shoulderLift * (variation.roughness * 0.72 + variation.canopyDensity * 0.18 + 0.06);
    heightDelta += hillLift * hillLift * hillMask * 1.65;
    heightDelta += mountainLift * mountainLift * mountainMask * 2.05;
    heightDelta -= std::max(0.0, 0.58 - hollowNoise) * (variation.lushness * 0.95 + 0.12);
    heightDelta -= std::max(0.0, variation.dryness - 0.62) * std::max(0.0, 0.56 - variation.canopyDensity) * 1.8;

    if (variation.primaryVariant == WoodlandVariant::WoodedMountains)
    {
        heightDelta += mountainLift * mountainMask * 0.95 + shoulderLift * mountainMask * 0.38;
    }
    else if (variation.primaryVariant == WoodlandVariant::WoodedHills)
    {
        heightDelta += hillLift * hillMask * 0.70 + mountainLift * hillMask * 0.16;
    }
    else if (variation.primaryVariant == WoodlandVariant::RockyRise)
    {
        heightDelta += knollLift * 0.42;
    }
    else if (variation.primaryVariant == WoodlandVariant::DryClearing
             || variation.primaryVariant == WoodlandVariant::MossyHollow)
    {
        heightDelta -= 0.45;
    }

    return std::clamp(heightDelta, -2.0, 3.0);
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
    const bool hillSlopeArea = surfaceHeight >= 84 || maxNeighborSurfaceDelta >= 7;
    const bool mountainSlopeArea = surfaceHeight >= 92 || maxNeighborSurfaceDelta >= 9;
    if (variation.primaryVariant == WoodlandVariant::WoodedMountains
        && variation.mountainness > 0.74
        && mountainSlopeArea)
    {
        decision.surfaceBlock = BlockType::Stone;
        decision.subsurfaceBlock = BlockType::Stone;
        decision.topsoilDepthDelta = -2;
        return decision;
    }

    if (variation.primaryVariant == WoodlandVariant::WoodedHills
        && variation.hilliness > 0.68
        && hillSlopeArea)
    {
        decision.surfaceBlock = BlockType::Stone;
        decision.subsurfaceBlock = BlockType::Stone;
        decision.topsoilDepthDelta = -1;
        return decision;
    }

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
