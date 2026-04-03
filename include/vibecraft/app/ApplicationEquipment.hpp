#pragma once

#include <cstddef>

#include "vibecraft/app/crafting/Crafting.hpp"
#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/app/OxygenItems.hpp"
#include "vibecraft/game/OxygenSystem.hpp"

namespace vibecraft::app
{
[[nodiscard]] EquipmentSlotKind equipmentSlotKindForIndex(std::size_t index);
[[nodiscard]] std::size_t equipmentSlotIndex(EquipmentSlotKind slotKind);
[[nodiscard]] float equippedArmorProtectionFraction(const EquipmentSlots& equipmentSlots);
void syncOxygenEquipmentSlotFromSystem(EquipmentSlots& equipmentSlots, const vibecraft::game::OxygenSystem& oxygenSystem);
void syncOxygenSystemFromEquipmentSlot(
    const EquipmentSlots& equipmentSlots,
    vibecraft::game::OxygenSystem& oxygenSystem,
    bool refillToCapacity);
void mergeOrSwapEquipmentSlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    EquipmentSlotKind slotKind);
void rightClickEquipmentSlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    EquipmentSlotKind slotKind);
}  // namespace vibecraft::app
