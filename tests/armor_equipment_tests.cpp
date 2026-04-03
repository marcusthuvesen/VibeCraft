#include <doctest/doctest.h>

#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/crafting/Crafting.hpp"

TEST_CASE("scout armor recipes craft each wearable piece")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;

    const vibecraft::app::InventorySlot hide{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::Leather,
    };

    CraftingGridSlots helmet{};
    helmet[0] = hide;
    helmet[1] = hide;
    helmet[2] = hide;
    helmet[3] = hide;
    helmet[5] = hide;
    const auto helmetMatch = vibecraft::app::evaluateCraftingGrid(helmet, CraftingMode::Workbench3x3);
    REQUIRE(helmetMatch.has_value());
    CHECK(helmetMatch->output.equippedItem == EquippedItem::ScoutHelmet);

    CraftingGridSlots chest{};
    chest[0] = hide;
    chest[2] = hide;
    chest[3] = hide;
    chest[4] = hide;
    chest[5] = hide;
    chest[6] = hide;
    chest[7] = hide;
    chest[8] = hide;
    const auto chestMatch = vibecraft::app::evaluateCraftingGrid(chest, CraftingMode::Workbench3x3);
    REQUIRE(chestMatch.has_value());
    CHECK(chestMatch->output.equippedItem == EquippedItem::ScoutChestRig);

    CraftingGridSlots legs{};
    legs[0] = hide;
    legs[1] = hide;
    legs[2] = hide;
    legs[3] = hide;
    legs[5] = hide;
    legs[6] = hide;
    legs[8] = hide;
    const auto legsMatch = vibecraft::app::evaluateCraftingGrid(legs, CraftingMode::Workbench3x3);
    REQUIRE(legsMatch.has_value());
    CHECK(legsMatch->output.equippedItem == EquippedItem::ScoutGreaves);

    CraftingGridSlots boots{};
    boots[0] = hide;
    boots[2] = hide;
    boots[3] = hide;
    boots[5] = hide;
    const auto bootsMatch = vibecraft::app::evaluateCraftingGrid(boots, CraftingMode::Workbench3x3);
    REQUIRE(bootsMatch.has_value());
    CHECK(bootsMatch->output.equippedItem == EquippedItem::ScoutBoots);
}

TEST_CASE("armor slots only accept their matching scout piece")
{
    using vibecraft::app::EquipmentSlotKind;
    using vibecraft::app::EquippedItem;
    using vibecraft::app::InventorySlot;

    const InventorySlot helmet{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutHelmet,
    };
    const InventorySlot chest{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutChestRig,
    };
    const InventorySlot tank{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::StarterTank,
    };

    CHECK(vibecraft::app::canPlaceIntoEquipmentSlot(helmet, EquipmentSlotKind::Helmet));
    CHECK_FALSE(vibecraft::app::canPlaceIntoEquipmentSlot(helmet, EquipmentSlotKind::Chestplate));
    CHECK(vibecraft::app::canPlaceIntoEquipmentSlot(chest, EquipmentSlotKind::Chestplate));
    CHECK_FALSE(vibecraft::app::canPlaceIntoEquipmentSlot(chest, EquipmentSlotKind::Boots));
    CHECK_FALSE(vibecraft::app::canPlaceIntoEquipmentSlot(tank, EquipmentSlotKind::Helmet));
}

TEST_CASE("equipped scout armor reduces hostile damage")
{
    using vibecraft::app::EquipmentSlotKind;
    using vibecraft::app::EquipmentSlots;
    using vibecraft::app::EquippedItem;

    EquipmentSlots equipmentSlots{};
    equipmentSlots[static_cast<std::size_t>(EquipmentSlotKind::Helmet)] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutHelmet,
    };
    equipmentSlots[static_cast<std::size_t>(EquipmentSlotKind::Chestplate)] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutChestRig,
    };
    equipmentSlots[static_cast<std::size_t>(EquipmentSlotKind::Leggings)] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutGreaves,
    };
    equipmentSlots[static_cast<std::size_t>(EquipmentSlotKind::Boots)] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::ScoutBoots,
    };

    CHECK(vibecraft::app::equippedArmorProtectionFraction(equipmentSlots) == doctest::Approx(0.28f));
    CHECK(1.0f - vibecraft::app::equippedArmorProtectionFraction(equipmentSlots) == doctest::Approx(0.72f));
}
