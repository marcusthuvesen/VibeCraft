#include "CraftingRecipeInternal.hpp"

namespace vibecraft::app::crafting_internal
{

std::vector<RecipeDefinition> buildAllRecipes()
{
    using vibecraft::world::BlockType;
    std::vector<RecipeDefinition> r;
    const InventorySlot empty{};
    const InventorySlot hide = ingredientItem(EquippedItem::Leather);
    r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::OakLog)},
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
            .pattern = {ingredientBlock(BlockType::OakLeaves)},
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
            .pattern = {ingredientBlock(BlockType::JungleLog)},
            .output = InventorySlot{
                .blockType = BlockType::JunglePlanks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::JungleLeaves)},
            .output = InventorySlot{
                .blockType = BlockType::JunglePlanks,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::SpruceLog)},
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
            .pattern = {ingredientBlock(BlockType::SpruceLeaves)},
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
            .pattern = {ingredientBlock(BlockType::BirchLog)},
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
            .pattern = {ingredientBlock(BlockType::BirchLeaves)},
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
            .pattern = {ingredientBlock(BlockType::DarkOakLog)},
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
            .pattern = {ingredientBlock(BlockType::DarkOakLeaves)},
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
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::HabitatPanel),
                ingredientBlock(BlockType::Glass),
                ingredientBlock(BlockType::HabitatFrame),
                ingredientItem(EquippedItem::IronIngot),
            },
            .output = InventorySlot{
                .blockType = BlockType::AirlockPanel,
                .count = 2,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Glowstone),
                ingredientItem(EquippedItem::IronIngot),
            },
            .output = InventorySlot{
                .blockType = BlockType::PowerConduit,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
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
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
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
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Dirt),
                ingredientBlock(BlockType::Gravel),
                ingredientBlock(BlockType::Gravel),
                ingredientBlock(BlockType::Dirt),
            },
            .output = InventorySlot{
                .blockType = BlockType::CoarseDirt,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Gravel),
                ingredientBlock(BlockType::Dirt),
                ingredientBlock(BlockType::Dirt),
                ingredientBlock(BlockType::Gravel),
            },
            .output = InventorySlot{
                .blockType = BlockType::CoarseDirt,
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
        // Minecraft-style stone bricks: four smooth stone in a square.
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Stone),
                ingredientBlock(BlockType::Stone),
                ingredientBlock(BlockType::Stone),
                ingredientBlock(BlockType::Stone),
            },
            .output = InventorySlot{
                .blockType = BlockType::Bricks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Deepslate),
                ingredientBlock(BlockType::Deepslate),
                ingredientBlock(BlockType::Deepslate),
                ingredientBlock(BlockType::Deepslate),
            },
            .output = InventorySlot{
                .blockType = BlockType::Bricks,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        // Mossy cobblestone: cobble + moss (two horizontal orders; vanilla is shapeless).
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Cobblestone),
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
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::MossyCobblestone,
                .count = 2,
                .equippedItem = EquippedItem::None,
            },
        });
        // Bookshelf: sticks stand in for books until a book item exists.
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::Stick),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
                ingredientBlock(BlockType::OakPlanks),
            },
            .output = InventorySlot{
                .blockType = BlockType::Bookshelf,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::Stick),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
            },
            .output = InventorySlot{
                .blockType = BlockType::Bookshelf,
                .count = 1,
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
                .blockType = BlockType::Furnace,
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
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                InventorySlot{},
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
                ingredientBlock(BlockType::JunglePlanks),
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
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientItem(EquippedItem::Charcoal),
                ingredientItem(EquippedItem::Stick),
            },
            .output = InventorySlot{
                .blockType = BlockType::Torch,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Glowstone),
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
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientItem(EquippedItem::IronIngot),
                ingredientBlock(BlockType::Glass),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::HabitatPanel,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::HabitatPanel),
                ingredientBlock(BlockType::HabitatPanel),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::HabitatFloor,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientItem(EquippedItem::IronIngot),
                ingredientBlock(BlockType::Cobblestone),
            },
            .output = InventorySlot{
                .blockType = BlockType::HabitatFrame,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::Glass),
                ingredientBlock(BlockType::Glass),
                ingredientBlock(BlockType::MossBlock),
                ingredientItem(EquippedItem::IronIngot),
            },
            .output = InventorySlot{
                .blockType = BlockType::GreenhouseGlass,
                .count = 4,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 2,
            .height = 2,
            .requiresWorkbench = false,
            .pattern = {
                ingredientBlock(BlockType::HabitatFloor),
                ingredientBlock(BlockType::MossBlock),
                ingredientBlock(BlockType::Cobblestone),
                ingredientBlock(BlockType::Glass),
            },
            .output = InventorySlot{
                .blockType = BlockType::PlanterTray,
                .count = 2,
                .equippedItem = EquippedItem::None,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 1,
            .requiresWorkbench = false,
            .pattern = {ingredientBlock(BlockType::MossBlock)},
            .output = InventorySlot{
                .blockType = BlockType::FiberSapling,
                .count = 1,
                .equippedItem = EquippedItem::None,
            },
        });

        appendVanillaBlockRecipes(r);

        r.push_back(makeSwordRecipe(BlockType::OakPlanks, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::JunglePlanks, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::OakLog, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::JungleLog, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::SpruceLog, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::BirchLog, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::DarkOakLog, EquippedItem::WoodSword, true));
        r.push_back(makeSwordRecipe(BlockType::Cobblestone, EquippedItem::StoneSword, true));
        // Let fractured basalt, basalt, or deep basalt all stand in for a rough stone tier.
        r.push_back(makeSwordRecipe(BlockType::Stone, EquippedItem::StoneSword, true));
        r.push_back(makeSwordRecipe(BlockType::Deepslate, EquippedItem::StoneSword, true));
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::Stick),
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::IronSword,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 1,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::Stick),
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::GoldSword,
            },
        });
        r.push_back(makeSwordRecipe(BlockType::DiamondOre, EquippedItem::DiamondSword, true));

        r.push_back(makePickaxeRecipe(BlockType::OakPlanks, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::JunglePlanks, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::OakLog, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::JungleLog, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::SpruceLog, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::BirchLog, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::DarkOakLog, EquippedItem::WoodPickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Cobblestone, EquippedItem::StonePickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Stone, EquippedItem::StonePickaxe));
        r.push_back(makePickaxeRecipe(BlockType::Deepslate, EquippedItem::StonePickaxe));
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::IronIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::IronPickaxe,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::GoldIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::GoldPickaxe,
            },
        });
        r.push_back(makePickaxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondPickaxe));

        r.push_back(makeAxeRecipe(BlockType::OakPlanks, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::OakPlanks, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::JunglePlanks, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::JunglePlanks, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::OakLog, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::OakLog, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::JungleLog, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::JungleLog, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::SpruceLog, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::SpruceLog, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::BirchLog, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::BirchLog, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::DarkOakLog, EquippedItem::WoodAxe, false));
        r.push_back(makeAxeRecipe(BlockType::DarkOakLog, EquippedItem::WoodAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Cobblestone, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Cobblestone, EquippedItem::StoneAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Stone, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Stone, EquippedItem::StoneAxe, true));
        r.push_back(makeAxeRecipe(BlockType::Deepslate, EquippedItem::StoneAxe, false));
        r.push_back(makeAxeRecipe(BlockType::Deepslate, EquippedItem::StoneAxe, true));
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::IronIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::IronAxe,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                InventorySlot{},
                ingredientItem(EquippedItem::IronIngot),
                ingredientItem(EquippedItem::IronIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::IronIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::IronAxe,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::GoldIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::GoldAxe,
            },
        });
        r.push_back(RecipeDefinition{
            .width = 3,
            .height = 3,
            .requiresWorkbench = true,
            .pattern = {
                InventorySlot{},
                ingredientItem(EquippedItem::GoldIngot),
                ingredientItem(EquippedItem::GoldIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                ingredientItem(EquippedItem::GoldIngot),
                InventorySlot{},
                ingredientItem(EquippedItem::Stick),
                InventorySlot{},
            },
            .output = InventorySlot{
                .blockType = BlockType::Air,
                .count = 1,
                .equippedItem = EquippedItem::GoldAxe,
            },
        });
        r.push_back(makeAxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondAxe, false));
        r.push_back(makeAxeRecipe(BlockType::DiamondOre, EquippedItem::DiamondAxe, true));
        r.push_back(makeScoutArmorRecipe(
            {
                hide,
                hide,
                hide,
                hide,
                empty,
                hide,
                empty,
                empty,
                empty,
            },
            EquippedItem::ScoutHelmet));
        r.push_back(makeScoutArmorRecipe(
            {
                hide,
                empty,
                hide,
                hide,
                hide,
                hide,
                hide,
                hide,
                hide,
            },
            EquippedItem::ScoutChestRig));
        r.push_back(makeScoutArmorRecipe(
            {
                hide,
                hide,
                hide,
                hide,
                empty,
                hide,
                hide,
                empty,
                hide,
            },
            EquippedItem::ScoutGreaves));
        r.push_back(makeScoutArmorRecipe(
            {
                hide,
                empty,
                hide,
                hide,
                empty,
                hide,
                empty,
                empty,
                empty,
            },
            EquippedItem::ScoutBoots));

        return r;
}

}  // namespace vibecraft::app::crafting_internal

