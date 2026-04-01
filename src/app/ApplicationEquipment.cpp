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
        return EquipmentSlotKind::Boots;
    case 4:
    default:
        return EquipmentSlotKind::OxygenTank;
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

void syncOxygenEquipmentSlotFromSystem(
    EquipmentSlots& equipmentSlots,
    const vibecraft::game::OxygenSystem& oxygenSystem)
{
    InventorySlot& oxygenSlot = equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::OxygenTank)];
    const EquippedItem oxygenTankItem = equippedItemForOxygenTankTier(oxygenSystem.state().tankTier);
    if (oxygenTankItem == EquippedItem::None)
    {
        clearInventorySlot(oxygenSlot);
        return;
    }

    oxygenSlot.blockType = vibecraft::world::BlockType::Air;
    oxygenSlot.equippedItem = oxygenTankItem;
    oxygenSlot.count = 1;
}

void syncOxygenSystemFromEquipmentSlot(
    const EquipmentSlots& equipmentSlots,
    vibecraft::game::OxygenSystem& oxygenSystem,
    const bool refillToCapacity)
{
    const InventorySlot& oxygenSlot = equipmentSlots[equipmentSlotIndex(EquipmentSlotKind::OxygenTank)];
    vibecraft::game::OxygenTankTier tankTier = oxygenTankTierForUpgradeItem(oxygenSlot.equippedItem);
    if (tankTier == vibecraft::game::OxygenTankTier::None)
    {
        tankTier = vibecraft::game::OxygenTankTier::Starter;
    }
    oxygenSystem.setTankTier(tankTier, refillToCapacity);
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
