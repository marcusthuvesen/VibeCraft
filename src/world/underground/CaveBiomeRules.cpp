#include "vibecraft/world/underground/CaveBiomeRules.hpp"

#include <algorithm>

#include "vibecraft/world/TerrainNoise.hpp"

namespace vibecraft::world::underground
{
namespace
{
[[nodiscard]] bool prefersLushCaves(const SurfaceBiome surfaceBiome)
{
    switch (surfaceBiome)
    {
    case SurfaceBiome::Forest:
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::OldGrowthSpruceTaiga:
    case SurfaceBiome::OldGrowthPineTaiga:
    case SurfaceBiome::Swamp:
    case SurfaceBiome::MushroomField:
    case SurfaceBiome::Jungle:
    case SurfaceBiome::SparseJungle:
    case SurfaceBiome::BambooJungle:
        return true;
    default:
        return false;
    }
}
}  // namespace

CaveBiome sampleCaveBiome(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const SurfaceBiome surfaceBiome)
{
    const int caveDepth = surfaceHeight - y;
    if (caveDepth < 18)
    {
        return CaveBiome::Default;
    }

    const double lushMask = noise::fbmNoise2d(
        static_cast<double>(worldX) + static_cast<double>(y) * 0.31,
        static_cast<double>(worldZ) - static_cast<double>(y) * 0.19,
        82.0,
        3,
        0x1a51ca0eU);
    const double dripstoneMask = noise::fbmNoise2d(
        static_cast<double>(worldX) - static_cast<double>(y) * 0.27,
        static_cast<double>(worldZ) + static_cast<double>(y) * 0.23,
        76.0,
        3,
        0xd715710eU);
    const double deepDarkMask = noise::fbmNoise2d(
        static_cast<double>(worldX) + static_cast<double>(y) * 0.53,
        static_cast<double>(worldZ) + static_cast<double>(y) * 0.41,
        94.0,
        3,
        0xdee9da4aU);

    if (y <= -32 && caveDepth >= 34 && deepDarkMask > 0.66)
    {
        return CaveBiome::DeepDark;
    }
    if (y >= -24 && y <= 56 && caveDepth >= 24 && dripstoneMask > 0.64)
    {
        return CaveBiome::Dripstone;
    }
    if (y >= -8 && y <= 72 && caveDepth >= 26 && prefersLushCaves(surfaceBiome) && lushMask > 0.62)
    {
        return CaveBiome::Lush;
    }

    return CaveBiome::Default;
}
}  // namespace vibecraft::world::underground
