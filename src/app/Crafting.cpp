#include "vibecraft/app/Crafting.hpp"

#include <algorithm>
#include <vector>

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
    std::array<InventorySlot, 9> pattern{};
    InventorySlot output{};
};

[[nodiscard]] constexpr std::size_t patternIndex(
    const std::size_t x,
    const std::size_t y,
    const std::size_t width)
{
    return y * width + x;
}

[[nodiscard]] constexpr InventorySlot ingredientBlock(const vibecraft::world::BlockType blockType)
{
    return InventorySlot{
        .blockType = blockType,
        .count = 1,
        .equippedItem = EquippedItem::None,
    };
}

[[nodiscard]] constexpr InventorySlot ingredientItem(const EquippedItem equippedItem)
{
    return InventorySlot{
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = equippedItem,
    };
}

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

[[nodiscard]] RecipeDefinition makeSwordRecipe(
    const vibecraft::world::BlockType material,
    const EquippedItem output,
    const bool requiresWorkbench)
{
    using vibecraft::world::BlockType;
    return RecipeDefinition{
        .width = 1,
        .height = 3,
        .requiresWorkbench = requiresWorkbench,
        .pattern = {
            ingredientBlock(material),
            ingredientBlock(material),
            ingredientItem(EquippedItem::Stick),
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = output,
        },
    };
}

[[nodiscard]] RecipeDefinition makePickaxeRecipe(
    const vibecraft::world::BlockType material,
    const EquippedItem output)
{
    using vibecraft::world::BlockType;
    const InventorySlot empty{};
    return RecipeDefinition{
        .width = 3,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = {
            ingredientBlock(material),
            ingredientBlock(material),
            ingredientBlock(material),
            empty,
            ingredientItem(EquippedItem::Stick),
            empty,
            empty,
            ingredientItem(EquippedItem::Stick),
            empty,
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = output,
        },
    };
}

[[nodiscard]] RecipeDefinition makeAxeRecipe(
    const vibecraft::world::BlockType material,
    const EquippedItem output,
    const bool mirrored)
{
    using vibecraft::world::BlockType;
    const InventorySlot empty{};
    const std::array<InventorySlot, 9> pattern = mirrored
        ? std::array<InventorySlot, 9>{
            empty,
            ingredientBlock(material),
            ingredientBlock(material),
            empty,
            ingredientItem(EquippedItem::Stick),
            ingredientBlock(material),
            empty,
            ingredientItem(EquippedItem::Stick),
            empty,
        }
        : std::array<InventorySlot, 9>{
            ingredientBlock(material),
            ingredientBlock(material),
            empty,
            ingredientBlock(material),
            ingredientItem(EquippedItem::Stick),
            empty,
            empty,
            ingredientItem(EquippedItem::Stick),
            empty,
        };
    return RecipeDefinition{
        .width = 3,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = pattern,
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = output,
        },
    };
}

[[nodiscard]] const std::vector<RecipeDefinition>& allRecipes()
{
    using vibecraft::world::BlockType;
    static const std::vector<RecipeDefinition> recipes = []
    {
        std::vector<RecipeDefinition> r;
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::TreeTrunk)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::TreeCrown)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::JungleTreeTrunk)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::JungleTreeCrown)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::SnowTreeTrunk)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::SnowTreeCrown)},
            .output = InventorySlot{
                .blockType = BlockType::OakPlanks,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
            },
            .output = InventorySlot{
                .blockType = BlockType::CraftingTable,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 4,
                .equippedItem = EquippedItem::Stick,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
            },
            .output = InventorySlot{
                .blockType = BlockType::Sandstone,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::Sandstone)},
            .output = InventorySlot{
                .blockType = BlockType::Sand,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::Stone)},
            .output = InventorySlot{
                .blockType = BlockType::Cobblestone,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::Deepslate)},
            .output = InventorySlot{
                .blockType = BlockType::Cobblestone,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::Stone,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                InventorySlot{},
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::Oven,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                InventorySlot{},
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
            },
            .output = InventorySlot{
                .blockType = BlockType::Chest,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientItem(EquippedItem::Coal),
                ingredientItem(EquippedItem::Stick),
            },
            .output = InventorySlot{
                .blockType = BlockType::Torch,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::Sand),
                ingredientItem(EquippedItem::Coal),
                ingredientBlock(BlockType::Sand),
                ingredientItem(EquippedItem::Coal),
                ingredientBlock(BlockType::Sand),
                ingredientItem(EquippedItem::Coal),
                ingredientBlock(BlockType::Sand),
                ingredientItem(EquippedItem::Coal),
                ingredientBlock(BlockType::Sand),
            },
            .output = InventorySlot{
                .blockType = BlockType::TNT,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
                ingredientBlock(BlockType::Sand),
            },
            .output = InventorySlot{
                .blockType = BlockType::Glass,
                .count = 2,
                .equippedItem = EquippedItem::None,
            },
        });

        r.push_back(makeSwordRecipe(BlockType::OakPlanks, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::TreeTrunk, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::JungleTreeTrunk, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::SnowTreeTrunk, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::Cobblestone, EquippedItem::StoneSword, true));
        // Stone-tier: mined Stone/Deepslate drop as blocks; accept them like Cobblestone (Minecraft uses cobble).
        r.push_back(makeSwordRecipe(BlockType::Stone, EquippedItem::StoneSword, true));
        r.push_back(makeSwordRecipe(BlockType::Deepslate, EquippedItem::StoneSword, true));
        r.push_back(makeSwordRecipe(BlockType::IronOre, EquippedItem::IronSword, true));
        r.push_back(makeSwordRecipe(BlockType::GoldOre, EquippedItem::GoldSword, true));
        r.push_back(makeSwordRecipe(BlockType::DiamondOre, EquippedItem::DiamondSword, true));

        r.push_back(makePickaxeRecipe(BlockType::OakPlanks, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::TreeTrunk, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::JungleTreeTrunk, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::SnowTreeTrunk, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Cobblestone, EquippedItem::StonePickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Stone, EquippedItem::StonePickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Deepslate, EquippedItem::StonePickaxe));
        r.push_back(makePickaxeRecipe(BlockType::IronOre, EquippedItem::IronPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::GoldOre, EquippedItem::GoldPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondPickaxe));

        r.push_back(makeAxeRecipe(BlockType::OakPlanks, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::OakPlanks, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::TreeTrunk, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::TreeTrunk, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::JungleTreeTrunk, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::JungleTreeTrunk, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::SnowTreeTrunk, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::SnowTreeTrunk, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Cobblestone, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Cobblestone, EquippedItem::StoneAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Stone, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Stone, EquippedItem::StoneAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Deepslate, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Deepslate, EquippedItem::StoneAxe, true));
        r.push_back(makeAxeRecipe(BlockType::IronOre, EquippedItem::IronAxe, false));
        r.push_back(makeAxeRecipe(BlockType::IronOre, EquippedItem::IronAxe, true));
        r.push_back(makeAxeRecipe(BlockType::GoldOre, EquippedItem::GoldAxe, false));
        r.push_back(makeAxeRecipe(BlockType::GoldOre, EquippedItem::GoldAxe, true));
        r.push_back(makeAxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondAxe, false));
        r.push_back(makeAxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondAxe, true));

        return r;
    }();
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
                            ? recipe.pattern[patternIndex(x - offsetX, y - offsetY, recipe.width)]
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
