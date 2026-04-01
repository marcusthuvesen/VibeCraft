#pragma once

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world::underground
{
/// 3D density carve: sparse overall, rarer near surface; allowed band [undergroundStartY, surface - roofBuffer].
[[nodiscard]] bool shouldCarveCave(int worldX, int y, int worldZ, int surfaceHeight);

/// Fills carved space with shallow aquifers, rare deep thermal vents, or open cavern air.
[[nodiscard]] vibecraft::world::BlockType caveInteriorBlockType(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight);
}  // namespace vibecraft::world::underground
