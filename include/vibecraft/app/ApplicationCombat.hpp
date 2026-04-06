#pragma once

#include "vibecraft/app/Inventory.hpp"

namespace vibecraft::app
{
[[nodiscard]] float meleeDamageForSlot(const InventorySlot& slot);
[[nodiscard]] float meleeReachForSlot(const InventorySlot& slot);
[[nodiscard]] float knockbackDistanceForSlot(const InventorySlot& slot);
}  // namespace vibecraft::app
