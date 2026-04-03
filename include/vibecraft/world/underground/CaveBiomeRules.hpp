#pragma once

#include <cstdint>

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

namespace vibecraft::world::underground
{
enum class CaveBiome : std::uint8_t
{
    Default = 0,
    Lush,
    Dripstone,
    DeepDark,
};

[[nodiscard]] CaveBiome sampleCaveBiome(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight,
    SurfaceBiome surfaceBiome);
}  // namespace vibecraft::world::underground
