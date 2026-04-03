#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::app::crafting_internal
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

[[nodiscard]] constexpr RecipeDefinition makeSwordRecipe(
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

[[nodiscard]] constexpr RecipeDefinition makePickaxeRecipe(
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

[[nodiscard]] constexpr RecipeDefinition makeAxeRecipe(
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

[[nodiscard]] constexpr RecipeDefinition makeScoutArmorRecipe(
    const std::array<InventorySlot, 9>& pattern,
    const EquippedItem output)
{
    using vibecraft::world::BlockType;
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

void appendVanillaBlockRecipes(std::vector<RecipeDefinition>& r);

[[nodiscard]] std::vector<RecipeDefinition> buildAllRecipes();

}  // namespace vibecraft::app::crafting_internal
