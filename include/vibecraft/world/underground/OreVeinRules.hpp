#pragma once

#include <optional>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world::underground
{
/// Host block is plain stone or deepslate (no ore overlay on dirt/sand/etc.).
/// Returns the ore to place, or std::nullopt to keep the host block.
/// Priority follows rarity: emerald (mountains) > diamond > gold > iron > coal.
[[nodiscard]] std::optional<BlockType> selectOreVeinBlock(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight,
    BlockType hostBlockType);
}  // namespace vibecraft::world::underground
