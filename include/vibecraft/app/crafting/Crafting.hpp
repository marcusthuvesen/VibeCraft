#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "vibecraft/app/Inventory.hpp"

namespace vibecraft::app
{
enum class CraftingMode : std::uint8_t
{
    Inventory2x2 = 0,
    Workbench3x3,
};

using CraftingGridSlots = std::array<InventorySlot, 27>;

struct CraftingMatch
{
    InventorySlot output{};
    std::array<std::size_t, 9> consumedSlotIndices{};
    std::size_t consumedSlotCount = 0;
};

[[nodiscard]] bool isInventorySlotEmpty(const InventorySlot& slot);
void clearInventorySlot(InventorySlot& slot);
[[nodiscard]] bool canMergeInventorySlots(const InventorySlot& a, const InventorySlot& b);
[[nodiscard]] bool isCraftingIngredientSlot(const InventorySlot& slot);
[[nodiscard]] std::optional<CraftingMatch> evaluateCraftingGrid(
    const CraftingGridSlots& gridSlots,
    CraftingMode mode);
void consumeCraftingIngredients(CraftingGridSlots& gridSlots, const CraftingMatch& craftingMatch);
}  // namespace vibecraft::app
