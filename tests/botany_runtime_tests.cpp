#include <doctest/doctest.h>

#include "vibecraft/app/ApplicationBotanyRuntime.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("fiber saplings require planter trays")
{
    vibecraft::world::World world;

    const auto noTray = vibecraft::app::validateBotanyBlockPlacement(
        world,
        {0, 11, 0},
        vibecraft::world::BlockType::FiberSapling,
        glm::vec3(0.0f, 10.0f, 0.0f),
        false);
    CHECK_FALSE(noTray.allowed);

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::PlanterTray,
    }));
    const auto withTray = vibecraft::app::validateBotanyBlockPlacement(
        world,
        {0, 11, 0},
        vibecraft::world::BlockType::FiberSapling,
        glm::vec3(0.0f, 10.0f, 0.0f),
        false);
    CHECK(withTray.allowed);
}

TEST_CASE("fiber saplings grow into trees inside relay zones")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::world::World world;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::PlanterTray,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 11, 0},
        .blockType = vibecraft::world::BlockType::FiberSapling,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 2},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    vibecraft::app::BotanyRuntimeState runtimeState{};
    const vibecraft::app::BotanyTickResult tickResult = vibecraft::app::tickLocalBotany(
        46.0f,
        world,
        terrainGenerator,
        glm::vec3(0.0f, 10.0f, 0.0f),
        runtimeState);

    CHECK(tickResult.treesGrown > 0);
    CHECK(world.blockAt(0, 11, 0) == vibecraft::world::BlockType::OakLog);
}

TEST_CASE("fiber saplings become sprouts before full tree growth")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::world::World world;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::PlanterTray,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 11, 0},
        .blockType = vibecraft::world::BlockType::FiberSapling,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 2},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    vibecraft::app::BotanyRuntimeState runtimeState{};
    const vibecraft::app::BotanyTickResult tickResult = vibecraft::app::tickLocalBotany(
        20.0f,
        world,
        terrainGenerator,
        glm::vec3(0.0f, 10.0f, 0.0f),
        runtimeState);

    CHECK(tickResult.treesGrown == 0);
    CHECK(world.blockAt(0, 11, 0) == vibecraft::world::BlockType::FiberSprout);
}

TEST_CASE("greenhouse structure speeds fiber growth")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::world::World world;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::PlanterTray,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 11, 0},
        .blockType = vibecraft::world::BlockType::FiberSapling,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 2},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {-2, 11, -2},
        .blockType = vibecraft::world::BlockType::GreenhouseGlass,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, 11, -2},
        .blockType = vibecraft::world::BlockType::GreenhouseGlass,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {-2, 12, -2},
        .blockType = vibecraft::world::BlockType::HabitatFrame,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, 12, 2},
        .blockType = vibecraft::world::BlockType::HabitatFrame,
    }));

    vibecraft::app::BotanyRuntimeState runtimeState{};
    const vibecraft::app::BotanyTickResult tickResult = vibecraft::app::tickLocalBotany(
        40.0f,
        world,
        terrainGenerator,
        glm::vec3(0.0f, 10.0f, 0.0f),
        runtimeState);

    CHECK(tickResult.treesGrown > 0);
    CHECK(world.blockAt(0, 11, 0) == vibecraft::world::BlockType::OakLog);
}

TEST_CASE("power conduits contribute to greenhouse growth speed")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::world::World world;

    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::PlanterTray,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 11, 0},
        .blockType = vibecraft::world::BlockType::FiberSapling,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 2},
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {-2, 11, -2},
        .blockType = vibecraft::world::BlockType::PowerConduit,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, 11, -2},
        .blockType = vibecraft::world::BlockType::PowerConduit,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {-2, 11, 2},
        .blockType = vibecraft::world::BlockType::PowerConduit,
    }));

    vibecraft::app::BotanyRuntimeState runtimeState{};
    const vibecraft::app::BotanyTickResult tickResult = vibecraft::app::tickLocalBotany(
        40.0f,
        world,
        terrainGenerator,
        glm::vec3(0.0f, 10.0f, 0.0f),
        runtimeState);

    CHECK(tickResult.treesGrown > 0);
    CHECK(world.blockAt(0, 11, 0) == vibecraft::world::BlockType::OakLog);
}
