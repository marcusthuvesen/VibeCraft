#include "vibecraft/world/biomes/FloraVariantRules.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
[[nodiscard]] float edgeBlendFactor(const BiomeTransitionSample& transition)
{
    if (transition.edgeStrength <= 0.22f)
    {
        return 0.0f;
    }
    return std::clamp((transition.edgeStrength - 0.22f) / 0.78f, 0.0f, 1.0f);
}
}  // namespace

GrassTuftTuning applyGrassTuftVariant(
    const SurfaceBiome biome,
    const BiomeVariationSample& variation,
    GrassTuftTuning tuning)
{
    if (!supportsWoodlandVariation(biome))
    {
        return tuning;
    }

    tuning.baseChance *= 0.80 + variation.lushness * 0.32 - variation.dryness * 0.24;
    switch (variation.primaryVariant)
    {
    case WoodlandVariant::FernGrove:
        tuning.baseChance *= 1.25;
        tuning.primaryTuft = BlockType::CloverTuft;
        tuning.secondaryTuft = BlockType::GrassTuft;
        tuning.primaryFraction = 0.66;
        break;
    case WoodlandVariant::MossyHollow:
        tuning.baseChance *= 0.82;
        tuning.primaryTuft = BlockType::CloverTuft;
        tuning.secondaryTuft = BlockType::SparseTuft;
        tuning.primaryFraction = 0.72;
        break;
    case WoodlandVariant::DryClearing:
        tuning.baseChance *= 0.42;
        tuning.secondaryTuft = BlockType::FlowerTuft;
        tuning.primaryFraction = 0.74;
        break;
    case WoodlandVariant::RockyRise:
        tuning.baseChance *= 0.34;
        tuning.primaryTuft = BlockType::SparseTuft;
        tuning.secondaryTuft = BlockType::SparseTuft;
        tuning.primaryFraction = 1.0;
        break;
    case WoodlandVariant::WoodedHills:
        tuning.baseChance *= 0.56;
        tuning.primaryTuft = BlockType::SparseTuft;
        tuning.secondaryTuft = BlockType::GrassTuft;
        tuning.primaryFraction = 0.68;
        break;
    case WoodlandVariant::WoodedMountains:
        tuning.baseChance *= 0.24;
        tuning.primaryTuft = BlockType::SparseTuft;
        tuning.secondaryTuft = BlockType::SparseTuft;
        tuning.primaryFraction = 1.0;
        break;
    case WoodlandVariant::BirchPocket:
        tuning.baseChance *= 0.88;
        tuning.secondaryTuft = BlockType::FlowerTuft;
        tuning.primaryFraction = 0.70;
        break;
    default:
        break;
    }

    tuning.baseChance = std::max(0.0, tuning.baseChance);
    return tuning;
}

GrassTuftTuning softenGrassTuftForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    GrassTuftTuning tuning)
{
    if (transition.neighboringBiome == biome)
    {
        return tuning;
    }

    tuning.baseChance *= 1.0f - edgeBlendFactor(transition) * 0.38f;
    return tuning;
}

FloraPatchTuning applyFloraPatchVariant(
    const SurfaceBiome biome,
    const BiomeVariationSample& variation,
    FloraPatchTuning tuning)
{
    if (!supportsWoodlandVariation(biome))
    {
        return tuning;
    }

    switch (variation.primaryVariant)
    {
    case WoodlandVariant::FernGrove:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + 0.05, 0.0, 1.0);
        tuning.flowerSpotChance *= 0.74;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin - 0.04, 0.0, 1.0);
        tuning.mushroomSpotChance *= 1.10;
        break;
    case WoodlandVariant::MossyHollow:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + 0.12, 0.0, 1.0);
        tuning.flowerSpotChance *= 0.52;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin - 0.10, 0.0, 1.0);
        tuning.mushroomSpotChance *= 1.45;
        break;
    case WoodlandVariant::DryClearing:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin - 0.08, 0.0, 1.0);
        tuning.flowerSpotChance *= 1.55;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin + 0.14, 0.0, 1.0);
        tuning.mushroomSpotChance *= 0.38;
        break;
    case WoodlandVariant::BirchPocket:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin - 0.05, 0.0, 1.0);
        tuning.flowerSpotChance *= 1.22;
        tuning.mushroomSpotChance *= 0.85;
        break;
    case WoodlandVariant::RockyRise:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + 0.08, 0.0, 1.0);
        tuning.flowerSpotChance *= 0.55;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin + 0.06, 0.0, 1.0);
        tuning.mushroomSpotChance *= 0.62;
        break;
    case WoodlandVariant::WoodedHills:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + 0.04, 0.0, 1.0);
        tuning.flowerSpotChance *= 0.72;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin + 0.05, 0.0, 1.0);
        tuning.mushroomSpotChance *= 0.76;
        break;
    case WoodlandVariant::WoodedMountains:
        tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + 0.10, 0.0, 1.0);
        tuning.flowerSpotChance *= 0.34;
        tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin + 0.10, 0.0, 1.0);
        tuning.mushroomSpotChance *= 0.30;
        break;
    default:
        break;
    }

    return tuning;
}

FloraPatchTuning softenFloraPatchForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    FloraPatchTuning tuning)
{
    if (transition.neighboringBiome == biome)
    {
        return tuning;
    }

    const float edgeBlend = edgeBlendFactor(transition);
    tuning.flowerPatchMin = std::clamp(tuning.flowerPatchMin + edgeBlend * 0.08f, 0.0, 1.0);
    tuning.flowerSpotChance *= 1.0f - edgeBlend * 0.24f;
    tuning.mushroomPatchMin = std::clamp(tuning.mushroomPatchMin + edgeBlend * 0.06f, 0.0, 1.0);
    tuning.mushroomSpotChance *= 1.0f - edgeBlend * 0.28f;
    return tuning;
}

