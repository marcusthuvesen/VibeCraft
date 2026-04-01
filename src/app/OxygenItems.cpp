#include "vibecraft/app/OxygenItems.hpp"

#include <algorithm>
#include <cstdint>

#include <fmt/format.h>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool isOxygenCanisterItem(const EquippedItem equippedItem)
{
    return equippedItem == EquippedItem::OxygenCanister;
}
}  // namespace

vibecraft::game::OxygenTankTier oxygenTankTierForUpgradeItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::StarterTank:
        return vibecraft::game::OxygenTankTier::Starter;
    case EquippedItem::FieldTank:
        return vibecraft::game::OxygenTankTier::Field;
    case EquippedItem::ExpeditionTank:
        return vibecraft::game::OxygenTankTier::Expedition;
    default:
        return vibecraft::game::OxygenTankTier::None;
    }
}

EquippedItem equippedItemForOxygenTankTier(const vibecraft::game::OxygenTankTier tankTier)
{
    switch (tankTier)
    {
    case vibecraft::game::OxygenTankTier::Starter:
        return EquippedItem::StarterTank;
    case vibecraft::game::OxygenTankTier::Field:
        return EquippedItem::FieldTank;
    case vibecraft::game::OxygenTankTier::Expedition:
        return EquippedItem::ExpeditionTank;
    case vibecraft::game::OxygenTankTier::None:
    default:
        return EquippedItem::None;
    }
}

PortableOxygenItemUseResult tryUsePortableOxygenItem(
    const InventorySlot& slot,
    vibecraft::game::OxygenSystem& oxygenSystem)
{
    if (slot.count == 0)
    {
        return {};
    }

    const vibecraft::game::OxygenTankTier tankUpgradeTier = oxygenTankTierForUpgradeItem(slot.equippedItem);
    const auto currentTierByte = static_cast<std::uint8_t>(oxygenSystem.state().tankTier);
    const auto upgradeTierByte = static_cast<std::uint8_t>(tankUpgradeTier);
    if (tankUpgradeTier != vibecraft::game::OxygenTankTier::None)
    {
        if (upgradeTierByte > currentTierByte)
        {
            oxygenSystem.setTankTier(tankUpgradeTier, true);
            return {
                .handled = true,
                .consumeSlot = true,
                .notice = fmt::format("{} tank equipped.", vibecraft::game::oxygenTankTierName(tankUpgradeTier)),
            };
        }

        const char* const tankName = vibecraft::game::oxygenTankTierName(tankUpgradeTier);
        return {
            .handled = true,
            .consumeSlot = false,
            .notice = upgradeTierByte == currentTierByte
                ? fmt::format("{} tank already installed.", tankName)
                : fmt::format("{} tank is below your current tier.", tankName),
        };
    }

    if (!isOxygenCanisterItem(slot.equippedItem))
    {
        return {};
    }

    const float oxygenBefore = oxygenSystem.state().oxygen;
    static_cast<void>(oxygenSystem.refill(std::max(50.0f, oxygenSystem.state().capacity * 0.45f)));
    if (oxygenSystem.state().oxygen > oxygenBefore + 0.001f)
    {
        return {
            .handled = true,
            .consumeSlot = true,
            .notice = "Used oxygen canister.",
        };
    }

    return {
        .handled = true,
        .consumeSlot = false,
        .notice = "Oxygen tank already full.",
    };
}

std::string portableOxygenItemUseHint(
    const InventorySlot& slot,
    const vibecraft::game::OxygenState& oxygenState)
{
    if (slot.count == 0)
    {
        return {};
    }

    const vibecraft::game::OxygenTankTier tankUpgradeTier = oxygenTankTierForUpgradeItem(slot.equippedItem);
    const auto currentTierByte = static_cast<std::uint8_t>(oxygenState.tankTier);
    const auto upgradeTierByte = static_cast<std::uint8_t>(tankUpgradeTier);
    if (tankUpgradeTier != vibecraft::game::OxygenTankTier::None)
    {
        if (upgradeTierByte > currentTierByte)
        {
            return fmt::format("Right-click: equip {} tank", vibecraft::game::oxygenTankTierName(tankUpgradeTier));
        }
        if (upgradeTierByte == currentTierByte)
        {
            return "Right-click: already installed";
        }
        return "Right-click: lower than equipped tank";
    }

    if (isOxygenCanisterItem(slot.equippedItem))
    {
        return oxygenState.oxygen + 0.001f < oxygenState.capacity
            ? "Right-click: refill oxygen"
            : "Right-click: tank already full";
    }

    return {};
}
}  // namespace vibecraft::app
