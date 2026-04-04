#include "vibecraft/app/Furnace.hpp"

#include <algorithm>

#include "vibecraft/app/crafting/Crafting.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool isWoodLog(const world::BlockType blockType)
{
    return blockType == world::BlockType::OakLog || blockType == world::BlockType::JungleLog
        || blockType == world::BlockType::SpruceLog || blockType == world::BlockType::BirchLog
        || blockType == world::BlockType::DarkOakLog;
}

[[nodiscard]] bool isWoodBlockFuel(const world::BlockType blockType)
{
    if (world::isWoodDoorBlock(blockType))
    {
        return true;
    }

    return blockType == world::BlockType::OakPlanks || blockType == world::BlockType::JunglePlanks
        || blockType == world::BlockType::CraftingTable || blockType == world::BlockType::Chest
        || blockType == world::BlockType::Bookshelf || blockType == world::BlockType::Bamboo
        || blockType == world::BlockType::OakStairs || blockType == world::BlockType::OakStairsNorth
        || blockType == world::BlockType::OakStairsEast || blockType == world::BlockType::OakStairsSouth
        || blockType == world::BlockType::OakStairsWest
        || blockType == world::BlockType::JungleStairs || blockType == world::BlockType::JungleStairsNorth
        || blockType == world::BlockType::JungleStairsEast || blockType == world::BlockType::JungleStairsSouth
        || blockType == world::BlockType::JungleStairsWest
        || isWoodLog(blockType);
}

[[nodiscard]] bool isWoodToolFuel(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::WoodPickaxe:
    case EquippedItem::WoodAxe:
        return true;
    default:
        return false;
    }
}

void consumeOne(InventorySlot& slot)
{
    if (slot.count == 0)
    {
        return;
    }
    --slot.count;
    if (slot.count == 0)
    {
        clearInventorySlot(slot);
    }
}
}  // namespace

bool furnaceStateHasAnyContents(const FurnaceBlockState& state)
{
    return !isInventorySlotEmpty(state.inputSlot) || !isInventorySlotEmpty(state.fuelSlot)
        || !isInventorySlotEmpty(state.outputSlot) || state.fuelSecondsRemaining > 0.0f
        || state.smeltProgressSeconds > 0.0f;
}

bool isFurnaceFuel(const InventorySlot& slot)
{
    return furnaceFuelSeconds(slot) > 0.0f;
}

float furnaceFuelSeconds(const InventorySlot& slot)
{
    if (slot.count == 0)
    {
        return 0.0f;
    }
    if (slot.equippedItem == EquippedItem::Coal || slot.equippedItem == EquippedItem::Charcoal)
    {
        return kFurnaceSmeltSecondsPerItem * 8.0f;
    }
    if (slot.equippedItem == EquippedItem::Stick)
    {
        return kFurnaceSmeltSecondsPerItem * 0.5f;
    }
    if (isWoodToolFuel(slot.equippedItem))
    {
        return kFurnaceSmeltSecondsPerItem * 1.5f;
    }
    if (isWoodBlockFuel(slot.blockType))
    {
        return kFurnaceSmeltSecondsPerItem * 1.5f;
    }
    return 0.0f;
}

std::optional<InventorySlot> furnaceSmeltOutputForInput(const InventorySlot& inputSlot)
{
    if (inputSlot.count == 0 || inputSlot.equippedItem != EquippedItem::None)
    {
        return std::nullopt;
    }

    using BT = world::BlockType;
    switch (inputSlot.blockType)
    {
    case BT::Sand:
        return InventorySlot{.blockType = BT::Glass, .count = 1, .equippedItem = EquippedItem::None};
    case BT::Cobblestone:
        return InventorySlot{.blockType = BT::Stone, .count = 1, .equippedItem = EquippedItem::None};
    case BT::IronOre:
        return InventorySlot{.blockType = BT::Air, .count = 1, .equippedItem = EquippedItem::IronIngot};
    case BT::GoldOre:
        return InventorySlot{.blockType = BT::Air, .count = 1, .equippedItem = EquippedItem::GoldIngot};
    case BT::OakLog:
    case BT::JungleLog:
    case BT::SpruceLog:
    case BT::BirchLog:
    case BT::DarkOakLog:
        return InventorySlot{.blockType = BT::Air, .count = 1, .equippedItem = EquippedItem::Charcoal};
    default:
        return std::nullopt;
    }
}

bool canAcceptFurnaceInput(const InventorySlot& slot)
{
    return isInventorySlotEmpty(slot) || furnaceSmeltOutputForInput(slot).has_value();
}

bool canAcceptFurnaceFuel(const InventorySlot& slot)
{
    return isInventorySlotEmpty(slot) || isFurnaceFuel(slot);
}

bool canSmeltInFurnace(const FurnaceBlockState& state)
{
    const std::optional<InventorySlot> output = furnaceSmeltOutputForInput(state.inputSlot);
    if (!output.has_value())
    {
        return false;
    }
    if (isInventorySlotEmpty(state.outputSlot))
    {
        return true;
    }
    return canMergeInventorySlots(state.outputSlot, *output)
        && state.outputSlot.count < inventorySlotStackLimit(state.outputSlot);
}

float furnaceFuelFraction(const FurnaceBlockState& state)
{
    if (state.fuelSecondsCapacity <= 0.0f)
    {
        return 0.0f;
    }
    return std::clamp(state.fuelSecondsRemaining / state.fuelSecondsCapacity, 0.0f, 1.0f);
}

float furnaceSmeltFraction(const FurnaceBlockState& state)
{
    return std::clamp(state.smeltProgressSeconds / kFurnaceSmeltSecondsPerItem, 0.0f, 1.0f);
}

void tickFurnaceBlockState(FurnaceBlockState& state, const float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f)
    {
        return;
    }

    if (!canSmeltInFurnace(state))
    {
        state.smeltProgressSeconds = 0.0f;
    }

    if (state.fuelSecondsRemaining <= 0.0f && canSmeltInFurnace(state) && isFurnaceFuel(state.fuelSlot))
    {
        state.fuelSecondsCapacity = furnaceFuelSeconds(state.fuelSlot);
        state.fuelSecondsRemaining = state.fuelSecondsCapacity;
        consumeOne(state.fuelSlot);
    }

    if (state.fuelSecondsRemaining <= 0.0f)
    {
        return;
    }

    const float burningTime = std::min(deltaTimeSeconds, state.fuelSecondsRemaining);
    state.fuelSecondsRemaining = std::max(0.0f, state.fuelSecondsRemaining - deltaTimeSeconds);
    if (!canSmeltInFurnace(state))
    {
        state.smeltProgressSeconds = 0.0f;
        return;
    }
    state.smeltProgressSeconds += burningTime;

    while (state.smeltProgressSeconds >= kFurnaceSmeltSecondsPerItem)
    {
        const std::optional<InventorySlot> output = furnaceSmeltOutputForInput(state.inputSlot);
        if (!output.has_value() || !canSmeltInFurnace(state))
        {
            state.smeltProgressSeconds = 0.0f;
            return;
        }

        if (isInventorySlotEmpty(state.outputSlot))
        {
            state.outputSlot = *output;
        }
        else
        {
            ++state.outputSlot.count;
        }
        consumeOne(state.inputSlot);
        state.smeltProgressSeconds -= kFurnaceSmeltSecondsPerItem;
        if (!canSmeltInFurnace(state))
        {
            state.smeltProgressSeconds = 0.0f;
            return;
        }
    }
}
}  // namespace vibecraft::app
