#include <doctest/doctest.h>

#include "vibecraft/app/Furnace.hpp"
#include "vibecraft/app/crafting/Crafting.hpp"

TEST_CASE("furnace smelts ore and block recipes with supported fuel")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::app::FurnaceBlockState;
    using vibecraft::world::BlockType;

    FurnaceBlockState ironFurnace{};
    ironFurnace.inputSlot.blockType = BlockType::IronOre;
    ironFurnace.inputSlot.count = 2;
    ironFurnace.fuelSlot.equippedItem = EquippedItem::Coal;
    ironFurnace.fuelSlot.count = 1;
    vibecraft::app::tickFurnaceBlockState(ironFurnace, vibecraft::app::kFurnaceSmeltSecondsPerItem);
    REQUIRE(ironFurnace.outputSlot.equippedItem == EquippedItem::IronIngot);
    CHECK(ironFurnace.outputSlot.count == 1);
    CHECK(ironFurnace.inputSlot.count == 1);
    CHECK(ironFurnace.fuelSecondsRemaining > 0.0f);

    vibecraft::app::tickFurnaceBlockState(ironFurnace, vibecraft::app::kFurnaceSmeltSecondsPerItem);
    CHECK(ironFurnace.outputSlot.equippedItem == EquippedItem::IronIngot);
    CHECK(ironFurnace.outputSlot.count == 2);
    CHECK(vibecraft::app::isInventorySlotEmpty(ironFurnace.inputSlot));

    FurnaceBlockState glassFurnace{};
    glassFurnace.inputSlot.blockType = BlockType::Sand;
    glassFurnace.inputSlot.count = 1;
    glassFurnace.fuelSlot.blockType = BlockType::OakPlanks;
    glassFurnace.fuelSlot.count = 1;
    vibecraft::app::tickFurnaceBlockState(glassFurnace, vibecraft::app::kFurnaceSmeltSecondsPerItem);
    CHECK(glassFurnace.outputSlot.blockType == BlockType::Glass);
    CHECK(glassFurnace.outputSlot.count == 1);

    FurnaceBlockState charcoalFurnace{};
    charcoalFurnace.inputSlot.blockType = BlockType::BirchLog;
    charcoalFurnace.inputSlot.count = 1;
    charcoalFurnace.fuelSlot.blockType = BlockType::OakPlanks;
    charcoalFurnace.fuelSlot.count = 1;
    vibecraft::app::tickFurnaceBlockState(charcoalFurnace, vibecraft::app::kFurnaceSmeltSecondsPerItem);
    CHECK(charcoalFurnace.outputSlot.equippedItem == EquippedItem::Charcoal);
    CHECK(charcoalFurnace.outputSlot.count == 1);
}

TEST_CASE("charcoal can craft torches")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots torchGrid{};
    torchGrid[0].equippedItem = EquippedItem::Charcoal;
    torchGrid[0].count = 1;
    torchGrid[3].equippedItem = EquippedItem::Stick;
    torchGrid[3].count = 1;
    const auto torchMatch = vibecraft::app::evaluateCraftingGrid(torchGrid, CraftingMode::Inventory2x2);
    REQUIRE(torchMatch.has_value());
    CHECK(torchMatch->output.blockType == BlockType::Torch);
    CHECK(torchMatch->output.count == 4);
}
