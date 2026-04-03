#include <array>
#include <vector>

#include <doctest/doctest.h>
#include <glm/vec3.hpp>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("world raycast hits the first solid block and reports the previous empty cell")
{
    vibecraft::world::World world;
    REQUIRE(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, 10, 0},
        .blockType = vibecraft::world::BlockType::Stone,
    }));

    const auto hit = world.raycast(
        glm::vec3{0.5f, 10.5f, 0.5f},
        glm::vec3{1.0f, 0.0f, 0.0f},
        10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->solidBlock == glm::ivec3(2, 10, 0));
    CHECK(hit->buildTarget == glm::ivec3(1, 10, 0));
    CHECK(hit->blockType == vibecraft::world::BlockType::Stone);
}

TEST_CASE("world queries below the supported world depth return bedrock for collision")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 0);
    CHECK(world.blockAt(0, vibecraft::world::kWorldMinY - 1, 0) == vibecraft::world::BlockType::Bedrock);
    CHECK(world.blockAt(0, vibecraft::world::kWorldMinY - 50, 0) == vibecraft::world::BlockType::Bedrock);
}

TEST_CASE("world edit commands cannot break bedrock")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 0);
    constexpr int kBedrockY = vibecraft::world::kBedrockFloorMinY;

    CHECK(world.blockAt(0, kBedrockY, 0) == vibecraft::world::BlockType::Bedrock);
    CHECK_FALSE(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Remove,
        .position = {0, kBedrockY, 0},
        .blockType = vibecraft::world::BlockType::Air,
    }));
    CHECK(world.blockAt(0, kBedrockY, 0) == vibecraft::world::BlockType::Bedrock);
}

TEST_CASE("rebuildDirtyMeshes can process a dirty subset")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 1);
    const std::size_t initialDirtyChunkCount = world.dirtyChunkCount();
    REQUIRE(initialDirtyChunkCount > 0);

    const std::vector<vibecraft::world::ChunkCoord> dirtyCoords = world.dirtyChunkCoords();
    REQUIRE(!dirtyCoords.empty());

    vibecraft::meshing::ChunkMesher mesher;
    const std::array<vibecraft::world::ChunkCoord, 1> selectedCoords{dirtyCoords.front()};
    world.rebuildDirtyMeshes(mesher, selectedCoords);

    CHECK(world.dirtyChunkCount() == initialDirtyChunkCount - 1);
}
