#include "vibecraft/world/biomes/BiomeBlending.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
inline constexpr std::array<BiomeBlendOffset, kBiomeBlendNeighborCount> kOffsets{{
    {16, 0},
    {-16, 0},
    {0, 16},
    {0, -16},
    {14, 14},
    {-14, 14},
    {14, -14},
    {-14, -14},
    {32, 0},
    {-32, 0},
    {0, 32},
    {0, -32},
}};

inline constexpr std::array<float, kBiomeBlendNeighborCount> kWeights{{
    1.00f,
    1.00f,
    1.00f,
    1.00f,
    0.82f,
    0.82f,
    0.82f,
    0.82f,
    0.56f,
    0.56f,
    0.56f,
    0.56f,
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
    constexpr float kCenterWeight = 1.85f;
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
    if (strongestShare < 0.34f)
    {
        return centerBiome;
    }
    if (centerShare >= 0.50f && strongestWeight < centerWeight + 0.35f)
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
