#pragma once

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world::underground
{
/// 3D density carve: preserves surface band and strata above [undergroundStartY, surface - roofBuffer].
[[nodiscard]] bool shouldCarveCave(int worldX, int y, int worldZ, int surfaceHeight);

/// Fills carved space: sea-level water tables, rare deep lava pools (Minecraft-like), or air.
[[nodiscard]] vibecraft::world::BlockType caveInteriorBlockType(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight);
}  // namespace vibecraft::world::underground
