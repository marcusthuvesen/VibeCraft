#include "vibecraft/app/Inventory.hpp"

#include <algorithm>

namespace vibecraft::app
{
namespace
{
template <std::size_t SlotCount>
bool addToMatchingOrEmptySlots(
    std::array<InventorySlot, SlotCount>& slots,
    const vibecraft::world::BlockType blockType,
    std::size_t* const selectedHotbarIndex)
{
    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.blockType == blockType && slot.count < kMaxStackSize)
        {
            ++slot.count;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.count == 0)
        {
            if (slot.equippedItem != EquippedItem::None)
            {
                continue;
            }
            slot.blockType = blockType;
            slot.count = 1;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }

    return false;
}

void refillHotbarSlotFromBag(HotbarSlots& hotbarSlots, BagSlots& bagSlots, const std::size_t hotbarIndex)
{
    InventorySlot& hotbarSlot = hotbarSlots[hotbarIndex];
    if (hotbarSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    for (InventorySlot& bagSlot : bagSlots)
    {
        if (bagSlot.count > 0 && bagSlot.blockType == hotbarSlot.blockType)
        {
            const std::uint32_t transferCount = std::min(kMaxStackSize, bagSlot.count);
            hotbarSlot.count = transferCount;
            hotbarSlot.equippedItem = EquippedItem::None;
            bagSlot.count -= transferCount;
            if (bagSlot.count == 0)
            {
                bagSlot.blockType = vibecraft::world::BlockType::Air;
            }
            return;
        }
    }

    for (InventorySlot& bagSlot : bagSlots)
    {
        if (bagSlot.count > 0)
        {
            const std::uint32_t transferCount = std::min(kMaxStackSize, bagSlot.count);
            hotbarSlot.blockType = bagSlot.blockType;
            hotbarSlot.count = transferCount;
            hotbarSlot.equippedItem = EquippedItem::None;
            bagSlot.count -= transferCount;
            if (bagSlot.count == 0)
            {
                bagSlot.blockType = vibecraft::world::BlockType::Air;
            }
            return;
        }
    }
}
}  // namespace

const char* blockTypeLabel(const vibecraft::world::BlockType blockType)
{
    switch (blockType)
    {
    case vibecraft::world::BlockType::Grass:
        return "Grass";
    case vibecraft::world::BlockType::Dirt:
        return "Dirt";
    case vibecraft::world::BlockType::Stone:
        return "Stone";
    case vibecraft::world::BlockType::Deepslate:
        return "Deepslate";
    case vibecraft::world::BlockType::CoalOre:
        return "Coal";
    case vibecraft::world::BlockType::Sand:
        return "Sand";
    case vibecraft::world::BlockType::Bedrock:
        return "Bedrock";
    case vibecraft::world::BlockType::Water:
        return "Water";
    case vibecraft::world::BlockType::IronOre:
        return "Iron";
    case vibecraft::world::BlockType::GoldOre:
        return "Gold";
    case vibecraft::world::BlockType::DiamondOre:
        return "Diamond";
    case vibecraft::world::BlockType::EmeraldOre:
        return "Emerald";
    case vibecraft::world::BlockType::Lava:
        return "Lava";
    case vibecraft::world::BlockType::TreeTrunk:
        return "Tree Trunk";
    case vibecraft::world::BlockType::TreeCrown:
        return "Tree Crown";
    case vibecraft::world::BlockType::OakPlanks:
        return "Oak Planks";
    case vibecraft::world::BlockType::CraftingTable:
        return "Crafting Table";
    case vibecraft::world::BlockType::Cobblestone:
        return "Cobblestone";
    case vibecraft::world::BlockType::Sandstone:
        return "Sandstone";
    case vibecraft::world::BlockType::Air:
    default:
        return "Empty";
    }
}

bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex)
{
    if (blockType == vibecraft::world::BlockType::Air || blockType == vibecraft::world::BlockType::Water
        || blockType == vibecraft::world::BlockType::Lava)
    {
        return false;
    }

    if (addToMatchingOrEmptySlots(hotbarSlots, blockType, &selectedHotbarIndex))
    {
        return true;
    }

    return addToMatchingOrEmptySlots(bagSlots, blockType, nullptr);
}

void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, const std::size_t selectedHotbarIndex)
{
    InventorySlot& selectedSlot = hotbarSlots[selectedHotbarIndex];
    if (selectedSlot.count == 0)
    {
        selectedSlot.blockType = vibecraft::world::BlockType::Air;
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
        return;
    }

    --selectedSlot.count;
    if (selectedSlot.count == 0)
    {
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
    }
}

}  // namespace vibecraft::app
