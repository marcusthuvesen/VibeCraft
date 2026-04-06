#include "vibecraft/world/biomes/BiomeBlending.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
inline constexpr std::array<BiomeBlendOffset, kBiomeBlendNeighborCount> kOffsets{{
    {8, 0},
    {-8, 0},
    {0, 8},
    {0, -8},
    {6, 6},
    {-6, 6},
    {6, -6},
    {-6, -6},
    {14, 0},
    {-14, 0},
    {0, 14},
    {0, -14},
}};

inline constexpr std::array<float, kBiomeBlendNeighborCount> kWeights{{
    0.74f,
    0.74f,
    0.74f,
    0.74f,
    0.52f,
    0.52f,
    0.52f,
    0.52f,
    0.24f,
    0.24f,
    0.24f,
    0.24f,
}};

inline constexpr std::size_t kSurfaceBiomeCount = static_cast<std::size_t>(SurfaceBiome::IceSpikePlains) + 1;

[[nodiscard]] constexpr std::size_t biomeIndex(const SurfaceBiome biome)
{
    return static_cast<std::size_t>(biome);
}
}  // namespace

const std::array<BiomeBlendOffset, kBiomeBlendNeighborCount>& biomeBlendOffsets()
{
    return kOffsets;
}

SurfaceBiome selectBlendedSurfaceBiome(
    const SurfaceBiome centerBiome,
    const std::array<SurfaceBiome, kBiomeBlendNeighborCount>& nearbyBiomes)
{
    std::array<float, kSurfaceBiomeCount> biomeWeights{};
    constexpr float kCenterWeight = 1.08f;
    biomeWeights[biomeIndex(centerBiome)] += kCenterWeight;
    float totalWeight = kCenterWeight;

    for (std::size_t i = 0; i < nearbyBiomes.size(); ++i)
    {
        biomeWeights[biomeIndex(nearbyBiomes[i])] += kWeights[i];
        totalWeight += kWeights[i];
    }

    SurfaceBiome strongestBiome = centerBiome;
    float strongestWeight = biomeWeights[biomeIndex(centerBiome)];
    for (std::size_t i = 0; i < biomeWeights.size(); ++i)
    {
        if (biomeWeights[i] > strongestWeight)
        {
            strongestWeight = biomeWeights[i];
            strongestBiome = static_cast<SurfaceBiome>(i);
        }
    }

    if (strongestBiome == centerBiome)
    {
        return centerBiome;
    }

    const float centerWeight = biomeWeights[biomeIndex(centerBiome)];
    const float centerShare = centerWeight / totalWeight;
    const float strongestShare = strongestWeight / totalWeight;
    if (strongestShare < 0.29f)
    {
        return centerBiome;
    }
    if (centerShare >= 0.43f && strongestWeight < centerWeight + 0.25f)
    {
        return centerBiome;
    }
    if (strongestShare > centerShare + 0.08f || centerShare < 0.32f)
    {
        return strongestBiome;
    }
    return centerBiome;
}
}  // namespace vibecraft::world::biomes
