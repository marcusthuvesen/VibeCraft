#include <doctest/doctest.h>

#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/app/Crafting.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("oxygen generator metadata uses machine-style atlas tiles")
{
    using vibecraft::world::BlockFace;
    using vibecraft::world::BlockType;

    CHECK(vibecraft::world::textureTileIndex(BlockType::OxygenGenerator, BlockFace::Top) == 43);
    CHECK(vibecraft::world::textureTileIndex(BlockType::OxygenGenerator, BlockFace::Bottom) == 41);
    CHECK(vibecraft::world::textureTileIndex(BlockType::OxygenGenerator, BlockFace::Side) == 66);
}

TEST_CASE("oxygen generator recipe crafts from glass torch iron and cobblestone")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::world::BlockType;

    CraftingGridSlots grid{};
    grid[0] = {.blockType = BlockType::Glass, .count = 1};
    grid[1] = {.blockType = BlockType::Torch, .count = 1};
    grid[2] = {.blockType = BlockType::Glass, .count = 1};
    grid[3] = {.blockType = BlockType::IronOre, .count = 1};
    grid[4] = {.blockType = BlockType::IronOre, .count = 1};
    grid[5] = {.blockType = BlockType::IronOre, .count = 1};
    grid[6] = {.blockType = BlockType::Cobblestone, .count = 1};
    grid[7] = {.blockType = BlockType::Cobblestone, .count = 1};
    grid[8] = {.blockType = BlockType::Cobblestone, .count = 1};

    const auto match = vibecraft::app::evaluateCraftingGrid(grid, CraftingMode::Workbench3x3);
    REQUIRE(match.has_value());
    CHECK(match->output.blockType == BlockType::OxygenGenerator);
    CHECK(match->output.count == 1);
}

TEST_CASE("oxygen generator also crafts from grove moss and a lumen crystal core")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::world::BlockType;

    CraftingGridSlots grid{};
    grid[0] = {.blockType = BlockType::Glass, .count = 1};
    grid[1] = {.blockType = BlockType::Glowstone, .count = 1};
    grid[2] = {.blockType = BlockType::Glass, .count = 1};
    grid[3] = {.blockType = BlockType::IronOre, .count = 1};
    grid[4] = {.blockType = BlockType::MossBlock, .count = 1};
    grid[5] = {.blockType = BlockType::IronOre, .count = 1};
    grid[6] = {.blockType = BlockType::Cobblestone, .count = 1};
    grid[7] = {.blockType = BlockType::Cobblestone, .count = 1};
    grid[8] = {.blockType = BlockType::Cobblestone, .count = 1};

    const auto match = vibecraft::app::evaluateCraftingGrid(grid, CraftingMode::Workbench3x3);
    REQUIRE(match.has_value());
    CHECK(match->output.blockType == BlockType::OxygenGenerator);
    CHECK(match->output.count == 1);
}

TEST_CASE("oxygen generator creates a local safe zone")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    const auto nearEnvironment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(0.5f, 10.0f, 0.5f),
        {},
        false);
    CHECK(nearEnvironment.insideSafeZone);

    const auto farEnvironment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(40.0f, 10.0f, 40.0f),
        {},
        false);
    CHECK_FALSE(farEnvironment.insideSafeZone);
}

TEST_CASE("powered airlock panels extend oxygen beyond the relay core")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));
    for (int x = 1; x <= 8; ++x)
    {
        CHECK(world.applyEditCommand({
            .action = vibecraft::world::WorldEditAction::Place,
            .position = {x, 10, 0},
            .blockType = vibecraft::world::BlockType::PowerConduit,
        }));
    }
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {9, 10, 0},
        .blockType = vibecraft::world::BlockType::AirlockPanel,
    }));

    const auto extendedEnvironment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(12.5f, 10.0f, 0.5f),
        {},
        false);
    CHECK(extendedEnvironment.insideSafeZone);

    const auto outsideOutletEnvironment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(14.5f, 10.0f, 0.5f),
        {},
        false);
    CHECK_FALSE(outsideOutletEnvironment.insideSafeZone);
}

TEST_CASE("airlock panels stay hostile without relay power")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    for (int x = 1; x <= 8; ++x)
    {
        CHECK(world.applyEditCommand({
            .action = vibecraft::world::WorldEditAction::Place,
            .position = {x, 10, 0},
            .blockType = vibecraft::world::BlockType::PowerConduit,
        }));
    }
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {9, 10, 0},
        .blockType = vibecraft::world::BlockType::AirlockPanel,
    }));

    const auto environment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(12.5f, 10.0f, 0.5f),
        {},
        false);
    CHECK_FALSE(environment.insideSafeZone);
}

TEST_CASE("overlapping oxygen generators merge into a larger safe zone")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {8, 10, 0},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    const auto zones = vibecraft::app::collectOxygenSafeZones(
        world,
        glm::vec3(4.0f, 10.0f, 0.0f),
        32,
        4);
    REQUIRE_FALSE(zones.empty());
    CHECK(zones.front().radius > 7.0f);

    // Pick a location outside the base relay radius but within the merged bubble.
    const auto mergedEnvironment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        glm::vec3(4.5f, 10.0f, 9.0f),
        {},
        false);
    CHECK(mergedEnvironment.insideSafeZone);
}
