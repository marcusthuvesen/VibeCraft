#include "vibecraft/world/biomes/BiomeSelection.hpp"

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
}  // namespace

SurfaceBiome selectSurfaceBiome(
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

    if (isVeryCold(inputs.temperature) || (inputs.surfaceHeight >= 106 && inputs.temperature < -0.18))
    {
        return inputs.humidity > -0.08 ? SurfaceBiome::SnowyTaiga : SurfaceBiome::SnowyPlains;
    }
    if (isCold(inputs.temperature))
    {
        return inputs.humidity > -0.02 ? SurfaceBiome::Taiga : SurfaceBiome::Plains;
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
}  // namespace vibecraft::world::biomes
