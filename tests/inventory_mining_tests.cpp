#include <doctest/doctest.h>

#include <set>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/app/Mining.hpp"

namespace
{
using vibecraft::app::BagSlots;
using vibecraft::app::EquippedItem;
using vibecraft::app::HotbarSlots;
using vibecraft::app::InventorySelectionBehavior;
using vibecraft::app::addBlockToInventory;
using vibecraft::app::addEquippedItemToInventory;
using vibecraft::app::applyCreativeInventoryLoadout;
using vibecraft::app::consumeEquippedItemDurability;
using vibecraft::app::durabilityUseAmountForEquippedItem;
using vibecraft::app::isDamageableEquippedItem;
using vibecraft::app::maxDurabilityForEquippedItem;
using vibecraft::app::miningDurationSeconds;
using vibecraft::world::BlockType;
}  // namespace

TEST_CASE("inventory pickup can preserve the selected hotbar slot")
{
    HotbarSlots hotbarSlots{};
    BagSlots bagSlots{};
    hotbarSlots[4] = {
        .blockType = BlockType::Cobblestone,
        .count = 12,
        .equippedItem = EquippedItem::None,
    };

    std::size_t selectedHotbarIndex = 4;

    CHECK(addBlockToInventory(
        hotbarSlots,
        bagSlots,
        BlockType::Stone,
        selectedHotbarIndex,
        InventorySelectionBehavior::PreserveCurrent));
    CHECK(selectedHotbarIndex == 4);
    CHECK(hotbarSlots[0].blockType == BlockType::Stone);
    CHECK(hotbarSlots[0].count == 1);

    CHECK(addEquippedItemToInventory(
        hotbarSlots,
        bagSlots,
        EquippedItem::Coal,
        selectedHotbarIndex,
        InventorySelectionBehavior::PreserveCurrent));
    CHECK(selectedHotbarIndex == 4);
    CHECK(hotbarSlots[1].equippedItem == EquippedItem::Coal);
    CHECK(hotbarSlots[1].count == 1);
}

TEST_CASE("creative inventory loadout exposes each normalized placeable block once")
{
    HotbarSlots hotbarSlots{};
    BagSlots bagSlots{};
    std::size_t selectedHotbarIndex = 5;

    applyCreativeInventoryLoadout(hotbarSlots, bagSlots, selectedHotbarIndex);

    CHECK(selectedHotbarIndex == 0);

    std::set<BlockType> expectedBlocks;
    for (std::uint16_t rawBlockIndex = 1;
         rawBlockIndex <= static_cast<std::uint16_t>(BlockType::IronDoorUpperWestOpen);
         ++rawBlockIndex)
    {
        const BlockType normalizedBlock =
            vibecraft::world::normalizePlaceVariantBlockType(static_cast<BlockType>(rawBlockIndex));
        if (normalizedBlock != BlockType::Air)
        {
            expectedBlocks.insert(normalizedBlock);
        }
    }

    std::set<BlockType> actualBlocks;
    const auto collectBlocks = [&actualBlocks](const auto& slots)
    {
        for (const vibecraft::app::InventorySlot& slot : slots)
        {
            if (slot.count == 0)
            {
                continue;
            }

            CHECK(slot.blockType != BlockType::Air);
            CHECK(slot.equippedItem == EquippedItem::None);
            CHECK(slot.count == vibecraft::app::kMaxStackSize);
            actualBlocks.insert(slot.blockType);
        }
    };

    collectBlocks(hotbarSlots);
    collectBlocks(bagSlots);

    CHECK(actualBlocks == expectedBlocks);
    CHECK(actualBlocks.contains(BlockType::Torch));
    CHECK(actualBlocks.contains(BlockType::Furnace));
    CHECK(actualBlocks.contains(BlockType::OakDoor));
    CHECK(actualBlocks.contains(BlockType::IronDoor));
    CHECK_FALSE(actualBlocks.contains(BlockType::TorchNorth));
    CHECK_FALSE(actualBlocks.contains(BlockType::FurnaceEast));
    CHECK_FALSE(actualBlocks.contains(BlockType::OakDoorUpperNorth));
}

