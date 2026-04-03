#include "CraftingRecipeInternal.hpp"

namespace vibecraft::app::crafting_internal
{

void appendVanillaBlockRecipes(std::vector<RecipeDefinition>& r)
{
    using vibecraft::world::BlockType;

    // Vines + cobblestone (vanilla shapeless uses vine + cobble -> mossy cobble).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Cobblestone),
            ingredientBlock(BlockType::Vines),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Vines),
            ingredientBlock(BlockType::Cobblestone),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 1,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Cobblestone),
            ingredientBlock(BlockType::Vines),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 1,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Vines),
            ingredientBlock(BlockType::Cobblestone),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });

    // Mossy stone bricks: stone bricks + moss block (approximate output as mossy cobble).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Bricks),
            ingredientBlock(BlockType::MossBlock),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::MossBlock),
            ingredientBlock(BlockType::Bricks),
        },
        .output = InventorySlot{
            .blockType = BlockType::MossyCobblestone,
            .count = 2,
            .equippedItem = EquippedItem::None,
        },
    });

    // Bamboo: two stalks -> stick (Java/Bedrock parity).
    r.push_back(RecipeDefinition{
        .width = 1,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = EquippedItem::Stick,
        },
    });

    // Bamboo planks equivalent: 2x2 bamboo -> crafting table (same role as wood planks).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });

    // Bamboo block: nine bamboo on a workbench.
    r.push_back(RecipeDefinition{
        .width = 3,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = {
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
            ingredientBlock(BlockType::Bamboo),
        },
        .output = InventorySlot{
            .blockType = BlockType::Bamboo,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });

    // Leaves -> crafting table (shortcut when you only have leaves, like plank paths).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::OakLeaves),
            ingredientBlock(BlockType::OakLeaves),
            ingredientBlock(BlockType::OakLeaves),
            ingredientBlock(BlockType::OakLeaves),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::JungleLeaves),
            ingredientBlock(BlockType::JungleLeaves),
            ingredientBlock(BlockType::JungleLeaves),
            ingredientBlock(BlockType::JungleLeaves),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::SpruceLeaves),
            ingredientBlock(BlockType::SpruceLeaves),
            ingredientBlock(BlockType::SpruceLeaves),
            ingredientBlock(BlockType::SpruceLeaves),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::BirchLeaves),
            ingredientBlock(BlockType::BirchLeaves),
            ingredientBlock(BlockType::BirchLeaves),
            ingredientBlock(BlockType::BirchLeaves),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::DarkOakLeaves),
            ingredientBlock(BlockType::DarkOakLeaves),
            ingredientBlock(BlockType::DarkOakLeaves),
            ingredientBlock(BlockType::DarkOakLeaves),
        },
        .output = InventorySlot{
            .blockType = BlockType::CraftingTable,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });

    // Snow block: four snow layers -> one block (snowgrass stands in for snowballs).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 2,
        .requiresWorkbench = false,
        .pattern = {
            ingredientBlock(BlockType::SnowGrass),
            ingredientBlock(BlockType::SnowGrass),
            ingredientBlock(BlockType::SnowGrass),
            ingredientBlock(BlockType::SnowGrass),
        },
        .output = InventorySlot{
            .blockType = BlockType::SnowGrass,
            .count = 1,
            .equippedItem = EquippedItem::None,
        },
    });
}


}  // namespace vibecraft::app::crafting_internal
