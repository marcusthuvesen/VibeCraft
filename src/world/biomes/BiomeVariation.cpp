#include "vibecraft/world/biomes/BiomeVariation.hpp"

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

[[nodiscard]] double normalizedFbm(
    const double worldX,
    const double worldZ,
    const double scale,
    const int octaves,
    const std::uint32_t seed)
{
    return std::clamp(noise::fbmNoise2d(worldX, worldZ, scale, octaves, seed) * 0.5 + 0.5, 0.0, 1.0);
}

[[nodiscard]] bool isBirchCapableBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Forest:
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
        return true;
    default:
        return false;
    }
}
}  // namespace

bool supportsWoodlandVariation(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Forest:
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::Taiga:
    case SurfaceBiome::OldGrowthSpruceTaiga:
    case SurfaceBiome::OldGrowthPineTaiga:
    case SurfaceBiome::SnowyTaiga:
        return true;
    default:
        return false;
    }
}

BiomeVariationSample sampleBiomeVariation(
    const SurfaceBiome biome,
    const int worldX,
    const int worldZ,
    const std::uint32_t worldSeed)
{
    const double wx = static_cast<double>(worldX);
    const double wz = static_cast<double>(worldZ);

    const double lushBase = normalizedFbm(wx + 41.0, wz - 73.0, 210.0, 3, mixedSeed(0x184b9d20U, worldSeed));
    const double dryBase = normalizedFbm(wx - 119.0, wz + 53.0, 240.0, 3, mixedSeed(0x295cae31U, worldSeed));
    const double roughBase = std::clamp(
        noise::ridgeNoise2d(wx + 67.0, wz + 23.0, 104.0, mixedSeed(0x3a6dbf42U, worldSeed)),
        0.0,
        1.0);
    const double canopyBase = normalizedFbm(wx - 29.0, wz - 91.0, 176.0, 3, mixedSeed(0x4b7ed053U, worldSeed));
    const double hollowBase = normalizedFbm(wx + 133.0, wz + 17.0, 146.0, 2, mixedSeed(0x5c8fe164U, worldSeed));
    const double pocketBase = normalizedFbm(wx - 151.0, wz + 87.0, 158.0, 2, mixedSeed(0x6da0f275U, worldSeed));

    double lushness = std::clamp(lushBase * 0.72 + (1.0 - dryBase) * 0.28, 0.0, 1.0);
    double dryness = std::clamp(dryBase * 0.78 + (1.0 - lushBase) * 0.22, 0.0, 1.0);
    double roughness = std::clamp(roughBase * 0.76 + pocketBase * 0.24, 0.0, 1.0);
    double canopyDensity = std::clamp(canopyBase * 0.72 + lushness * 0.18 - dryness * 0.12, 0.0, 1.0);

    if (biome == SurfaceBiome::DarkForest)
    {
        canopyDensity = std::clamp(canopyDensity + 0.18, 0.0, 1.0);
        lushness = std::clamp(lushness + 0.08, 0.0, 1.0);
    }
    else if (biome == SurfaceBiome::FlowerForest)
    {
        lushness = std::clamp(lushness + 0.10, 0.0, 1.0);
        dryness = std::clamp(dryness - 0.08, 0.0, 1.0);
    }
    else if (biome == SurfaceBiome::BirchForest || biome == SurfaceBiome::OldGrowthBirchForest)
    {
        canopyDensity = std::clamp(canopyDensity - 0.06, 0.0, 1.0);
        dryness = std::clamp(dryness - 0.04, 0.0, 1.0);
    }
    else if (biome == SurfaceBiome::Taiga
             || biome == SurfaceBiome::OldGrowthSpruceTaiga
             || biome == SurfaceBiome::OldGrowthPineTaiga
             || biome == SurfaceBiome::SnowyTaiga)
    {
        canopyDensity = std::clamp(canopyDensity + 0.08, 0.0, 1.0);
        roughness = std::clamp(roughness + 0.04, 0.0, 1.0);
    }

    WoodlandVariant primaryVariant = WoodlandVariant::None;
    if (supportsWoodlandVariation(biome))
    {
        // Keep rocky pockets uncommon so forest floors stay mostly grassy.
        if (roughness > 0.82 && canopyDensity < 0.54 && dryness > 0.52)
        {
            primaryVariant = WoodlandVariant::RockyRise;
        }
        else if (dryness > 0.70 && canopyDensity < 0.46)
        {
            primaryVariant = WoodlandVariant::DryClearing;
        }
        else if (lushness > 0.72 && canopyDensity > 0.66 && hollowBase > 0.56)
        {
            primaryVariant = WoodlandVariant::MossyHollow;
        }
        else if (lushness > 0.66 && canopyDensity < 0.64)
        {
            primaryVariant = WoodlandVariant::FernGrove;
        }
        else if (isBirchCapableBiome(biome) && pocketBase > 0.68 && dryness < 0.60)
        {
            primaryVariant = WoodlandVariant::BirchPocket;
        }
        else
        {
            primaryVariant = WoodlandVariant::WoodlandCore;
        }
    }

    return BiomeVariationSample{
        .primaryVariant = primaryVariant,
        .lushness = lushness,
        .dryness = dryness,
        .roughness = roughness,
        .canopyDensity = canopyDensity,
    };
}
}  // namespace vibecraft::world::biomes
