#pragma once

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/MobTypes.hpp"

namespace vibecraft::app
{
[[nodiscard]] float meleeDamageForSlot(const InventorySlot& slot);
[[nodiscard]] float meleeReachForSlot(const InventorySlot& slot);
[[nodiscard]] float knockbackDistanceForSlot(const InventorySlot& slot);
[[nodiscard]] EquippedItem mobDropItemForKind(vibecraft::game::MobKind mobKind);
}  // namespace vibecraft::app
