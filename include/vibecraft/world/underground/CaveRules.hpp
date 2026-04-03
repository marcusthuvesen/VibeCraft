#pragma once

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <cstdint>

namespace vibecraft::world::underground
{
/// 3D density carve: sparse overall, rarer near surface; allowed band [undergroundStartY, surface - roofBuffer].
[[nodiscard]] bool shouldCarveCave(int worldX, int y, int worldZ, int surfaceHeight);

/// Narrow woodland ravines: open air near the surface along the same crevasse noise as height variation.
[[nodiscard]] bool shouldCarveWoodlandSurfaceRavine(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight,
    SurfaceBiome surfaceBiome,
    std::uint32_t worldSeed);

/// Fills carved space with shallow aquifers, rare deep thermal vents, or open cavern air.
[[nodiscard]] vibecraft::world::BlockType caveInteriorBlockType(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight);
}  // namespace vibecraft::world::underground
