#include "vibecraft/app/Crafting.hpp"

#include <algorithm>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
namespace
{
struct RecipeDefinition
{
    std::uint8_t width = 0;
    std::uint8_t height = 0;
    bool requiresWorkbench = false;
    std::array<vibecraft::world::BlockType, 9> pattern{};
    InventorySlot output{};
};

[[nodiscard]] constexpr std::size_t patternIndex(
    const std::size_t x,
    const std::size_t y,
    const std::size_t width)
{
    return y * width + x;
}

[[nodiscard]] vibecraft::world::BlockType ingredientTypeAt(
    const CraftingGridSlots& gridSlots,
    const std::size_t x,
    const std::size_t y)
{
    const InventorySlot& slot = gridSlots[y * 3 + x];
    if (!isCraftingIngredientSlot(slot))
    {
        return vibecraft::world::BlockType::Air;
    }
    return slot.blockType;
}

[[nodiscard]] constexpr std::array<RecipeDefinition, 2> recipeDefinitions()
{
    using vibecraft::world::BlockType;
    return {{
        RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {BlockType::TreeTrunk},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        },
        RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                BlockType::OakPlanks, BlockType::OakPlanks,
                BlockType::OakPlanks, BlockType::OakPlanks,
            },
            .output = InventorySlot{
                .blockType = BlockType::CraftingTable,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        },
    }};
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
}

bool canMergeInventorySlots(const InventorySlot& a, const InventorySlot& b)
{
    if (isInventorySlotEmpty(a) || isInventorySlotEmpty(b))
    {
        return false;
    }

    return a.blockType == b.blockType && a.equippedItem == b.equippedItem;
}

bool isCraftingIngredientSlot(const InventorySlot& slot)
{
    return slot.count > 0
        && slot.equippedItem == EquippedItem::None
        && slot.blockType != vibecraft::world::BlockType::Air;
}

std::optional<CraftingMatch> evaluateCraftingGrid(
    const CraftingGridSlots& gridSlots,
    const CraftingMode mode)
{
    const std::size_t activeWidth = mode == CraftingMode::Workbench3x3 ? 3 : 2;
    const std::size_t activeHeight = mode == CraftingMode::Workbench3x3 ? 3 : 2;

    const auto recipes = recipeDefinitions();
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
                        const vibecraft::world::BlockType expectedType = insideRecipe
                            ? recipe.pattern[patternIndex(x - offsetX, y - offsetY, recipe.width)]
                            : vibecraft::world::BlockType::Air;
                        const vibecraft::world::BlockType actualType = ingredientTypeAt(gridSlots, x, y);
                        if (actualType != expectedType)
                        {
                            matches = false;
                            break;
                        }
                        if (insideRecipe && expectedType != vibecraft::world::BlockType::Air)
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
