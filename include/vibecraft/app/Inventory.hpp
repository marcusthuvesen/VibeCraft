#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
enum class InventorySelectionBehavior : std::uint8_t
{
    PreserveCurrent,
    SelectAffectedHotbarSlot,
};

// Explicit values preserve stable uint8 network and save payloads for equipped items.
enum class EquippedItem : std::uint8_t
{
    None = 0,
    DiamondSword = 1,
    Stick = 2,
    RottenFlesh = 3,
    Leather = 4,
    RawPorkchop = 5,
    Mutton = 6,
    Feather = 7,
    WoodSword = 8,
    StoneSword = 9,
    IronSword = 10,
    GoldSword = 11,
    WoodPickaxe = 12,
    StonePickaxe = 13,
    IronPickaxe = 14,
    GoldPickaxe = 15,
    DiamondPickaxe = 16,
    WoodAxe = 17,
    StoneAxe = 18,
    IronAxe = 19,
    GoldAxe = 20,
    DiamondAxe = 21,
    Coal = 25,
    Charcoal = 26,
    ScoutHelmet = 27,
    ScoutChestRig = 28,
    ScoutGreaves = 29,
    ScoutBoots = 30,
    IronIngot = 31,
    GoldIngot = 32,
};

struct InventorySlot
{
    vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
    std::uint32_t count = 0;
    EquippedItem equippedItem = EquippedItem::None;
    /// Remaining durability for damageable equipped items; zero for non-damageable stacks.
    std::uint16_t durabilityRemaining = 0;
};

enum class EquipmentSlotKind : std::uint8_t
{
    Helmet = 0,
    Chestplate,
    Leggings,
    Boots,
};

constexpr std::size_t kHotbarSlotCount = 9;
constexpr std::size_t kBagSlotCount = 81;
constexpr std::size_t kEquipmentSlotCount = 4;
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
[[nodiscard]] bool isDamageableEquippedItem(EquippedItem equippedItem);
[[nodiscard]] std::uint16_t maxDurabilityForEquippedItem(EquippedItem equippedItem);
[[nodiscard]] std::uint32_t inventorySlotStackLimit(const InventorySlot& slot);
/// Decrements durability for damageable equipped items. Returns true if item broke and was removed.
[[nodiscard]] bool consumeEquippedItemDurability(InventorySlot& slot, std::uint16_t amount = 1);
[[nodiscard]] bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex,
    InventorySelectionBehavior selectionBehavior = InventorySelectionBehavior::PreserveCurrent);
[[nodiscard]] bool addEquippedItemToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    EquippedItem equippedItem,
    std::size_t& selectedHotbarIndex,
    InventorySelectionBehavior selectionBehavior = InventorySelectionBehavior::PreserveCurrent);
void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, std::size_t selectedHotbarIndex);
void compactBagSlots(BagSlots& bagSlots);
}  // namespace vibecraft::app
