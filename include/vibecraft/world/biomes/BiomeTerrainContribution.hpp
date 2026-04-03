#pragma once

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <cstdint>

namespace vibecraft::world::biomes
{
[[nodiscard]] double biomeTerrainContribution(
    SurfaceBiome biome,
    int worldX,
    int worldZ,
    double continents,
    double uplands,
    double ridges,
    std::uint32_t worldSeed);
}  // namespace vibecraft::world::biomes
