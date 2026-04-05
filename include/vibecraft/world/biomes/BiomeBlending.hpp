#pragma once

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <array>
#include <cstddef>

namespace vibecraft::world::biomes
{
inline constexpr std::size_t kBiomeBlendNeighborCount = 12;

struct BiomeBlendOffset
{
    int dx = 0;
    int dz = 0;
};

[[nodiscard]] const std::array<BiomeBlendOffset, kBiomeBlendNeighborCount>& biomeBlendOffsets();

[[nodiscard]] SurfaceBiome selectBlendedSurfaceBiome(
    SurfaceBiome centerBiome,
    const std::array<SurfaceBiome, kBiomeBlendNeighborCount>& nearbyBiomes);
}  // namespace vibecraft::world::biomes
