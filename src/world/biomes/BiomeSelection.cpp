#include "vibecraft/world/biomes/BiomeSelection.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
[[nodiscard]] bool isWarm(const double temperature)
{
    return temperature > 0.24;
}

[[nodiscard]] bool isVeryWarm(const double temperature)
{
    return temperature > 0.34;
}

[[nodiscard]] bool isCold(const double temperature)
{
    return temperature < -0.10;
}

[[nodiscard]] bool isVeryCold(const double temperature)
{
    return temperature < -0.34;
}

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
}  // namespace

SurfaceBiome selectRawSurfaceBiome(
    const BiomeSelectionInputs& inputs,
    const std::optional<SurfaceBiome> biomeOverride)
{
    if (biomeOverride.has_value())
    {
        return *biomeOverride;
    }

    if (inputs.starterRegion)
    {
        return SurfaceBiome::Forest;
    }

    const double variantNoise = std::clamp(
        noise::fbmNoise2d(
            static_cast<double>(inputs.worldX) + 31.0,
            static_cast<double>(inputs.worldZ) - 17.0,
            720.0,
            3,
            mixedSeed(0x1f7d8a5cU, inputs.worldSeed))
            * 0.5
            + 0.5,
        0.0,
        1.0);
    const double alpineNoise = std::clamp(
        noise::fbmNoise2d(
            static_cast<double>(inputs.worldX) - 71.0,
            static_cast<double>(inputs.worldZ) + 43.0,
            860.0,
            3,
            mixedSeed(0x2b8e9f6dU, inputs.worldSeed))
            * 0.5
            + 0.5,
        0.0,
        1.0);
    const double fungusNoise = std::clamp(
        noise::fbmNoise2d(
            static_cast<double>(inputs.worldX) + 93.0,
            static_cast<double>(inputs.worldZ) + 11.0,
            930.0,
            2,
            mixedSeed(0x3c90b07eU, inputs.worldSeed))
            * 0.5
            + 0.5,
        0.0,
        1.0);

    if (inputs.surfaceHeight >= 130)
    {
        if (isVeryCold(inputs.temperature) || inputs.temperature < -0.22)
        {
            return alpineNoise > 0.58 ? SurfaceBiome::JaggedPeaks : SurfaceBiome::FrozenPeaks;
        }
        if (isWarm(inputs.temperature) && inputs.humidity < 0.10)
        {
            return alpineNoise > 0.62 ? SurfaceBiome::WindsweptSavanna : SurfaceBiome::SavannaPlateau;
        }
        return SurfaceBiome::StonyPeaks;
    }
    if (inputs.surfaceHeight >= 114 && inputs.temperature < -0.16)
    {
        return SurfaceBiome::SnowySlopes;
    }

    if (isWarm(inputs.temperature) && inputs.humidity > 0.36 && inputs.surfaceHeight <= 72)
    {
        return SurfaceBiome::Swamp;
    }
    if (inputs.humidity > 0.40
        && inputs.temperature > 0.02
        && inputs.temperature < 0.24
        && inputs.surfaceHeight <= 78
        && fungusNoise > 0.76)
    {
        return SurfaceBiome::MushroomField;
    }
    if (isVeryWarm(inputs.temperature) && inputs.humidity > 0.46)
    {
        return SurfaceBiome::BambooJungle;
    }
    if (isWarm(inputs.temperature) && inputs.humidity > 0.26)
    {
        return inputs.humidity > 0.34 ? SurfaceBiome::Jungle : SurfaceBiome::SparseJungle;
    }
    if (isWarm(inputs.temperature) && inputs.humidity < -0.16 && inputs.surfaceHeight <= 82)
    {
        return SurfaceBiome::Desert;
    }
    if (isWarm(inputs.temperature) && inputs.humidity < 0.08 && inputs.surfaceHeight <= 96)
    {
        return SurfaceBiome::Savanna;
    }
    if (isWarm(inputs.temperature) && inputs.humidity < 0.14 && inputs.surfaceHeight >= 108)
    {
        return SurfaceBiome::SavannaPlateau;
    }
    if (isVeryCold(inputs.temperature) || (inputs.surfaceHeight >= 106 && inputs.temperature < -0.18))
    {
        if (inputs.humidity < -0.26)
        {
            return SurfaceBiome::IceSpikePlains;
        }
        if (inputs.humidity < -0.22)
        {
            return SurfaceBiome::IcePlains;
        }
        return inputs.humidity > -0.08 ? SurfaceBiome::SnowyTaiga : SurfaceBiome::SnowyPlains;
    }
    if (isCold(inputs.temperature))
    {
        if (inputs.humidity < -0.26 && alpineNoise > 0.52)
        {
            return SurfaceBiome::IceSpikePlains;
        }
        if (inputs.humidity < -0.18)
        {
            return SurfaceBiome::IcePlains;
        }
        if (inputs.humidity > 0.18 && variantNoise > 0.66)
        {
            return SurfaceBiome::OldGrowthSpruceTaiga;
        }
        if (inputs.humidity > 0.06 && variantNoise > 0.55)
        {
            return SurfaceBiome::OldGrowthPineTaiga;
        }
        return inputs.humidity > -0.02 ? SurfaceBiome::Taiga : SurfaceBiome::Plains;
    }

    if (inputs.surfaceHeight >= 112 && inputs.humidity > -0.08)
    {
        return SurfaceBiome::WindsweptHills;
    }
    if (inputs.surfaceHeight >= 90
        && inputs.humidity > 0.02
        && inputs.humidity < 0.34
        && variantNoise > 0.40)
    {
        return SurfaceBiome::Meadow;
    }
    if (inputs.humidity > 0.22 && inputs.humidity < 0.38 && variantNoise > 0.63)
    {
        return SurfaceBiome::FlowerForest;
    }
    if (inputs.humidity > 0.08 && inputs.humidity < 0.22 && variantNoise > 0.74)
    {
        return SurfaceBiome::OldGrowthBirchForest;
    }
    if (inputs.humidity < 0.04 && variantNoise > 0.58)
    {
        return SurfaceBiome::SunflowerPlains;
    }

    if (inputs.humidity > 0.42)
    {
        return SurfaceBiome::DarkForest;
    }
    if (inputs.humidity > 0.24)
    {
        return SurfaceBiome::Forest;
    }
    if (inputs.humidity > 0.10)
    {
        return SurfaceBiome::BirchForest;
    }
    if (inputs.humidity > -0.04)
    {
        return SurfaceBiome::Forest;
    }
    return SurfaceBiome::Plains;
}

SurfaceBiome selectSurfaceBiome(
    const BiomeSelectionInputs& inputs,
    const std::optional<SurfaceBiome> biomeOverride)
{
    return selectRawSurfaceBiome(inputs, biomeOverride);
}
}  // namespace vibecraft::world::biomes
