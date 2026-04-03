#include "vibecraft/app/ApplicationEquipment.hpp"

#include <algorithm>
#include <utility>

namespace vibecraft::app
{
EquipmentSlotKind equipmentSlotKindForIndex(const std::size_t index)
{
    switch (index)
    {
    case 0:
        return EquipmentSlotKind::Helmet;
    case 1:
        return EquipmentSlotKind::Chestplate;
    case 2:
        return EquipmentSlotKind::Leggings;
    case 3:
    default:
        return EquipmentSlotKind::Boots;
    }
}

std::size_t equipmentSlotIndex(const EquipmentSlotKind slotKind)
{
    return static_cast<std::size_t>(slotKind);
}

float equippedArmorProtectionFraction(const EquipmentSlots& equipmentSlots)
{
    float totalProtection = 0.0f;
    totalProtection += armorProtectionFractionForEquippedItem(
        equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::Helmet)].equippedItem);
    totalProtection += armorProtectionFractionForEquippedItem(
        equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::Chestplate)].equippedItem);
    totalProtection += armorProtectionFractionForEquippedItem(
        equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::Leggings)].equippedItem);
    totalProtection += armorProtectionFractionForEquippedItem(
        equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::Boots)].equippedItem);
    return std::clamp(totalProtection, 0.0f, 0.8f);
}

void mergeOrSwapEquipmentSlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const EquipmentSlotKind slotKind)
{
    if (isInventorySlotEmpty(carriedSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }
    if (!canPlaceIntoEquipmentSlot(carriedSlot, slotKind))
    {
        return;
    }
    if (isInventorySlotEmpty(targetSlot))
    {
        targetSlot = carriedSlot;
        clearInventorySlot(carriedSlot);
        return;
    }

    std::swap(carriedSlot, targetSlot);
    if (!isInventorySlotEmpty(carriedSlot) && !canPlaceIntoEquipmentSlot(carriedSlot, slotKind))
    {
        std::swap(carriedSlot, targetSlot);
    }
}

void rightClickEquipmentSlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const EquipmentSlotKind slotKind)
{
    if (isInventorySlotEmpty(carriedSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }
    if (!canPlaceIntoEquipmentSlot(carriedSlot, slotKind))
    {
        return;
    }
    if (isInventorySlotEmpty(targetSlot))
    {
        targetSlot.blockType = carriedSlot.blockType;
        targetSlot.equippedItem = carriedSlot.equippedItem;
        targetSlot.count = 1;
        --carriedSlot.count;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
        return;
    }

    std::swap(carriedSlot, targetSlot);
    if (!isInventorySlotEmpty(carriedSlot) && !canPlaceIntoEquipmentSlot(carriedSlot, slotKind))
    {
        std::swap(carriedSlot, targetSlot);
    }
}
}  // namespace vibecraft::app
