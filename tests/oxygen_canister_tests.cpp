#include <doctest/doctest.h>

#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/Crafting.hpp"
#include "vibecraft/app/OxygenItems.hpp"
#include "vibecraft/game/OxygenSystem.hpp"

TEST_CASE("oxygen canister recipe crafts portable refill items")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots grid{};
    grid[0] = {.blockType = BlockType::Glass, .count = 1};
    grid[3] = {.blockType = BlockType::Glowstone, .count = 1};

    const auto match = vibecraft::app::evaluateCraftingGrid(grid, CraftingMode::Inventory2x2);
    REQUIRE(match.has_value());
    CHECK(match->output.equippedItem == EquippedItem::OxygenCanister);
    CHECK(match->output.count == 2);
    CHECK(match->output.blockType == BlockType::Air);
}

TEST_CASE("field tank recipe upgrades the starter oxygen setup")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots grid{};
    grid[0] = {.blockType = BlockType::Glass, .count = 1};
    grid[1] = {.blockType = BlockType::IronOre, .count = 1};
    grid[3] = {.equippedItem = EquippedItem::OxygenCanister, .count = 1};
    grid[4] = {.blockType = BlockType::MossBlock, .count = 1};

    const auto match = vibecraft::app::evaluateCraftingGrid(grid, CraftingMode::Inventory2x2);
    REQUIRE(match.has_value());
    CHECK(match->output.equippedItem == EquippedItem::FieldTank);
    CHECK(match->output.count == 1);
    CHECK(match->output.blockType == BlockType::Air);
}

TEST_CASE("expedition tank recipe builds on the field tank tier")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots grid{};
    grid[0] = {.blockType = BlockType::Glass, .count = 1};
    grid[1] = {.blockType = BlockType::DiamondOre, .count = 1};
    grid[2] = {.blockType = BlockType::Glass, .count = 1};
    grid[3] = {.blockType = BlockType::IronOre, .count = 1};
    grid[4] = {.equippedItem = EquippedItem::FieldTank, .count = 1};
    grid[5] = {.blockType = BlockType::IronOre, .count = 1};
    grid[6] = {.blockType = BlockType::Glowstone, .count = 1};
    grid[7] = {.blockType = BlockType::MossBlock, .count = 1};
    grid[8] = {.blockType = BlockType::Glowstone, .count = 1};

    const auto match = vibecraft::app::evaluateCraftingGrid(grid, CraftingMode::Workbench3x3);
    REQUIRE(match.has_value());
    CHECK(match->output.equippedItem == EquippedItem::ExpeditionTank);
    CHECK(match->output.count == 1);
    CHECK(match->output.blockType == BlockType::Air);
}

TEST_CASE("portable oxygen items report usable hints and outcomes")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::app::InventorySlot;

    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Starter, 0.3f);

    const InventorySlot canisterSlot{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::OxygenCanister,
    };
    CHECK(vibecraft::app::portableOxygenItemUseHint(canisterSlot, oxygenSystem.state()) == "Right-click: refill oxygen");
    const auto canisterUse = vibecraft::app::tryUsePortableOxygenItem(canisterSlot, oxygenSystem);
    CHECK(canisterUse.handled);
    CHECK(canisterUse.consumeSlot);
    CHECK(canisterUse.notice == "Used oxygen canister.");

    const InventorySlot fieldTankSlot{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::FieldTank,
    };
    CHECK(vibecraft::app::portableOxygenItemUseHint(fieldTankSlot, oxygenSystem.state()) == "Right-click: equip field tank");
    const auto fieldTankUse = vibecraft::app::tryUsePortableOxygenItem(fieldTankSlot, oxygenSystem);
    CHECK(fieldTankUse.handled);
    CHECK(fieldTankUse.consumeSlot);
    CHECK(fieldTankUse.notice == "field tank equipped.");
    CHECK(oxygenSystem.state().tankTier == vibecraft::game::OxygenTankTier::Field);

    const auto repeatedFieldTankUse = vibecraft::app::tryUsePortableOxygenItem(fieldTankSlot, oxygenSystem);
    CHECK(repeatedFieldTankUse.handled);
    CHECK_FALSE(repeatedFieldTankUse.consumeSlot);
    CHECK(repeatedFieldTankUse.notice == "field tank already installed.");
}

TEST_CASE("oxygen equipment slot mirrors the installed oxygen tank")
{
    vibecraft::app::EquipmentSlots equipmentSlots{};
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Starter, 1.0f);

    vibecraft::app::syncOxygenEquipmentSlotFromSystem(equipmentSlots, oxygenSystem);
    CHECK(
        equipmentSlots[static_cast<std::size_t>(vibecraft::app::EquipmentSlotKind::OxygenTank)].equippedItem
        == vibecraft::app::EquippedItem::StarterTank);

    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Expedition, 0.5f);

    vibecraft::app::syncOxygenEquipmentSlotFromSystem(equipmentSlots, oxygenSystem);
    const auto& oxygenSlot =
        equipmentSlots[static_cast<std::size_t>(vibecraft::app::EquipmentSlotKind::OxygenTank)];
    CHECK(oxygenSlot.equippedItem == vibecraft::app::EquippedItem::ExpeditionTank);
    CHECK(oxygenSlot.count == 1);

    vibecraft::app::clearInventorySlot(equipmentSlots[static_cast<std::size_t>(vibecraft::app::EquipmentSlotKind::OxygenTank)]);
    vibecraft::app::syncOxygenSystemFromEquipmentSlot(equipmentSlots, oxygenSystem, false);
    CHECK(oxygenSystem.state().tankTier == vibecraft::game::OxygenTankTier::Starter);
}
