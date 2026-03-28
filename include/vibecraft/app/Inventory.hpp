#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
struct InventorySlot
{
    vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
    std::uint32_t count = 0;
};

constexpr std::size_t kHotbarSlotCount = 9;
constexpr std::size_t kBagSlotCount = 27;
constexpr std::uint32_t kMaxStackSize = 64;

using HotbarSlots = std::array<InventorySlot, kHotbarSlotCount>;
using BagSlots = std::array<InventorySlot, kBagSlotCount>;

[[nodiscard]] const char* blockTypeLabel(vibecraft::world::BlockType blockType);
[[nodiscard]] bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex);
void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, std::size_t selectedHotbarIndex);
}  // namespace vibecraft::app
