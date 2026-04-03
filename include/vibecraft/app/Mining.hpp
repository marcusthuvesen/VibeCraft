#pragma once

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
[[nodiscard]] float miningDurationSeconds(
    world::BlockType targetBlockType,
    world::BlockType equippedBlockType,
    EquippedItem equippedItem);
}  // namespace vibecraft::app
