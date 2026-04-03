#include "vibecraft/world/WoodlandRavine.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>
#include <cmath>

namespace vibecraft::world
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
}  // namespace

bool isWoodlandRavineBiome(const SurfaceBiome biome)
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

double woodlandRavineBiomeScale(const SurfaceBiome biome)
{
    if (biome == SurfaceBiome::SnowyTaiga)
    {
        return 0.72;
    }
    if (biome == SurfaceBiome::DarkForest)
    {
        return 1.12;
    }
    return 1.0;
}

WoodlandRavineSample sampleWoodlandRavine(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double wx = static_cast<double>(worldX);
    const double wz = static_cast<double>(worldZ);
    const double wobble =
        noise::fbmNoise2d(wx + 19.0, wz - 11.0, 76.0, 2, mixedSeed(0xc91e4b2dU, worldSeed));
    const double a1 = wx * 0.036 + wz * 0.028;
    const double a2 = wx * -0.031 + wz * 0.041;
    const double line1 = std::abs(std::sin(a1 * 1.08 + wobble * 0.55));
    const double line2 = std::abs(std::sin(a2 * 0.97 - wobble * 0.38));
    const double slot = std::min(line1, line2);
    const double inRavine = std::clamp((0.20 - slot) / 0.20, 0.0, 1.0);
    const double ravineShape = std::pow(inRavine, 1.35);
    const double depthNoise =
        0.5
        + 0.5
            * noise::fbmNoise2d(wx - 211.0, wz + 177.0, 240.0, 2, mixedSeed(0xd82f5c3eU, worldSeed));
    const double regionalMask = std::clamp(
        noise::fbmNoise2d(wx + 401.0, wz - 333.0, 620.0, 2, mixedSeed(0xe93a6d4fU, worldSeed)) * 0.65 + 0.5,
        0.0,
        1.0);
    return WoodlandRavineSample{
        .ravineShape = ravineShape,
        .regionalMask = regionalMask,
        .depthNoise = depthNoise,
    };
}

double woodlandSurfaceHeightDelta(
    const SurfaceBiome biome,
    const int worldX,
    const int worldZ,
    const std::uint32_t worldSeed)
{
    if (!isWoodlandRavineBiome(biome))
    {
        return 0.0;
    }

    const double wx = static_cast<double>(worldX);
    const double wz = static_cast<double>(worldZ);
    const WoodlandRavineSample s = sampleWoodlandRavine(worldX, worldZ, worldSeed);
    const double ravineDepth =
        s.ravineShape * s.depthNoise * s.regionalMask * 12.5 * woodlandRavineBiomeScale(biome);

    const double micro =
        (noise::fbmNoise2d(wx, wz, 12.5, 2, mixedSeed(0xfa4b7e60U, worldSeed)) * 2.0 - 1.0) * 3.2;
    const double jitter =
        (noise::fbmNoise2d(wx - 83.0, wz + 59.0, 5.8, 2, mixedSeed(0x0b5c8f71U, worldSeed)) * 2.0 - 1.0) * 1.1;

    return micro + jitter - ravineDepth;
}
}  // namespace vibecraft::world
