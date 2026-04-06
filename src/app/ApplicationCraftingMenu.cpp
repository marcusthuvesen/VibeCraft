#include "vibecraft/app/Application.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::int64_t blockStorageKey(const glm::ivec3& blockPosition)
{
    constexpr std::int64_t offset = 1LL << 20;
    constexpr std::int64_t mask = (1LL << 21) - 1LL;
    const std::int64_t x = (static_cast<std::int64_t>(blockPosition.x) + offset) & mask;
    const std::int64_t y = (static_cast<std::int64_t>(blockPosition.y) + offset) & mask;
    const std::int64_t z = (static_cast<std::int64_t>(blockPosition.z) + offset) & mask;
    return (x << 42) | (y << 21) | z;
}

[[nodiscard]] bool canPlaceIntoCraftingGrid(const InventorySlot& slot)
{
    return slot.count > 0
        && (slot.equippedItem != EquippedItem::None || slot.blockType != world::BlockType::Air);
}

[[nodiscard]] bool canReceiveCraftingOutput(
    const InventorySlot& carriedSlot,
    const InventorySlot& outputSlot)
{
    const std::uint32_t stackLimit = isInventorySlotEmpty(carriedSlot)
        ? inventorySlotStackLimit(outputSlot)
        : inventorySlotStackLimit(carriedSlot);
    return isInventorySlotEmpty(carriedSlot)
        || (canMergeInventorySlots(carriedSlot, outputSlot)
            && carriedSlot.count + outputSlot.count <= stackLimit);
}

[[nodiscard]] bool isFurnaceGridSlotIndex(const std::size_t slotIndex)
{
    return slotIndex == kFurnaceInputSlotIndex || slotIndex == kFurnaceFuelSlotIndex;
}

[[nodiscard]] bool canPlaceIntoFurnaceGridSlot(const InventorySlot& slot, const std::size_t slotIndex)
{
    if (isInventorySlotEmpty(slot))
    {
        return true;
    }
    if (slotIndex == kFurnaceInputSlotIndex)
    {
        return canAcceptFurnaceInput(slot);
    }
    if (slotIndex == kFurnaceFuelSlotIndex)
    {
        return canAcceptFurnaceFuel(slot);
    }
    return false;
}

void mergeOrSwapInventorySlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const bool allowPlacedEquippedItem)
{
    if (!allowPlacedEquippedItem && carriedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }
    if (!allowPlacedEquippedItem && targetSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    if (isInventorySlotEmpty(carriedSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }
    if (isInventorySlotEmpty(targetSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }

    const std::uint32_t stackLimit = inventorySlotStackLimit(targetSlot);
    if (canMergeInventorySlots(carriedSlot, targetSlot) && targetSlot.count < stackLimit)
    {
        const std::uint32_t space = stackLimit - targetSlot.count;
        const std::uint32_t transfer = std::min(space, carriedSlot.count);
        targetSlot.count += transfer;
        carriedSlot.count -= transfer;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
        return;
    }

    std::swap(carriedSlot, targetSlot);
}

void rightClickInventorySlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const bool allowPlacedEquippedItem)
{
    if (!allowPlacedEquippedItem && !isInventorySlotEmpty(carriedSlot)
        && carriedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }
    if (!allowPlacedEquippedItem && !isInventorySlotEmpty(targetSlot)
        && targetSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    if (isInventorySlotEmpty(carriedSlot))
    {
        if (isInventorySlotEmpty(targetSlot))
        {
            return;
        }

        carriedSlot = targetSlot;
        carriedSlot.count = (targetSlot.count + 1U) / 2U;
        targetSlot.count -= carriedSlot.count;
        if (targetSlot.count == 0)
        {
            clearInventorySlot(targetSlot);
        }
        return;
    }

    if (isInventorySlotEmpty(targetSlot))
    {
        targetSlot = carriedSlot;
        targetSlot.count = 1U;
        --carriedSlot.count;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
        return;
    }

    if (!canMergeInventorySlots(carriedSlot, targetSlot)
        || targetSlot.count >= inventorySlotStackLimit(targetSlot))
    {
        return;
    }

    ++targetSlot.count;
    --carriedSlot.count;
    if (carriedSlot.count == 0)
    {
        clearInventorySlot(carriedSlot);
    }
}

void releaseMouseForMenu(
    bool& mouseCaptured,
    platform::Window& window,
    platform::InputState& inputState)
{
    mouseCaptured = false;
    window.setRelativeMouseMode(false);
    inputState.clearMouseMotion();
}

void recaptureMouseAfterMenu(
    bool& mouseCaptured,
    platform::Window& window,
    platform::InputState& inputState)
{
    mouseCaptured = true;
    window.setRelativeMouseMode(true);
    inputState.clearMouseMotion();
}
}  // namespace

void Application::openCraftingMenu(
    const bool useWorkbench,
    const glm::ivec3& workbenchBlockPosition)
{
    if (!craftingMenuState_.active)
    {
        craftingMenuState_ = CraftingMenuState{};
    }
    craftingMenuState_.active = true;
    craftingMenuState_.mode = useWorkbench
        ? CraftingMenuState::Mode::WorkbenchCrafting
        : CraftingMenuState::Mode::InventoryCrafting;
    craftingMenuState_.usesWorkbench = useWorkbench;
    craftingMenuState_.workbenchBlockPosition = workbenchBlockPosition;
    craftingMenuState_.chestBlockPosition = glm::ivec3(0);
    craftingMenuState_.furnaceBlockPosition = glm::ivec3(0);
    craftingMenuState_.bagStartRow = 0;
    craftingMenuState_.hint = useWorkbench
        ? "3x3 workbench crafting: left-click move, right-click split/place one."
        : "2x2 inventory crafting: logs -> planks, planks -> sticks/table. Gear slots are on the left.";
    soundEffects_.playUiClick();
    releaseMouseForMenu(mouseCaptured_, window_, inputState_);
}

void Application::openChestMenu(const glm::ivec3& chestBlockPosition)
{
    if (!craftingMenuState_.active)
    {
        craftingMenuState_ = CraftingMenuState{};
    }
    craftingMenuState_.active = true;
    craftingMenuState_.mode = CraftingMenuState::Mode::ChestStorage;
    craftingMenuState_.usesWorkbench = true;
    craftingMenuState_.workbenchBlockPosition = glm::ivec3(0);
    craftingMenuState_.chestBlockPosition = chestBlockPosition;
    craftingMenuState_.furnaceBlockPosition = glm::ivec3(0);
    craftingMenuState_.bagStartRow = 0;
    craftingMenuState_.hint = "Chest storage: move stacks between chest and inventory.";
    craftingMenuState_.gridSlots = chestSlotsByPosition_[blockStorageKey(chestBlockPosition)];
    soundEffects_.playChestOpen();
    releaseMouseForMenu(mouseCaptured_, window_, inputState_);
}

void Application::openFurnaceMenu(const glm::ivec3& furnaceBlockPosition)
{
    if (!craftingMenuState_.active)
    {
        craftingMenuState_ = CraftingMenuState{};
    }
    craftingMenuState_.active = true;
    craftingMenuState_.mode = CraftingMenuState::Mode::Furnace;
    craftingMenuState_.usesWorkbench = true;
    craftingMenuState_.workbenchBlockPosition = glm::ivec3(0);
    craftingMenuState_.chestBlockPosition = glm::ivec3(0);
    craftingMenuState_.furnaceBlockPosition = furnaceBlockPosition;
    craftingMenuState_.bagStartRow = 0;
    craftingMenuState_.hint = "Furnace: top slot smelts, bottom slot burns fuel. Coal and wood both work.";
    craftingMenuState_.gridSlots.fill({});
    syncFurnaceStateToOpenMenu();
    soundEffects_.playUiClick();
    releaseMouseForMenu(mouseCaptured_, window_, inputState_);
}

void Application::syncOpenFurnaceMenuToState()
{
    if (!craftingMenuState_.active || craftingMenuState_.mode != CraftingMenuState::Mode::Furnace)
    {
        return;
    }
    FurnaceBlockState& furnaceState =
        furnaceStatesByPosition_[blockStorageKey(craftingMenuState_.furnaceBlockPosition)];
    furnaceState.inputSlot = craftingMenuState_.gridSlots[kFurnaceInputSlotIndex];
    furnaceState.fuelSlot = craftingMenuState_.gridSlots[kFurnaceFuelSlotIndex];
}

void Application::syncFurnaceStateToOpenMenu()
{
    if (!craftingMenuState_.active || craftingMenuState_.mode != CraftingMenuState::Mode::Furnace)
    {
        return;
    }
    const auto furnaceIt = furnaceStatesByPosition_.find(blockStorageKey(craftingMenuState_.furnaceBlockPosition));
    craftingMenuState_.gridSlots.fill({});
    if (furnaceIt == furnaceStatesByPosition_.end())
    {
        return;
    }
    craftingMenuState_.gridSlots[kFurnaceInputSlotIndex] = furnaceIt->second.inputSlot;
    craftingMenuState_.gridSlots[kFurnaceFuelSlotIndex] = furnaceIt->second.fuelSlot;
}

void Application::tickFurnaces(const float deltaTimeSeconds)
{
    if (deltaTimeSeconds <= 0.0f)
    {
        return;
    }
    if (craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::Furnace)
    {
        syncOpenFurnaceMenuToState();
    }
    for (auto it = furnaceStatesByPosition_.begin(); it != furnaceStatesByPosition_.end();)
    {
        tickFurnaceBlockState(it->second, deltaTimeSeconds);
        if (!furnaceStateHasAnyContents(it->second))
        {
            it = furnaceStatesByPosition_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    if (craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::Furnace)
    {
        syncFurnaceStateToOpenMenu();
    }
}

void Application::returnCraftingSlotsToInventory(const bool includeGridSlots)
{
    const auto tryInsertSlot = [&](InventorySlot& slot)
    {
        if (isInventorySlotEmpty(slot))
        {
            return true;
        }

        const auto tryMergeInto = [&](auto& slots) -> bool
        {
            for (InventorySlot& existingSlot : slots)
            {
                const std::uint32_t stackLimit = inventorySlotStackLimit(existingSlot);
                if (canMergeInventorySlots(existingSlot, slot) && existingSlot.count < stackLimit)
                {
                    const std::uint32_t transfer = std::min(stackLimit - existingSlot.count, slot.count);
                    existingSlot.count += transfer;
                    slot.count -= transfer;
                    if (slot.count == 0)
                    {
                        return true;
                    }
                }
            }
            for (InventorySlot& existingSlot : slots)
            {
                if (isInventorySlotEmpty(existingSlot))
                {
                    existingSlot = slot;
                    clearInventorySlot(slot);
                    return true;
                }
            }
            return isInventorySlotEmpty(slot);
        };

        if (tryMergeInto(hotbarSlots_) && isInventorySlotEmpty(slot))
        {
            return true;
        }
        if (tryMergeInto(bagSlots_) && isInventorySlotEmpty(slot))
        {
            return true;
        }
        return isInventorySlotEmpty(slot);
    };

    if (includeGridSlots)
    {
        for (InventorySlot& slot : craftingMenuState_.gridSlots)
        {
            while (!isInventorySlotEmpty(slot))
            {
                if (tryInsertSlot(slot))
                {
                    break;
                }

                if (slot.equippedItem != EquippedItem::None)
                {
                    spawnDroppedItemAtPosition(
                        slot.equippedItem,
                        playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
                }
                else
                {
                    spawnDroppedItemAtPosition(
                        slot.blockType,
                        playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
                }
                --slot.count;
                if (slot.count == 0)
                {
                    clearInventorySlot(slot);
                }
            }
        }
    }

    while (!isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        if (tryInsertSlot(craftingMenuState_.carriedSlot))
        {
            break;
        }

        if (craftingMenuState_.carriedSlot.equippedItem != EquippedItem::None)
        {
            spawnDroppedItemAtPosition(
                craftingMenuState_.carriedSlot.equippedItem,
                playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
        }
        else if (craftingMenuState_.carriedSlot.blockType != world::BlockType::Air)
        {
            spawnDroppedItemAtPosition(
                craftingMenuState_.carriedSlot.blockType,
                playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
        }
        clearInventorySlot(craftingMenuState_.carriedSlot);
    }
}

void Application::closeCraftingMenu()
{
    const bool closingChest = craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage;
    const bool closingFurnace = craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::Furnace;
    if (craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage)
    {
        const std::int64_t key = blockStorageKey(craftingMenuState_.chestBlockPosition);
        bool hasAnyItem = false;
        for (const InventorySlot& slot : craftingMenuState_.gridSlots)
        {
            if (!isInventorySlotEmpty(slot))
            {
                hasAnyItem = true;
                break;
            }
        }
        if (hasAnyItem)
        {
            chestSlotsByPosition_[key] = craftingMenuState_.gridSlots;
        }
        else
        {
            chestSlotsByPosition_.erase(key);
        }
    }
    if (closingFurnace)
    {
        syncOpenFurnaceMenuToState();
        const std::int64_t key = blockStorageKey(craftingMenuState_.furnaceBlockPosition);
        const auto furnaceIt = furnaceStatesByPosition_.find(key);
        if (furnaceIt != furnaceStatesByPosition_.end() && !furnaceStateHasAnyContents(furnaceIt->second))
        {
            furnaceStatesByPosition_.erase(furnaceIt);
        }
    }
    returnCraftingSlotsToInventory(!(closingChest || closingFurnace));
    craftingMenuState_ = CraftingMenuState{};
    if (closingChest)
    {
        soundEffects_.playChestClose();
    }
    else
    {
        soundEffects_.playUiClick();
    }
    recaptureMouseAfterMenu(mouseCaptured_, window_, inputState_);
}

void Application::applyCraftingMenuPrimaryInteraction(const int hit)
{
    const bool chestMode = craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage;
    const bool furnaceMode = craftingMenuState_.mode == CraftingMenuState::Mode::Furnace;
    if (hit < 0)
    {
        if (!isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            dropEntireSlotInFront(craftingMenuState_.carriedSlot, 1.4f);
            soundEffects_.playUiClick();
        }
        return;
    }
    soundEffects_.playUiClick();

    if (furnaceMode && hit == render::Renderer::kCraftingResultHit)
    {
        syncOpenFurnaceMenuToState();
        FurnaceBlockState& furnaceState =
            furnaceStatesByPosition_[blockStorageKey(craftingMenuState_.furnaceBlockPosition)];
        if (!canReceiveCraftingOutput(craftingMenuState_.carriedSlot, furnaceState.outputSlot))
        {
            return;
        }
        if (isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            craftingMenuState_.carriedSlot = furnaceState.outputSlot;
        }
        else
        {
            craftingMenuState_.carriedSlot.count += furnaceState.outputSlot.count;
        }
        clearInventorySlot(furnaceState.outputSlot);
        syncFurnaceStateToOpenMenu();
        soundEffects_.playUiClick();
        return;
    }
    if (!chestMode && !furnaceMode && hit == render::Renderer::kCraftingResultHit)
    {
        const std::optional<CraftingMatch> craftingMatch = evaluateCraftingGrid(
            craftingMenuState_.gridSlots,
            craftingMenuState_.usesWorkbench ? CraftingMode::Workbench3x3 : CraftingMode::Inventory2x2);
        if (!craftingMatch.has_value() || !canReceiveCraftingOutput(craftingMenuState_.carriedSlot, craftingMatch->output))
        {
            return;
        }
        if (isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            craftingMenuState_.carriedSlot = craftingMatch->output;
        }
        else
        {
            craftingMenuState_.carriedSlot.count += craftingMatch->output.count;
        }
        consumeCraftingIngredients(craftingMenuState_.gridSlots, craftingMatch.value());
        soundEffects_.playBlockPlace(craftingMatch->output.blockType);
        return;
    }
    if ((chestMode || furnaceMode) && hit == render::Renderer::kCraftingResultHit)
    {
        return;
    }

    InventorySlot* targetSlot = nullptr;
    bool isCraftingGridSlot = false;
    if (hit >= render::Renderer::kCraftingGridHitBase
        && hit < render::Renderer::kCraftingGridHitBase + static_cast<int>(craftingMenuState_.gridSlots.size()))
    {
        targetSlot = &craftingMenuState_.gridSlots[static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase)];
        isCraftingGridSlot = true;
    }
    else if (hit >= render::Renderer::kCraftingEquipmentHitBase
             && hit < render::Renderer::kCraftingEquipmentHitBase + static_cast<int>(equipmentSlots_.size()))
    {
        const EquipmentSlotKind slotKind =
            equipmentSlotKindForIndex(static_cast<std::size_t>(hit - render::Renderer::kCraftingEquipmentHitBase));
        InventorySlot& equipmentSlot = equipmentSlots_[equipmentSlotIndex(slotKind)];
        mergeOrSwapEquipmentSlot(craftingMenuState_.carriedSlot, equipmentSlot, slotKind);
        return;
    }
    else if (hit >= render::Renderer::kCraftingHotbarHitBase
             && hit < render::Renderer::kCraftingHotbarHitBase + static_cast<int>(hotbarSlots_.size()))
    {
        targetSlot = &hotbarSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingHotbarHitBase)];
    }
    else if (hit >= render::Renderer::kCraftingBagHitBase
             && hit < render::Renderer::kCraftingBagHitBase + static_cast<int>(bagSlots_.size()))
    {
        targetSlot = &bagSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingBagHitBase)];
    }

    if (targetSlot == nullptr)
    {
        return;
    }

    if (furnaceMode && isCraftingGridSlot)
    {
        const std::size_t furnaceSlotIndex = static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase);
        if (!isFurnaceGridSlotIndex(furnaceSlotIndex)
            || (!isInventorySlotEmpty(craftingMenuState_.carriedSlot)
                && !canPlaceIntoFurnaceGridSlot(craftingMenuState_.carriedSlot, furnaceSlotIndex)))
        {
            return;
        }
    }

    if (!chestMode && !furnaceMode && isCraftingGridSlot && !canPlaceIntoCraftingGrid(craftingMenuState_.carriedSlot)
        && !isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        return;
    }

    mergeOrSwapInventorySlot(
        craftingMenuState_.carriedSlot,
        *targetSlot,
        true);
    if (furnaceMode)
    {
        syncOpenFurnaceMenuToState();
    }
}

void Application::handleCraftingMenuRightClick()
{
    const bool chestMode = craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage;
    const bool furnaceMode = craftingMenuState_.mode == CraftingMenuState::Mode::Furnace;
    const render::CraftingUiMode renderMode =
        furnaceMode ? render::CraftingUiMode::Furnace
        : chestMode ? render::CraftingUiMode::Chest
        : craftingMenuState_.usesWorkbench ? render::CraftingUiMode::Workbench
                                           : render::CraftingUiMode::Inventory;
    const int hit = render::Renderer::hitTestCraftingMenu(
        inputState_.mouseWindowX,
        inputState_.mouseWindowY,
        window_.width(),
        window_.height(),
        renderMode,
        craftingMenuState_.usesWorkbench,
        craftingMenuState_.bagStartRow);
    if (hit < 0)
    {
        if (!isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            dropSingleItemFromSlotInFront(craftingMenuState_.carriedSlot, 1.4f);
            soundEffects_.playUiClick();
        }
        return;
    }
    if (furnaceMode && hit == render::Renderer::kCraftingResultHit)
    {
        syncOpenFurnaceMenuToState();
        FurnaceBlockState& furnaceState =
            furnaceStatesByPosition_[blockStorageKey(craftingMenuState_.furnaceBlockPosition)];
        if (isInventorySlotEmpty(furnaceState.outputSlot))
        {
            return;
        }
        const InventorySlot oneItemOutput{
            .blockType = furnaceState.outputSlot.blockType,
            .count = 1,
            .equippedItem = furnaceState.outputSlot.equippedItem,
        };
        if (!canReceiveCraftingOutput(craftingMenuState_.carriedSlot, oneItemOutput))
        {
            return;
        }
        if (isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            craftingMenuState_.carriedSlot = oneItemOutput;
        }
        else
        {
            ++craftingMenuState_.carriedSlot.count;
        }
        --furnaceState.outputSlot.count;
        if (furnaceState.outputSlot.count == 0)
        {
            clearInventorySlot(furnaceState.outputSlot);
        }
        syncFurnaceStateToOpenMenu();
        soundEffects_.playUiClick();
        return;
    }
    if (hit == render::Renderer::kCraftingResultHit)
    {
        return;
    }
    soundEffects_.playUiClick();

    InventorySlot* targetSlot = nullptr;
    bool isCraftingGridSlot = false;
    if (hit >= render::Renderer::kCraftingGridHitBase
        && hit < render::Renderer::kCraftingGridHitBase + static_cast<int>(craftingMenuState_.gridSlots.size()))
    {
        targetSlot = &craftingMenuState_.gridSlots[static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase)];
        isCraftingGridSlot = true;
    }
    else if (hit >= render::Renderer::kCraftingEquipmentHitBase
             && hit < render::Renderer::kCraftingEquipmentHitBase + static_cast<int>(equipmentSlots_.size()))
    {
        const EquipmentSlotKind slotKind =
            equipmentSlotKindForIndex(static_cast<std::size_t>(hit - render::Renderer::kCraftingEquipmentHitBase));
        InventorySlot& equipmentSlot = equipmentSlots_[equipmentSlotIndex(slotKind)];
        rightClickEquipmentSlot(craftingMenuState_.carriedSlot, equipmentSlot, slotKind);
        return;
    }
    else if (hit >= render::Renderer::kCraftingHotbarHitBase
             && hit < render::Renderer::kCraftingHotbarHitBase + static_cast<int>(hotbarSlots_.size()))
    {
        targetSlot = &hotbarSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingHotbarHitBase)];
    }
    else if (hit >= render::Renderer::kCraftingBagHitBase
             && hit < render::Renderer::kCraftingBagHitBase + static_cast<int>(bagSlots_.size()))
    {
        targetSlot = &bagSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingBagHitBase)];
    }

    if (targetSlot == nullptr)
    {
        return;
    }

    if (furnaceMode && isCraftingGridSlot)
    {
        const std::size_t furnaceSlotIndex = static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase);
        if (!isFurnaceGridSlotIndex(furnaceSlotIndex)
            || (!isInventorySlotEmpty(craftingMenuState_.carriedSlot)
                && !canPlaceIntoFurnaceGridSlot(craftingMenuState_.carriedSlot, furnaceSlotIndex)))
        {
            return;
        }
    }

    if (!chestMode && !furnaceMode && isCraftingGridSlot && !canPlaceIntoCraftingGrid(craftingMenuState_.carriedSlot)
        && !isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        return;
    }

    rightClickInventorySlot(
        craftingMenuState_.carriedSlot,
        *targetSlot,
        true);
    if (furnaceMode)
    {
        syncOpenFurnaceMenuToState();
    }
}
}  // namespace vibecraft::app
