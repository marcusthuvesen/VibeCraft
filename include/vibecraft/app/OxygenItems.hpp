#pragma once

#include <string>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/OxygenSystem.hpp"

namespace vibecraft::app
{
struct PortableOxygenItemUseResult
{
    bool handled = false;
    bool consumeSlot = false;
    std::string notice;
};

[[nodiscard]] vibecraft::game::OxygenTankTier oxygenTankTierForUpgradeItem(EquippedItem equippedItem);
[[nodiscard]] EquippedItem equippedItemForOxygenTankTier(vibecraft::game::OxygenTankTier tankTier);
[[nodiscard]] PortableOxygenItemUseResult tryUsePortableOxygenItem(
    const InventorySlot& slot,
    vibecraft::game::OxygenSystem& oxygenSystem);
[[nodiscard]] std::string portableOxygenItemUseHint(
    const InventorySlot& slot,
    const vibecraft::game::OxygenState& oxygenState);
}  // namespace vibecraft::app
