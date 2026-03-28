#include "vibecraft/app/Inventory.hpp"

#include <algorithm>
#include <fmt/format.h>

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

    for (InventorySlot& bagSlot : bagSlots)
    {
        if (bagSlot.count > 0 && bagSlot.blockType == hotbarSlot.blockType)
        {
            const std::uint32_t transferCount = std::min(kMaxStackSize, bagSlot.count);
            hotbarSlot.count = transferCount;
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
    case vibecraft::world::BlockType::Water:
        return "Water";
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
    if (blockType == vibecraft::world::BlockType::Air || blockType == vibecraft::world::BlockType::Water)
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

std::string formatBagLine(const BagSlots& bagSlots)
{
    std::uint32_t usedSlots = 0;
    std::uint32_t totalItems = 0;

    for (const InventorySlot& slot : bagSlots)
    {
        if (slot.count > 0)
        {
            ++usedSlots;
            totalItems += slot.count;
        }
    }

    std::string sampleEntries;
    std::size_t shown = 0;
    constexpr std::size_t kSampleLimit = 3;
    for (const InventorySlot& slot : bagSlots)
    {
        if (slot.count == 0)
        {
            continue;
        }

        if (!sampleEntries.empty())
        {
            sampleEntries += ", ";
        }
        sampleEntries += fmt::format("{}x{}", blockTypeLabel(slot.blockType), slot.count);
        ++shown;
        if (shown >= kSampleLimit)
        {
            break;
        }
    }

    if (sampleEntries.empty())
    {
        sampleEntries = "empty";
    }

    return fmt::format(
        "Bag {}/{} slots, items:{} ({})",
        usedSlots,
        bagSlots.size(),
        totalItems,
        sampleEntries);
}
}  // namespace vibecraft::app
