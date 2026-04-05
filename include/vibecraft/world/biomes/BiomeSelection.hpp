#pragma once

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <cstdint>
#include <optional>

namespace vibecraft::world::biomes
{
struct BiomeSelectionInputs
{
    int worldX = 0;
    int worldZ = 0;
    int surfaceHeight = 63;
    std::uint32_t worldSeed = 0;
    double temperature = 0.0;
    double humidity = 0.0;
    bool starterRegion = false;
};

[[nodiscard]] SurfaceBiome selectRawSurfaceBiome(
    const BiomeSelectionInputs& inputs,
    std::optional<SurfaceBiome> biomeOverride);

[[nodiscard]] SurfaceBiome selectSurfaceBiome(
    const BiomeSelectionInputs& inputs,
    std::optional<SurfaceBiome> biomeOverride);
}  // namespace vibecraft::world::biomes
