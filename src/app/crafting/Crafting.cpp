#include "vibecraft/app/crafting/Crafting.hpp"

#include <algorithm>
#include <vector>

#include "CraftingRecipeInternal.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
namespace
{
using crafting_internal::RecipeDefinition;

[[nodiscard]] InventorySlot normalizedIngredientAt(
    const CraftingGridSlots& gridSlots,
    const std::size_t x,
    const std::size_t y)
{
    const InventorySlot& slot = gridSlots[y * 3 + x];
    if (!isCraftingIngredientSlot(slot))
    {
        return {};
    }
    return InventorySlot{
        .blockType = slot.blockType,
        .count = 1,
        .equippedItem = slot.equippedItem,
    };
}

[[nodiscard]] const std::vector<RecipeDefinition>& allRecipes()
{
    static const std::vector<RecipeDefinition> recipes = crafting_internal::buildAllRecipes();
    return recipes;
}
}  // namespace

bool isInventorySlotEmpty(const InventorySlot& slot)
{
    return slot.count == 0 || (slot.blockType == vibecraft::world::BlockType::Air && slot.equippedItem == EquippedItem::None);
}

void clearInventorySlot(InventorySlot& slot)
{
    slot.blockType = vibecraft::world::BlockType::Air;
    slot.count = 0;
    slot.equippedItem = EquippedItem::None;
    slot.durabilityRemaining = 0;
}

bool canMergeInventorySlots(const InventorySlot& a, const InventorySlot& b)
{
    if (isInventorySlotEmpty(a) || isInventorySlotEmpty(b))
    {
        return false;
    }

    if (a.blockType != b.blockType || a.equippedItem != b.equippedItem)
    {
        return false;
    }
    if (isDamageableEquippedItem(a.equippedItem))
    {
        const std::uint16_t maxDurability = maxDurabilityForEquippedItem(a.equippedItem);
        const std::uint16_t aDurability = a.durabilityRemaining == 0 ? maxDurability : a.durabilityRemaining;
        const std::uint16_t bDurability = b.durabilityRemaining == 0 ? maxDurability : b.durabilityRemaining;
        return aDurability == bDurability;
    }
    return true;
}

bool isCraftingIngredientSlot(const InventorySlot& slot)
{
    return slot.count > 0
        && (slot.equippedItem != EquippedItem::None || slot.blockType != vibecraft::world::BlockType::Air);
}

std::optional<CraftingMatch> evaluateCraftingGrid(
    const CraftingGridSlots& gridSlots,
    const CraftingMode mode)
{
    const std::size_t activeWidth = mode == CraftingMode::Workbench3x3 ? 3 : 2;
    const std::size_t activeHeight = mode == CraftingMode::Workbench3x3 ? 3 : 2;

    const std::vector<RecipeDefinition>& recipes = allRecipes();
    for (const RecipeDefinition& recipe : recipes)
    {
        if (recipe.requiresWorkbench && mode != CraftingMode::Workbench3x3)
        {
            continue;
        }
        if (recipe.width > activeWidth || recipe.height > activeHeight)
        {
            continue;
        }

        const std::size_t maxOffsetX = activeWidth - recipe.width;
        const std::size_t maxOffsetY = activeHeight - recipe.height;
        for (std::size_t offsetY = 0; offsetY <= maxOffsetY; ++offsetY)
        {
            for (std::size_t offsetX = 0; offsetX <= maxOffsetX; ++offsetX)
            {
                bool matches = true;
                CraftingMatch craftingMatch{};
                craftingMatch.output = recipe.output;

                for (std::size_t y = 0; y < activeHeight && matches; ++y)
                {
                    for (std::size_t x = 0; x < activeWidth; ++x)
                    {
                        const bool insideRecipe =
                            x >= offsetX && x < offsetX + recipe.width
                            && y >= offsetY && y < offsetY + recipe.height;
                        const InventorySlot expectedSlot = insideRecipe
                            ? recipe.pattern[crafting_internal::patternIndex(
                                  x - offsetX,
                                  y - offsetY,
                                  recipe.width)]
                            : InventorySlot{};
                        const InventorySlot actualSlot = normalizedIngredientAt(gridSlots, x, y);
                        if (actualSlot.blockType != expectedSlot.blockType
                            || actualSlot.equippedItem != expectedSlot.equippedItem)
                        {
                            matches = false;
                            break;
                        }
                        if (insideRecipe && !isInventorySlotEmpty(expectedSlot))
                        {
                            craftingMatch.consumedSlotIndices[craftingMatch.consumedSlotCount++] = y * 3 + x;
                        }
                    }
                }

                if (!matches)
                {
                    continue;
                }

                for (std::size_t y = activeHeight; y < 3 && matches; ++y)
                {
                    for (std::size_t x = 0; x < 3; ++x)
                    {
                        if (!isInventorySlotEmpty(gridSlots[y * 3 + x]))
                        {
                            matches = false;
                            break;
                        }
                    }
                }

                for (std::size_t y = 0; y < activeHeight && matches; ++y)
                {
                    for (std::size_t x = activeWidth; x < 3; ++x)
                    {
                        if (!isInventorySlotEmpty(gridSlots[y * 3 + x]))
                        {
                            matches = false;
                            break;
                        }
                    }
                }

                if (matches && !isInventorySlotEmpty(craftingMatch.output))
                {
                    return craftingMatch;
                }
            }
        }
    }

    return std::nullopt;
}

void consumeCraftingIngredients(CraftingGridSlots& gridSlots, const CraftingMatch& craftingMatch)
{
    for (std::size_t i = 0; i < craftingMatch.consumedSlotCount; ++i)
    {
        InventorySlot& slot = gridSlots[craftingMatch.consumedSlotIndices[i]];
        if (slot.count > 0)
        {
            --slot.count;
        }
        if (slot.count == 0)
        {
            clearInventorySlot(slot);
        }
    }
}
}  // namespace vibecraft::app
