#pragma once

#include <optional>

#include "vibecraft/app/Inventory.hpp"

namespace vibecraft::app
{
constexpr std::size_t kFurnaceInputSlotIndex = 1;
constexpr std::size_t kFurnaceFuelSlotIndex = 7;
constexpr float kFurnaceSmeltSecondsPerItem = 5.0f;

struct FurnaceBlockState
{
    InventorySlot inputSlot{};
    InventorySlot fuelSlot{};
    InventorySlot outputSlot{};
    float fuelSecondsRemaining = 0.0f;
    float fuelSecondsCapacity = 0.0f;
    float smeltProgressSeconds = 0.0f;
};

[[nodiscard]] bool furnaceStateHasAnyContents(const FurnaceBlockState& state);
[[nodiscard]] bool isFurnaceFuel(const InventorySlot& slot);
[[nodiscard]] float furnaceFuelSeconds(const InventorySlot& slot);
[[nodiscard]] std::optional<InventorySlot> furnaceSmeltOutputForInput(const InventorySlot& inputSlot);
[[nodiscard]] bool canAcceptFurnaceInput(const InventorySlot& slot);
[[nodiscard]] bool canAcceptFurnaceFuel(const InventorySlot& slot);
[[nodiscard]] bool canSmeltInFurnace(const FurnaceBlockState& state);
[[nodiscard]] float furnaceFuelFraction(const FurnaceBlockState& state);
[[nodiscard]] float furnaceSmeltFraction(const FurnaceBlockState& state);
void tickFurnaceBlockState(FurnaceBlockState& state, float deltaTimeSeconds);
}  // namespace vibecraft::app
