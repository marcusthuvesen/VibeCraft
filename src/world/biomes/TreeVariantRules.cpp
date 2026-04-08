#include "vibecraft/world/biomes/TreeVariantRules.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
constexpr std::uint32_t kTreeShapeSeed = 0x72f1a4b3U;
constexpr std::uint32_t kTreeForestDensitySeed = 0x83c2b5d4U;

[[nodiscard]] float forestDensityAt(const SurfaceBiome biome, const int worldX, const int worldZ)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);

    switch (biomeProfile(biome).treeFamily)
    {
    case TreeGenerationFamily::Plains:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd + 31.0, worldZd - 17.0, 248.0, 3, kTreeForestDensitySeed) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::Forest:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd + 31.0, worldZd - 17.0, 188.0, 3, kTreeForestDensitySeed + 3U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::BirchForest:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd - 27.0, worldZd + 69.0, 180.0, 3, kTreeForestDensitySeed + 11U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::DarkForest:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd - 85.0, worldZd + 17.0, 132.0, 3, kTreeForestDensitySeed + 19U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::Taiga:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd - 59.0, worldZd + 43.0, 176.0, 3, kTreeForestDensitySeed + 17U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::SnowyTaiga:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd - 59.0, worldZd + 43.0, 196.0, 3, kTreeForestDensitySeed + 23U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::Jungle:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 160.0, 3, kTreeForestDensitySeed + 31U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::SparseJungle:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 220.0, 3, kTreeForestDensitySeed + 37U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::BambooJungle:
        return static_cast<float>(std::clamp(
            noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 132.0, 3, kTreeForestDensitySeed + 41U) * 0.5 + 0.5,
            0.0,
            1.0));
    case TreeGenerationFamily::None:
    default:
        return 0.0f;
    }
}

[[nodiscard]] float woodlandDensityMultiplier(const BiomeVariationSample& variation)
{
    float multiplier = static_cast<float>(
        0.65 + variation.canopyDensity * 0.75 + variation.lushness * 0.18 - variation.dryness * 0.30);
    switch (variation.primaryVariant)
    {
    case WoodlandVariant::DryClearing:
        multiplier *= 0.38f;
        break;
    case WoodlandVariant::BirchPocket:
        multiplier *= 1.06f;
        break;
    case WoodlandVariant::MossyHollow:
        multiplier *= 1.12f;
        break;
    case WoodlandVariant::RockyRise:
        multiplier *= 0.72f;
        break;
    case WoodlandVariant::WoodedHills:
        multiplier *= 0.82f;
        break;
    case WoodlandVariant::WoodedMountains:
        multiplier *= 0.52f;
        break;
    default:
        break;
    }
    return std::clamp(multiplier, 0.18f, 1.42f);
}
}  // namespace