TEST_CASE("pickaxe mining speed scales by pickaxe material for stone and ore")
{
    const float handStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::None);
    const float woodStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::WoodPickaxe);
    const float stoneStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::StonePickaxe);
    const float ironStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::IronPickaxe);
    const float diamondStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::DiamondPickaxe);
    const float goldStone = miningDurationSeconds(BlockType::Stone, BlockType::Air, EquippedItem::GoldPickaxe);

    CHECK(handStone > woodStone);
    CHECK(woodStone > stoneStone);
    CHECK(stoneStone > ironStone);
    CHECK(ironStone > diamondStone);
    CHECK(diamondStone > goldStone);

    const float handOre = miningDurationSeconds(BlockType::DiamondOre, BlockType::Air, EquippedItem::None);
    const float woodOre = miningDurationSeconds(BlockType::DiamondOre, BlockType::Air, EquippedItem::WoodPickaxe);
    const float stoneOre = miningDurationSeconds(BlockType::DiamondOre, BlockType::Air, EquippedItem::StonePickaxe);
    const float ironOre = miningDurationSeconds(BlockType::DiamondOre, BlockType::Air, EquippedItem::IronPickaxe);
    const float diamondOre = miningDurationSeconds(BlockType::DiamondOre, BlockType::Air, EquippedItem::DiamondPickaxe);

    CHECK(handOre > woodOre);
    CHECK(woodOre > stoneOre);
    CHECK(stoneOre > ironOre);
    CHECK(ironOre > diamondOre);
}

TEST_CASE("damageable tools are non-stackable and start with minecraft durability")
{
    HotbarSlots hotbarSlots{};
    BagSlots bagSlots{};
    std::size_t selectedHotbarIndex = 0;

    CHECK(isDamageableEquippedItem(EquippedItem::WoodPickaxe));
    CHECK(maxDurabilityForEquippedItem(EquippedItem::WoodPickaxe) == 59);
    CHECK(maxDurabilityForEquippedItem(EquippedItem::StonePickaxe) == 131);
    CHECK(maxDurabilityForEquippedItem(EquippedItem::IronPickaxe) == 250);
    CHECK(maxDurabilityForEquippedItem(EquippedItem::GoldPickaxe) == 32);
    CHECK(maxDurabilityForEquippedItem(EquippedItem::DiamondPickaxe) == 1561);

    CHECK(addEquippedItemToInventory(
        hotbarSlots,
        bagSlots,
        EquippedItem::WoodPickaxe,
        selectedHotbarIndex,
        InventorySelectionBehavior::PreserveCurrent));
    CHECK(addEquippedItemToInventory(
        hotbarSlots,
        bagSlots,
        EquippedItem::WoodPickaxe,
        selectedHotbarIndex,
        InventorySelectionBehavior::PreserveCurrent));

    CHECK(hotbarSlots[0].equippedItem == EquippedItem::WoodPickaxe);
    CHECK(hotbarSlots[0].count == 1);
    CHECK(hotbarSlots[0].durabilityRemaining == 59);
    CHECK(hotbarSlots[1].equippedItem == EquippedItem::WoodPickaxe);
    CHECK(hotbarSlots[1].count == 1);
    CHECK(hotbarSlots[1].durabilityRemaining == 59);
}

TEST_CASE("durability decreases on use and breaks tools")
{
    vibecraft::app::InventorySlot slot{
        .blockType = BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::GoldAxe,
        .durabilityRemaining = 2,
    };

    CHECK_FALSE(consumeEquippedItemDurability(slot));
    CHECK(slot.count == 1);
    CHECK(slot.equippedItem == EquippedItem::GoldAxe);
    CHECK(slot.durabilityRemaining == 1);

    CHECK(consumeEquippedItemDurability(slot));
    CHECK(slot.count == 0);
    CHECK(slot.equippedItem == EquippedItem::None);
    CHECK(slot.blockType == BlockType::Air);
}

TEST_CASE("sword durability wear scales by material")
{
    CHECK(durabilityUseAmountForEquippedItem(EquippedItem::DiamondSword) == 1);
    CHECK(durabilityUseAmountForEquippedItem(EquippedItem::IronSword) == 2);
    CHECK(durabilityUseAmountForEquippedItem(EquippedItem::StoneSword) == 3);
    CHECK(durabilityUseAmountForEquippedItem(EquippedItem::WoodSword) == 4);
    CHECK(durabilityUseAmountForEquippedItem(EquippedItem::GoldSword) == 5);

    vibecraft::app::InventorySlot goldSword{
        .blockType = BlockType::Air,
        .count = 1,
        .equippedItem = EquippedItem::GoldSword,
        .durabilityRemaining = 6,
    };
    CHECK_FALSE(consumeEquippedItemDurability(
        goldSword,
        durabilityUseAmountForEquippedItem(goldSword.equippedItem)));
    CHECK(goldSword.durabilityRemaining == 1);
}
