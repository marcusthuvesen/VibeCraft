#pragma once

#include "vibecraft/world/biomes/BiomeProfile.hpp"

#include <algorithm>
#include <array>

namespace vibecraft::world::biomes
{
struct BiomeTransitionSample
{
    SurfaceBiome neighboringBiome = SurfaceBiome::Forest;
    float edgeStrength = 0.0f;
};

template <typename BiomeSampler>
[[nodiscard]] BiomeTransitionSample sampleBiomeTransition(
    const SurfaceBiome centerBiome,
    const int worldX,
    const int worldZ,
    const BiomeSampler& biomeSampler)
{
    struct WeightedOffset
    {
        int dx = 0;
        int dz = 0;
        float weight = 0.0f;
    };

    constexpr std::array<WeightedOffset, 12> kOffsets{{
        {12, 0, 1.00f},
        {-12, 0, 1.00f},
        {0, 12, 1.00f},
        {0, -12, 1.00f},
        {10, 10, 0.85f},
        {-10, 10, 0.85f},
        {10, -10, 0.85f},
        {-10, -10, 0.85f},
        {24, 0, 0.60f},
        {-24, 0, 0.60f},
        {0, 24, 0.60f},
        {0, -24, 0.60f},
    }};

    float totalWeight = 0.0f;
    float differingWeight = 0.0f;
    SurfaceBiome strongestNeighbor = centerBiome;
    float strongestNeighborWeight = -1.0f;

    for (const auto& offset : kOffsets)
    {
        const SurfaceBiome neighborBiome = biomeSampler(worldX + offset.dx, worldZ + offset.dz);
        totalWeight += offset.weight;
        if (neighborBiome == centerBiome)
        {
            continue;
        }
        differingWeight += offset.weight;
        if (offset.weight > strongestNeighborWeight)
        {
            strongestNeighborWeight = offset.weight;
            strongestNeighbor = neighborBiome;
        }
    }

    const float edgeStrength = totalWeight > 0.0f ? std::clamp(differingWeight / totalWeight, 0.0f, 1.0f) : 0.0f;
    return BiomeTransitionSample{
        .neighboringBiome = strongestNeighbor,
        .edgeStrength = edgeStrength,
    };
}
}  // namespace vibecraft::world::biomes