float effectiveTreeSpawnChance(
    const SurfaceBiome biome,
    const TreeBiomeSettings& baseSettings,
    const BiomeVariationSample& variation,
    const int worldX,
    const int worldZ)
{
    float density = forestDensityAt(biome, worldX, worldZ);
    if (supportsWoodlandVariation(biome))
    {
        density = std::clamp(density * woodlandDensityMultiplier(variation), 0.0f, 1.0f);
    }

    switch (biomeProfile(biome).treeFamily)
    {
    case TreeGenerationFamily::Plains:
        return density < 0.16f ? std::clamp(density * 0.08f, 0.0f, 0.015f)
                               : std::clamp(baseSettings.spawnChance + (density - 0.16f) * 0.15f, 0.0f, 0.16f);
    case TreeGenerationFamily::Forest:
        return density < 0.12f ? std::clamp(density * 0.12f, 0.0f, 0.04f)
                               : std::clamp(baseSettings.spawnChance + (density - 0.12f) * 0.75f, 0.0f, 0.87f);
    case TreeGenerationFamily::BirchForest:
        return density < 0.12f ? std::clamp(density * 0.12f, 0.0f, 0.04f)
                               : std::clamp(baseSettings.spawnChance + (density - 0.12f) * 0.45f, 0.0f, 0.60f);
    case TreeGenerationFamily::DarkForest:
        return density < 0.08f ? std::clamp(density * 0.12f, 0.0f, 0.04f)
                               : std::clamp(baseSettings.spawnChance + density * 0.36f, 0.0f, 0.74f);
    case TreeGenerationFamily::Taiga:
        return density < 0.14f ? std::clamp(density * 0.10f, 0.0f, 0.03f)
                               : std::clamp(baseSettings.spawnChance + (density - 0.14f) * 0.34f, 0.0f, 0.48f);
    case TreeGenerationFamily::SnowyTaiga:
        return density < 0.16f ? std::clamp(density * 0.10f, 0.0f, 0.03f)
                               : std::clamp(baseSettings.spawnChance + (density - 0.16f) * 0.36f, 0.0f, 0.50f);
    case TreeGenerationFamily::Jungle:
        return density < 0.12f ? std::clamp(density * 0.12f, 0.0f, 0.04f)
                               : std::clamp(baseSettings.spawnChance + density * 0.38f, 0.0f, 0.76f);
    case TreeGenerationFamily::SparseJungle:
        return density < 0.14f ? std::clamp(density * 0.10f, 0.0f, 0.03f)
                               : std::clamp(baseSettings.spawnChance + density * 0.20f, 0.0f, 0.42f);
    case TreeGenerationFamily::BambooJungle:
        return density < 0.12f ? std::clamp(density * 0.12f, 0.0f, 0.04f)
                               : std::clamp(baseSettings.spawnChance + density * 0.42f, 0.0f, 0.82f);
    case TreeGenerationFamily::None:
    default:
        return 0.0f;
    }
}

float softenTreeSpawnChanceForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    const float spawnChance)
{
    if (transition.neighboringBiome == biome || transition.edgeStrength <= 0.20f)
    {
        return spawnChance;
    }

    const float edgeBlend = std::clamp((transition.edgeStrength - 0.20f) / 0.80f, 0.0f, 1.0f);
    const TreeGenerationFamily centerFamily = biomeProfile(biome).treeFamily;
    const TreeGenerationFamily neighborFamily = biomeProfile(transition.neighboringBiome).treeFamily;
    const float neutralChance = treeBiomeSettingsForSurfaceBiome(biome).spawnChance * 0.82f;
    const float familyDamp = centerFamily == neighborFamily ? 0.28f : 0.46f;
    return std::clamp(spawnChance + (neutralChance - spawnChance) * edgeBlend * familyDamp, 0.0f, 0.92f);
}