TemperateForestDecorProfile applyTemperateForestDecorVariant(
    const SurfaceBiome biome,
    const BiomeVariationSample& variation,
    TemperateForestDecorProfile profile)
{
    if (!supportsWoodlandVariation(biome))
    {
        return profile;
    }

    switch (variation.primaryVariant)
    {
    case WoodlandVariant::FernGrove:
        profile.patchEnterThreshold = std::max(0.24, profile.patchEnterThreshold - 0.06);
        profile.fernForestPatchMin = std::max(0.32, profile.fernForestPatchMin - 0.10);
        profile.fernChanceDense *= 1.45;
        profile.fernChanceSparse *= 1.38;
        profile.mossGroundRollOther *= 1.10;
        break;
    case WoodlandVariant::MossyHollow:
        profile.patchEnterThreshold = std::max(0.22, profile.patchEnterThreshold - 0.08);
        profile.mossWetnessMin = std::max(0.42, profile.mossWetnessMin - 0.16);
        profile.mossGroundRollDark *= 1.55;
        profile.mossGroundRollOther *= 1.70;
        profile.mushroomWetnessMin = std::max(0.34, profile.mushroomWetnessMin - 0.10);
        profile.mushroomRollDark *= 1.55;
        profile.mushroomRollOther *= 1.65;
        break;
    case WoodlandVariant::DryClearing:
        profile.patchEnterThreshold = std::min(0.70, profile.patchEnterThreshold + 0.10);
        profile.denseForestThreshold = std::min(0.90, profile.denseForestThreshold + 0.10);
        profile.fernChanceDense *= 0.52;
        profile.fernChanceSparse *= 0.46;
        profile.mushroomRollDark *= 0.42;
        profile.mushroomRollOther *= 0.34;
        profile.woodlandGroundPatchEnabled = true;
        profile.woodlandGroundPatchRollMax *= 1.45;
        profile.woodlandGroundPatchWetnessPodzol = 0.72;
        break;
    case WoodlandVariant::BirchPocket:
        profile.patchEnterThreshold = std::max(0.22, profile.patchEnterThreshold - 0.02);
        profile.fernChanceDense *= 0.86;
        profile.fernChanceSparse *= 0.82;
        profile.woodlandGroundPatchEnabled = true;
        profile.woodlandGroundPatchRollMax *= 1.12;
        break;
    case WoodlandVariant::RockyRise:
        profile.patchEnterThreshold = std::min(0.80, profile.patchEnterThreshold + 0.12);
        profile.fernChanceDense *= 0.42;
        profile.fernChanceSparse *= 0.40;
        profile.woodlandGroundPatchEnabled = false;
        profile.mossyCobbleGroundRollMax *= 1.45;
        break;
    case WoodlandVariant::WoodedHills:
        profile.patchEnterThreshold = std::min(0.78, profile.patchEnterThreshold + 0.10);
        profile.denseForestThreshold = std::min(0.88, profile.denseForestThreshold + 0.06);
        profile.fernChanceDense *= 0.66;
        profile.fernChanceSparse *= 0.62;
        profile.woodlandGroundPatchRollMax *= 0.82;
        profile.mossyCobbleGroundRollMax *= 1.20;
        break;
    case WoodlandVariant::WoodedMountains:
        profile.patchEnterThreshold = std::min(0.86, profile.patchEnterThreshold + 0.18);
        profile.denseForestThreshold = std::min(0.92, profile.denseForestThreshold + 0.14);
        profile.fernChanceDense *= 0.34;
        profile.fernChanceSparse *= 0.30;
        profile.woodlandGroundPatchEnabled = false;
        profile.mossyCobbleGroundRollMax *= 1.65;
        break;
    default:
        break;
    }

    return profile;
}

TemperateForestDecorProfile softenTemperateForestDecorForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    TemperateForestDecorProfile profile)
{
    if (transition.neighboringBiome == biome)
    {
        return profile;
    }

    const float edgeBlend = edgeBlendFactor(transition);
    profile.patchEnterThreshold = std::min(0.90, profile.patchEnterThreshold + edgeBlend * 0.12f);
    profile.denseForestThreshold = std::min(0.92, profile.denseForestThreshold + edgeBlend * 0.10f);
    profile.mossGroundRollDark *= 1.0f - edgeBlend * 0.42f;
    profile.mossGroundRollOther *= 1.0f - edgeBlend * 0.42f;
    profile.mushroomRollDark *= 1.0f - edgeBlend * 0.36f;
    profile.mushroomRollOther *= 1.0f - edgeBlend * 0.36f;
    profile.fernChanceDense *= 1.0f - edgeBlend * 0.30f;
    profile.fernChanceSparse *= 1.0f - edgeBlend * 0.30f;
    profile.woodlandGroundPatchRollMax *= 1.0f - edgeBlend * 0.48f;
    profile.mossyCobbleGroundRollMax *= 1.0f - edgeBlend * 0.55f;
    return profile;
}

float softenSpecialFloraChanceForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    const float chance)
{
    if (transition.neighboringBiome == biome)
    {
        return chance;
    }
    return chance * (1.0f - edgeBlendFactor(transition) * 0.45f);
}

bool isTemperateForestDecorSurface(const BlockType blockType)
{
    return blockType == BlockType::Grass
        || blockType == BlockType::MossBlock
        || blockType == BlockType::Podzol
        || blockType == BlockType::CoarseDirt
        || blockType == BlockType::SnowGrass;
}
}  // namespace vibecraft::world::biomes
