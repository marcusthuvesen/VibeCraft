#include <doctest/doctest.h>

#include "vibecraft/app/crafting/Crafting.hpp"
#include "vibecraft/world/Block.hpp"

TEST_CASE("crafting recipes cover inventory basics and workbench-only outputs")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots inventoryGrid{};
    inventoryGrid[0].blockType = BlockType::OakLog;
    inventoryGrid[0].count = 1;
    const auto planksMatch = vibecraft::app::evaluateCraftingGrid(
        inventoryGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(planksMatch.has_value());
    CHECK(planksMatch->output.blockType == BlockType::OakPlanks);
    CHECK(planksMatch->output.count == 4);

    CraftingGridSlots junglePlanksGrid{};
    junglePlanksGrid[0].blockType = BlockType::JungleLog;
    junglePlanksGrid[0].count = 1;
    const auto junglePlanksMatch = vibecraft::app::evaluateCraftingGrid(
        junglePlanksGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(junglePlanksMatch.has_value());
    CHECK(junglePlanksMatch->output.blockType == BlockType::JunglePlanks);
    CHECK(junglePlanksMatch->output.count == 4);

    CraftingGridSlots tableGrid{};
    tableGrid[0].blockType = BlockType::OakPlanks;
    tableGrid[0].count = 1;
    tableGrid[1].blockType = BlockType::OakPlanks;
    tableGrid[1].count = 1;
    tableGrid[3].blockType = BlockType::OakPlanks;
    tableGrid[3].count = 1;
    tableGrid[4].blockType = BlockType::OakPlanks;
    tableGrid[4].count = 1;
    const auto tableMatch = vibecraft::app::evaluateCraftingGrid(
        tableGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(tableMatch.has_value());
    CHECK(tableMatch->output.blockType == BlockType::CraftingTable);
    CHECK(tableMatch->output.count == 1);

    CraftingGridSlots stickGrid{};
    stickGrid[0].blockType = BlockType::OakPlanks;
    stickGrid[0].count = 1;
    stickGrid[3].blockType = BlockType::OakPlanks;
    stickGrid[3].count = 1;
    const auto stickMatch = vibecraft::app::evaluateCraftingGrid(
        stickGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(stickMatch.has_value());
    CHECK(stickMatch->output.equippedItem == EquippedItem::Stick);
    CHECK(stickMatch->output.count == 4);

    CraftingGridSlots torchGrid{};
    torchGrid[0].equippedItem = EquippedItem::Coal;
    torchGrid[0].count = 1;
    torchGrid[3].equippedItem = EquippedItem::Stick;
    torchGrid[3].count = 1;
    const auto torchMatch = vibecraft::app::evaluateCraftingGrid(
        torchGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(torchMatch.has_value());
    CHECK(torchMatch->output.blockType == BlockType::Torch);
    CHECK(torchMatch->output.count == 4);

    CraftingGridSlots wrongTorchOrderGrid{};
    wrongTorchOrderGrid[0].equippedItem = EquippedItem::Stick;
    wrongTorchOrderGrid[0].count = 1;
    wrongTorchOrderGrid[3].equippedItem = EquippedItem::Coal;
    wrongTorchOrderGrid[3].count = 1;
    const auto wrongTorchOrderMatch = vibecraft::app::evaluateCraftingGrid(
        wrongTorchOrderGrid,
        CraftingMode::Inventory2x2);
    CHECK_FALSE(wrongTorchOrderMatch.has_value());

    CraftingGridSlots sandstoneGrid{};
    sandstoneGrid[0].blockType = BlockType::Sand;
    sandstoneGrid[0].count = 1;
    sandstoneGrid[1].blockType = BlockType::Sand;
    sandstoneGrid[1].count = 1;
    sandstoneGrid[3].blockType = BlockType::Sand;
    sandstoneGrid[3].count = 1;
    sandstoneGrid[4].blockType = BlockType::Sand;
    sandstoneGrid[4].count = 1;
    const auto sandstoneMatch = vibecraft::app::evaluateCraftingGrid(
        sandstoneGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(sandstoneMatch.has_value());
    CHECK(sandstoneMatch->output.blockType == BlockType::Sandstone);
    CHECK(sandstoneMatch->output.count == 1);

    CraftingGridSlots swordGrid{};
    swordGrid[1].blockType = BlockType::DiamondOre;
    swordGrid[1].count = 1;
    swordGrid[4].blockType = BlockType::DiamondOre;
    swordGrid[4].count = 1;
    swordGrid[7].blockType = BlockType::Air;
    swordGrid[7].equippedItem = EquippedItem::Stick;
    swordGrid[7].count = 1;
    const auto swordWithoutWorkbench = vibecraft::app::evaluateCraftingGrid(
        swordGrid,
        CraftingMode::Inventory2x2);
    CHECK_FALSE(swordWithoutWorkbench.has_value());
    const auto swordWithWorkbench = vibecraft::app::evaluateCraftingGrid(
        swordGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(swordWithWorkbench.has_value());
    CHECK(swordWithWorkbench->output.blockType == BlockType::Air);
    CHECK(swordWithWorkbench->output.equippedItem == EquippedItem::DiamondSword);
    CHECK(swordWithWorkbench->output.count == 1);

    // Pickaxe (3x3 workbench): top row material, sticks in center column — Minecraft "T" head + handle.
    CraftingGridSlots woodPickaxeGrid{};
    woodPickaxeGrid[0].blockType = BlockType::OakPlanks;
    woodPickaxeGrid[0].count = 1;
    woodPickaxeGrid[1].blockType = BlockType::OakPlanks;
    woodPickaxeGrid[1].count = 1;
    woodPickaxeGrid[2].blockType = BlockType::OakPlanks;
    woodPickaxeGrid[2].count = 1;
    woodPickaxeGrid[4].equippedItem = EquippedItem::Stick;
    woodPickaxeGrid[4].count = 1;
    woodPickaxeGrid[7].equippedItem = EquippedItem::Stick;
    woodPickaxeGrid[7].count = 1;
    const auto woodPickaxeNoBench = vibecraft::app::evaluateCraftingGrid(
        woodPickaxeGrid,
        CraftingMode::Inventory2x2);
    CHECK_FALSE(woodPickaxeNoBench.has_value());
    const auto woodPickaxeBench = vibecraft::app::evaluateCraftingGrid(
        woodPickaxeGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(woodPickaxeBench.has_value());
    CHECK(woodPickaxeBench->output.equippedItem == EquippedItem::WoodPickaxe);
    CHECK(woodPickaxeBench->output.count == 1);

    CraftingGridSlots stonePickaxeGrid{};
    stonePickaxeGrid[0].blockType = BlockType::Cobblestone;
    stonePickaxeGrid[0].count = 1;
    stonePickaxeGrid[1].blockType = BlockType::Cobblestone;
    stonePickaxeGrid[1].count = 1;
    stonePickaxeGrid[2].blockType = BlockType::Cobblestone;
    stonePickaxeGrid[2].count = 1;
    stonePickaxeGrid[4].equippedItem = EquippedItem::Stick;
    stonePickaxeGrid[4].count = 1;
    stonePickaxeGrid[7].equippedItem = EquippedItem::Stick;
    stonePickaxeGrid[7].count = 1;
    const auto stonePickaxeBench = vibecraft::app::evaluateCraftingGrid(
        stonePickaxeGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(stonePickaxeBench.has_value());
    CHECK(stonePickaxeBench->output.equippedItem == EquippedItem::StonePickaxe);

    CraftingGridSlots ovenGrid{};
    for (std::size_t i = 0; i < 9; ++i)
    {
        ovenGrid[i].blockType = BlockType::Cobblestone;
        ovenGrid[i].count = 1;
    }
    ovenGrid[4].blockType = BlockType::Air;
    ovenGrid[4].count = 0;
    const auto ovenMatch = vibecraft::app::evaluateCraftingGrid(
        ovenGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(ovenMatch.has_value());
    CHECK(ovenMatch->output.blockType == BlockType::Furnace);
    CHECK(ovenMatch->output.count == 1);

    CraftingGridSlots chestGrid{};
    for (std::size_t i = 0; i < 9; ++i)
    {
        chestGrid[i].blockType = BlockType::OakPlanks;
        chestGrid[i].count = 1;
    }
    chestGrid[4].blockType = BlockType::Air;
    chestGrid[4].count = 0;
    const auto chestMatch = vibecraft::app::evaluateCraftingGrid(
        chestGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(chestMatch.has_value());
    CHECK(chestMatch->output.blockType == BlockType::Chest);
    CHECK(chestMatch->output.count == 1);

    CraftingGridSlots stoneBricksGrid{};
    stoneBricksGrid[0].blockType = BlockType::Stone;
    stoneBricksGrid[0].count = 1;
    stoneBricksGrid[1].blockType = BlockType::Stone;
    stoneBricksGrid[1].count = 1;
    stoneBricksGrid[3].blockType = BlockType::Stone;
    stoneBricksGrid[3].count = 1;
    stoneBricksGrid[4].blockType = BlockType::Stone;
    stoneBricksGrid[4].count = 1;
    const auto stoneBricksMatch = vibecraft::app::evaluateCraftingGrid(
        stoneBricksGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(stoneBricksMatch.has_value());
    CHECK(stoneBricksMatch->output.blockType == BlockType::Bricks);
    CHECK(stoneBricksMatch->output.count == 4);

    CraftingGridSlots mossyGrid{};
    mossyGrid[0].blockType = BlockType::Cobblestone;
    mossyGrid[0].count = 1;
    mossyGrid[1].blockType = BlockType::MossBlock;
    mossyGrid[1].count = 1;
    const auto mossyMatch = vibecraft::app::evaluateCraftingGrid(
        mossyGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(mossyMatch.has_value());
    CHECK(mossyMatch->output.blockType == BlockType::MossyCobblestone);
    CHECK(mossyMatch->output.count == 2);

    CraftingGridSlots vineMossyGrid{};
    vineMossyGrid[0].blockType = BlockType::Cobblestone;
    vineMossyGrid[0].count = 1;
    vineMossyGrid[1].blockType = BlockType::Vines;
    vineMossyGrid[1].count = 1;
    const auto vineMossyMatch = vibecraft::app::evaluateCraftingGrid(
        vineMossyGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(vineMossyMatch.has_value());
    CHECK(vineMossyMatch->output.blockType == BlockType::MossyCobblestone);

    CraftingGridSlots bambooStickGrid{};
    bambooStickGrid[0].blockType = BlockType::Bamboo;
    bambooStickGrid[0].count = 1;
    bambooStickGrid[3].blockType = BlockType::Bamboo;
    bambooStickGrid[3].count = 1;
    const auto bambooStickMatch = vibecraft::app::evaluateCraftingGrid(
        bambooStickGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(bambooStickMatch.has_value());
    CHECK(bambooStickMatch->output.equippedItem == EquippedItem::Stick);
    CHECK(bambooStickMatch->output.count == 1);

    CraftingGridSlots bookshelfGrid{};
    for (const std::size_t i : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{6}, std::size_t{7}, std::size_t{8}})
    {
        bookshelfGrid[i].blockType = BlockType::OakPlanks;
        bookshelfGrid[i].count = 1;
    }
    for (const std::size_t i : {std::size_t{3}, std::size_t{4}, std::size_t{5}})
    {
        bookshelfGrid[i].equippedItem = EquippedItem::Stick;
        bookshelfGrid[i].count = 1;
    }
    const auto bookshelfMatch = vibecraft::app::evaluateCraftingGrid(
        bookshelfGrid,
        CraftingMode::Workbench3x3);
    REQUIRE(bookshelfMatch.has_value());
    CHECK(bookshelfMatch->output.blockType == BlockType::Bookshelf);
    CHECK(bookshelfMatch->output.count == 1);
}
