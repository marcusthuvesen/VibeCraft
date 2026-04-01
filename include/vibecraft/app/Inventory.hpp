#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
enum class EquippedItem : std::uint8_t
{
    None = 0,
    DiamondSword,
    Stick,
    RottenFlesh,
    Leather,
    RawPorkchop,
    Mutton,
    Feather,
    WoodSword,
    StoneSword,
    IronSword,
    GoldSword,
    WoodPickaxe,
    StonePickaxe,
    IronPickaxe,
    GoldPickaxe,
    DiamondPickaxe,
    WoodAxe,
    StoneAxe,
    IronAxe,
    GoldAxe,
    DiamondAxe,
    OxygenCanister,
    FieldTank,
    ExpeditionTank,
    Coal,
    StarterTank,
    ScoutHelmet,
    ScoutChestRig,
    ScoutGreaves,
    ScoutBoots,
};

struct InventorySlot
{
    vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
    std::uint32_t count = 0;
    EquippedItem equippedItem = EquippedItem::None;
};

enum class EquipmentSlotKind : std::uint8_t
{
    Helmet = 0,
    Chestplate,
    Leggings,
    Boots,
    OxygenTank,
};

constexpr std::size_t kHotbarSlotCount = 9;
constexpr std::size_t kBagSlotCount = 81;
constexpr std::size_t kEquipmentSlotCount = 5;
constexpr std::uint32_t kMaxStackSize = 64;

using HotbarSlots = std::array<InventorySlot, kHotbarSlotCount>;
using BagSlots = std::array<InventorySlot, kBagSlotCount>;
using EquipmentSlots = std::array<InventorySlot, kEquipmentSlotCount>;

[[nodiscard]] const char* blockTypeLabel(vibecraft::world::BlockType blockType);
[[nodiscard]] const char* equippedItemLabel(EquippedItem equippedItem);
[[nodiscard]] const char* equipmentSlotLabel(EquipmentSlotKind slotKind);
[[nodiscard]] std::string inventorySlotLabel(const InventorySlot& slot);
[[nodiscard]] float armorProtectionFractionForEquippedItem(EquippedItem equippedItem);
[[nodiscard]] bool canPlaceIntoEquipmentSlot(const InventorySlot& slot, EquipmentSlotKind slotKind);
[[nodiscard]] bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex);
void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, std::size_t selectedHotbarIndex);
void compactBagSlots(BagSlots& bagSlots);
}  // namespace vibecraft::app