TreeBiomeSettings treeVariantSettings(
    const SurfaceBiome biome,
    const TreeBiomeSettings& baseSettings,
    const BiomeVariationSample& variation,
    const int worldX,
    const int worldZ)
{
    TreeBiomeSettings settings = baseSettings;
    const float variantRoll = noise::random01(worldX, worldZ, kTreeShapeSeed + 0x6b9U);

    switch (biomeProfile(biome).treeFamily)
    {
    case TreeGenerationFamily::Plains:
        settings.trunkBlock = variantRoll < 0.86f ? BlockType::OakLog : BlockType::BirchLog;
        settings.crownBlock = settings.trunkBlock == BlockType::OakLog ? BlockType::OakLeaves : BlockType::BirchLeaves;
        break;
    case TreeGenerationFamily::Forest:
        if (variation.primaryVariant == WoodlandVariant::BirchPocket)
        {
            settings.trunkBlock = variantRoll < 0.58f ? BlockType::BirchLog : BlockType::OakLog;
            settings.crownBlock = settings.trunkBlock == BlockType::BirchLog ? BlockType::BirchLeaves : BlockType::OakLeaves;
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 5);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 8);
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate;
            settings.largeTreeChance = std::max(settings.largeTreeChance, 0.08f);
            settings.largeTreeHeightBonus = std::max(settings.largeTreeHeightBonus, 1);
            settings.largeTreeCrownRadiusBonus = std::max(settings.largeTreeCrownRadiusBonus, 1);
        }
        else if (variation.primaryVariant == WoodlandVariant::DryClearing)
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.maxTrunkHeight = std::max(settings.minTrunkHeight, settings.maxTrunkHeight - 1);
            settings.largeTreeChance = 0.0f;
        }
        else
        {
            settings.trunkBlock = variantRoll < 0.88f ? BlockType::OakLog : BlockType::BirchLog;
            settings.crownBlock = settings.trunkBlock == BlockType::OakLog ? BlockType::OakLeaves : BlockType::BirchLeaves;
            if (variation.primaryVariant == WoodlandVariant::MossyHollow)
            {
                settings.crownRadius = std::max(settings.crownRadius, 3);
                settings.canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate;
                settings.largeTreeChance = std::max(settings.largeTreeChance, 0.18f);
                settings.largeTreeHeightBonus = std::max(settings.largeTreeHeightBonus, 2);
                settings.largeTreeCrownRadiusBonus = std::max(settings.largeTreeCrownRadiusBonus, 1);
            }
            else if (variation.canopyDensity > 0.74 && variation.primaryVariant == WoodlandVariant::WoodlandCore)
            {
                settings.canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate;
                settings.largeTreeChance = std::max(settings.largeTreeChance, 0.10f);
                settings.largeTreeHeightBonus = std::max(settings.largeTreeHeightBonus, 1);
            }
        }
        break;
    case TreeGenerationFamily::BirchForest:
        settings.trunkBlock = BlockType::BirchLog;
        settings.crownBlock = BlockType::BirchLeaves;
        settings.minTrunkHeight = std::max(settings.minTrunkHeight, variation.primaryVariant == WoodlandVariant::BirchPocket ? 7 : 5);
        settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, variation.primaryVariant == WoodlandVariant::BirchPocket ? 10 : 8);
        settings.crownRadius = variation.primaryVariant == WoodlandVariant::DryClearing ? 2 : 3;
        if (variation.primaryVariant == WoodlandVariant::BirchPocket)
        {
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate;
            settings.largeTreeChance = 0.14f;
            settings.largeTreeHeightBonus = 1;
            settings.largeTreeCrownRadiusBonus = 1;
        }
        break;
    case TreeGenerationFamily::DarkForest:
        settings.trunkBlock = (variation.primaryVariant == WoodlandVariant::DryClearing || variantRoll > 0.82f)
            ? BlockType::OakLog
            : BlockType::DarkOakLog;
        settings.crownBlock = settings.trunkBlock == BlockType::DarkOakLog ? BlockType::DarkOakLeaves : BlockType::OakLeaves;
        settings.canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate;
        if (settings.trunkBlock == BlockType::DarkOakLog)
        {
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 6);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 9);
            settings.crownRadius = variation.primaryVariant == WoodlandVariant::MossyHollow ? 4 : 3;
            if (variation.primaryVariant == WoodlandVariant::MossyHollow)
            {
                settings.largeTreeChance = std::max(settings.largeTreeChance, 0.52f);
                settings.largeTreeHeightBonus = std::max(settings.largeTreeHeightBonus, 3);
                settings.largeTreeCrownRadiusBonus = std::max(settings.largeTreeCrownRadiusBonus, 2);
            }
        }
        break;
    case TreeGenerationFamily::Taiga:
        settings.trunkBlock = BlockType::SpruceLog;
        settings.crownBlock = BlockType::SpruceLeaves;
        if (variation.primaryVariant == WoodlandVariant::DryClearing && variantRoll > 0.84f)
        {
            settings.trunkBlock = BlockType::BirchLog;
            settings.crownBlock = BlockType::BirchLeaves;
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        break;
    case TreeGenerationFamily::SnowyTaiga:
        settings.trunkBlock = variantRoll < 0.88f ? BlockType::SpruceLog : BlockType::BirchLog;
        settings.crownBlock = settings.trunkBlock == BlockType::SpruceLog ? BlockType::SpruceLeaves : BlockType::BirchLeaves;
        if (settings.trunkBlock == BlockType::BirchLog)
        {
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        break;
    case TreeGenerationFamily::Jungle:
    case TreeGenerationFamily::SparseJungle:
    case TreeGenerationFamily::BambooJungle:
        settings.trunkBlock = variantRoll < 0.88f ? BlockType::JungleLog : BlockType::OakLog;
        settings.crownBlock = settings.trunkBlock == BlockType::JungleLog ? BlockType::JungleLeaves : BlockType::OakLeaves;
        break;
    case TreeGenerationFamily::None:
    default:
        break;
    }

    if (variation.primaryVariant == WoodlandVariant::WoodedHills)
    {
        settings.largeTreeChance *= 0.72f;
    }
    else if (variation.primaryVariant == WoodlandVariant::WoodedMountains)
    {
        settings.largeTreeChance *= 0.40f;
        settings.maxTrunkHeight = std::max(settings.minTrunkHeight, settings.maxTrunkHeight - 1);
        settings.crownRadius = std::max(2, settings.crownRadius - 1);
    }

    return settings;
}

