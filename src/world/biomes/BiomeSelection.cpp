#include "vibecraft/world/biomes/BiomeSelection.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
[[nodiscard]] bool isWarm(const double temperature)
{
    return temperature > 0.16;
}

[[nodiscard]] bool isVeryWarm(const double temperature)
{
    return temperature > 0.26;
}

[[nodiscard]] bool isCold(const double temperature)
{
    return temperature < -0.06;
}

[[nodiscard]] bool isVeryCold(const double temperature)
{
    return temperature < -0.24;
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
            360.0,
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
            440.0,
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
            500.0,
            2,
            mixedSeed(0x3c90b07eU, inputs.worldSeed))
            * 0.5
            + 0.5,
        0.0,
        1.0);

    if (inputs.surfaceHeight >= 108)
    {
        if (isVeryCold(inputs.temperature) || inputs.temperature < -0.22)
        {
            return alpineNoise > 0.52 ? SurfaceBiome::JaggedPeaks : SurfaceBiome::FrozenPeaks;
        }
        if (isWarm(inputs.temperature) && inputs.humidity < 0.14)
        {
            return alpineNoise > 0.56 ? SurfaceBiome::WindsweptSavanna : SurfaceBiome::SavannaPlateau;
        }
        return SurfaceBiome::StonyPeaks;
    }
    if (inputs.surfaceHeight >= 92 && inputs.temperature < -0.10)
    {
        return SurfaceBiome::SnowySlopes;
    }

    if (isWarm(inputs.temperature) && inputs.humidity > 0.30 && inputs.surfaceHeight <= 76)
    {
        return SurfaceBiome::Swamp;
    }
    if (inputs.humidity > 0.34
        && inputs.temperature > -0.02
        && inputs.temperature < 0.28
        && inputs.surfaceHeight <= 82
        && fungusNoise > 0.68)
    {
        return SurfaceBiome::MushroomField;
    }
    if (inputs.temperature > 0.30 && inputs.humidity > 0.38)
    {
        return SurfaceBiome::BambooJungle;
    }
    if (isWarm(inputs.temperature) && inputs.humidity > 0.16)
    {
        return inputs.humidity > 0.28 ? SurfaceBiome::Jungle : SurfaceBiome::SparseJungle;
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

    if (inputs.surfaceHeight >= 88 && inputs.humidity > -0.18)
    {
        return SurfaceBiome::WindsweptHills;
    }
    if (inputs.surfaceHeight >= 84
        && inputs.humidity > -0.02
        && inputs.humidity < 0.38
        && variantNoise > 0.34)
    {
        return SurfaceBiome::Meadow;
    }
    if (inputs.humidity > 0.18 && inputs.humidity < 0.40 && variantNoise > 0.54)
    {
        return SurfaceBiome::FlowerForest;
    }
    if (inputs.humidity > 0.04 && inputs.humidity < 0.24 && variantNoise > 0.64)
    {
        return SurfaceBiome::OldGrowthBirchForest;
    }
    if (inputs.humidity < 0.08 && variantNoise > 0.48)
    {
        return SurfaceBiome::SunflowerPlains;
    }

    if (inputs.humidity > 0.40)
    {
        return SurfaceBiome::DarkForest;
    }
    if (inputs.humidity > 0.20)
    {
        return SurfaceBiome::Forest;
    }
    if (inputs.humidity > 0.02)
    {
        return SurfaceBiome::BirchForest;
    }
    if (inputs.humidity > -0.14)
    {
        return SurfaceBiome::Plains;
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
