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

    // Ladder: seven sticks in the vanilla "H" pattern -> 3 ladders.
    r.push_back(RecipeDefinition{
        .width = 3,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = {
            ingredientItem(EquippedItem::Stick),
            InventorySlot{},
            ingredientItem(EquippedItem::Stick),
            ingredientItem(EquippedItem::Stick),
            ingredientItem(EquippedItem::Stick),
            ingredientItem(EquippedItem::Stick),
            ingredientItem(EquippedItem::Stick),
            InventorySlot{},
            ingredientItem(EquippedItem::Stick),
        },
        .output = InventorySlot{
            .blockType = BlockType::Ladder,
            .count = 3,
            .equippedItem = EquippedItem::None,
        },
    });

    const auto addWoodDoorRecipe = [&r](const BlockType plankBlock, const BlockType doorBlock)
    {
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(plankBlock),
                ingredientBlock(plankBlock),
                ingredientBlock(plankBlock),
                ingredientBlock(plankBlock),
                ingredientBlock(plankBlock),
                ingredientBlock(plankBlock),
            },
            .output = InventorySlot{
                .blockType = doorBlock,
                .count = 3,
                .equippedItem = EquippedItem::None,
            },
        });
    };
    addWoodDoorRecipe(BlockType::OakPlanks, BlockType::OakDoor);
    addWoodDoorRecipe(BlockType::JunglePlanks, BlockType::JungleDoor);

    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = {
            ingredientItem(EquippedItem::IronIngot),
            ingredientItem(EquippedItem::IronIngot),
            ingredientItem(EquippedItem::IronIngot),
            ingredientItem(EquippedItem::IronIngot),
            ingredientItem(EquippedItem::IronIngot),
            ingredientItem(EquippedItem::IronIngot),
        },
        .output = InventorySlot{
            .blockType = BlockType::IronDoor,
            .count = 3,
            .equippedItem = EquippedItem::None,
        },
    });

    const auto addStairsRecipes = [&r](const BlockType material, const BlockType stairs)
    {
        const InventorySlot empty{};
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(material),
                empty,
                empty,
                ingredientBlock(material),
                ingredientBlock(material),
                empty,
                ingredientBlock(material),
                ingredientBlock(material),
                ingredientBlock(material),
            },
            .output = InventorySlot{
                .blockType = stairs,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                empty,
                empty,
                ingredientBlock(material),
                empty,
                ingredientBlock(material),
                ingredientBlock(material),
                ingredientBlock(material),
                ingredientBlock(material),
                ingredientBlock(material),
            },
            .output = InventorySlot{
                .blockType = stairs,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
    };
    addStairsRecipes(BlockType::OakPlanks, BlockType::OakStairs);
    addStairsRecipes(BlockType::JunglePlanks, BlockType::JungleStairs);
    addStairsRecipes(BlockType::Cobblestone, BlockType::CobblestoneStairs);
    addStairsRecipes(BlockType::Stone, BlockType::StoneStairs);
    addStairsRecipes(BlockType::Bricks, BlockType::BrickStairs);
    addStairsRecipes(BlockType::Sandstone, BlockType::SandstoneStairs);

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

    const InventorySlot emptySlot{};

    // Bow (workbench): three string (top row) + three sticks in column shape (Minecraft layout).
    r.push_back(RecipeDefinition{
        .width = 3,
        .height = 3,
        .requiresWorkbench = true,
        .pattern = {
            ingredientItem(EquippedItem::String),
            ingredientItem(EquippedItem::String),
            ingredientItem(EquippedItem::String),
            ingredientItem(EquippedItem::Stick),
            emptySlot,
            ingredientItem(EquippedItem::Stick),
            emptySlot,
            ingredientItem(EquippedItem::Stick),
            emptySlot,
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = EquippedItem::Bow,
        },
    });

    // Arrows x4 from feather + stick (both shapeless orders on 2x2 grid).
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientItem(EquippedItem::Feather),
            ingredientItem(EquippedItem::Stick),
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 4,
            .equippedItem = EquippedItem::Arrow,
        },
    });
    r.push_back(RecipeDefinition{
        .width = 2,
        .height = 1,
        .requiresWorkbench = false,
        .pattern = {
            ingredientItem(EquippedItem::Stick),
            ingredientItem(EquippedItem::Feather),
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
            emptySlot,
        },
        .output = InventorySlot{
            .blockType = BlockType::Air,
            .count = 4,
            .equippedItem = EquippedItem::Arrow,
        },
    });
}


}  // namespace vibecraft::app::crafting_internal