TreeBiomeSettings softenTreeSettingsForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    TreeBiomeSettings settings)
{
    if (transition.neighboringBiome == biome || transition.edgeStrength <= 0.20f)
    {
        return settings;
    }

    const float edgeBlend = std::clamp((transition.edgeStrength - 0.20f) / 0.80f, 0.0f, 1.0f);
    const TreeGenerationFamily centerFamily = biomeProfile(biome).treeFamily;
    const TreeGenerationFamily neighborFamily = biomeProfile(transition.neighboringBiome).treeFamily;

    settings.largeTreeChance *= 1.0f - edgeBlend * 0.72f;
    if (edgeBlend > 0.42f)
    {
        settings.trunkWidth = 1;
    }
    if (settings.crownRadius > 2)
    {
        settings.crownRadius -= static_cast<int>(std::round(edgeBlend));
        settings.crownRadius = std::max(settings.crownRadius, 2);
    }
    if (centerFamily != neighborFamily && edgeBlend > 0.50f)
    {
        settings.canopyStyle = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Jungle
            ? TreeBiomeSettings::CanopyStyle::Jungle
            : TreeBiomeSettings::CanopyStyle::Temperate;
    }

    return settings;
}

float secondaryTreeChance(const SurfaceBiome biome, const BiomeVariationSample& variation)
{
    float extraChance = 0.0f;
    switch (biomeProfile(biome).treeFamily)
    {
    case TreeGenerationFamily::Forest:
        extraChance = 0.70f;
        break;
    case TreeGenerationFamily::BirchForest:
        extraChance = 0.28f;
        break;
    case TreeGenerationFamily::DarkForest:
        extraChance = 0.44f;
        break;
    case TreeGenerationFamily::Taiga:
        extraChance = 0.30f;
        break;
    case TreeGenerationFamily::SnowyTaiga:
        extraChance = 0.24f;
        break;
    default:
        return 0.0f;
    }

    if (!supportsWoodlandVariation(biome))
    {
        return extraChance;
    }

    float multiplier = static_cast<float>(0.55 + variation.canopyDensity * 0.85 + variation.lushness * 0.12 - variation.dryness * 0.28);
    switch (variation.primaryVariant)
    {
    case WoodlandVariant::DryClearing:
        multiplier *= 0.32f;
        break;
    case WoodlandVariant::MossyHollow:
        multiplier *= 1.18f;
        break;
    case WoodlandVariant::RockyRise:
        multiplier *= 0.58f;
        break;
    case WoodlandVariant::WoodedHills:
        multiplier *= 0.78f;
        break;
    case WoodlandVariant::WoodedMountains:
        multiplier *= 0.42f;
        break;
    default:
        break;
    }

    return std::clamp(extraChance * multiplier, 0.0f, 0.92f);
}

float softenSecondaryTreeChanceForBiomeEdge(
    const SurfaceBiome biome,
    const BiomeTransitionSample& transition,
    const float extraChance)
{
    if (transition.neighboringBiome == biome || transition.edgeStrength <= 0.20f)
    {
        return extraChance;
    }

    const float edgeBlend = std::clamp((transition.edgeStrength - 0.20f) / 0.80f, 0.0f, 1.0f);
    const TreeGenerationFamily centerFamily = biomeProfile(biome).treeFamily;
    const TreeGenerationFamily neighborFamily = biomeProfile(transition.neighboringBiome).treeFamily;
    const float damp = centerFamily == neighborFamily ? 0.18f : 0.42f;
    return std::clamp(extraChance * (1.0f - edgeBlend * damp), 0.0f, 0.92f);
}
}  // namespace vibecraft::world::biomes
