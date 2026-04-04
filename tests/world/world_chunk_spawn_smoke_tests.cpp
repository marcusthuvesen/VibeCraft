#include <optional>

#include <doctest/doctest.h>
#include <glm/vec3.hpp>
#include <algorithm>
#include <cmath>

#include "vibecraft/game/MobSpawnSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("generateMissingChunksAround adds only missing chunks near a center")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    world.generateRadius(terrainGenerator, 1);
    const std::size_t initialChunkCount = world.chunks().size();
    REQUIRE(initialChunkCount == 9);

    world.generateMissingChunksAround(terrainGenerator, vibecraft::world::ChunkCoord{3, -2}, 1);

    CHECK(world.chunks().size() > initialChunkCount);
    CHECK(world.chunks().contains(vibecraft::world::ChunkCoord{3, -2}));
    CHECK(world.chunks().contains(vibecraft::world::ChunkCoord{2, -3}));
    CHECK(world.blockAt(3 * vibecraft::world::Chunk::kSize, 0, -2 * vibecraft::world::Chunk::kSize)
        != vibecraft::world::BlockType::Air);
}

TEST_CASE("world chunk coordinate helpers match minecraft-style chunk math")
{
    using vibecraft::world::ChunkCoord;

    CHECK(vibecraft::world::worldToChunkCoord(0, 0) == ChunkCoord{0, 0});
    CHECK(vibecraft::world::worldToChunkCoord(15, 15) == ChunkCoord{0, 0});
    CHECK(vibecraft::world::worldToChunkCoord(16, 16) == ChunkCoord{1, 1});
    CHECK(vibecraft::world::worldToChunkCoord(-1, -1) == ChunkCoord{-1, -1});
    CHECK(vibecraft::world::worldToChunkCoord(-16, -16) == ChunkCoord{-1, -1});
    CHECK(vibecraft::world::worldToChunkCoord(-17, -17) == ChunkCoord{-2, -2});
    CHECK(vibecraft::world::worldToLocalCoord(0) == 0);
    CHECK(vibecraft::world::worldToLocalCoord(15) == 15);
    CHECK(vibecraft::world::worldToLocalCoord(16) == 0);
    CHECK(vibecraft::world::worldToLocalCoord(-1) == 15);
    CHECK(vibecraft::world::worldToLocalCoord(-16) == 0);
    CHECK(vibecraft::world::worldToLocalCoord(-17) == 15);
}

TEST_CASE("MobSpawnSystem clearAllMobs leaves empty mob list")
{
    vibecraft::game::MobSpawnSystem sys;
    CHECK(sys.mobs().empty());
    sys.clearAllMobs();
    CHECK(sys.mobs().empty());
}

TEST_CASE("MobSpawnSystem does not spawn hostiles during daytime")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    world.generateRadius(terrainGenerator, 3);

    vibecraft::game::MobSpawnSettings settings;
    settings.spawnAttemptIntervalSeconds = 0.01f;
    // Daytime would otherwise spawn passive animals on grass; this test targets hostile rules only.
    settings.maxPassiveMobsNearPlayer = 0;
    vibecraft::game::MobSpawnSystem sys(settings);
    sys.setRngSeedForTests(12'345u);

    vibecraft::game::PlayerVitals vitals;
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    const glm::vec3 playerFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};

    for (int i = 0; i < 400; ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            playerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Day,
            true,
            vitals);
    }

    CHECK(sys.mobs().empty());
}

TEST_CASE("MobSpawnSystem blocks hostile spawns near torches")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    world.generateRadius(terrainGenerator, 3);

    vibecraft::game::MobSpawnSettings settings;
    settings.spawnAttemptIntervalSeconds = 0.01f;
    settings.maxPassiveMobsNearPlayer = 0;
    settings.spawnMinHorizontalDistance = 4.0f;
    settings.spawnMaxHorizontalDistance = 6.0f;
    settings.hostileTorchExclusionRadius = 9.0f;
    vibecraft::game::MobSpawnSystem sys(settings);
    sys.setRngSeedForTests(99'123u);

    vibecraft::game::PlayerVitals vitals;
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    const glm::vec3 playerFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    REQUIRE(world.applyEditCommand(vibecraft::world::WorldEditCommand{
        .action = vibecraft::world::WorldEditAction::Place,
        .position = glm::ivec3{0, surfaceY + 1, 0},
        .blockType = vibecraft::world::BlockType::Torch,
    }));

    for (int i = 0; i < 400; ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            playerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Night,
            true,
            vitals);
    }

    CHECK(sys.mobs().empty());
}

TEST_CASE("MobSpawnSystem hostiles can climb a one-block ledge while chasing")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    world.generateRadius(terrainGenerator, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.spawnAttemptIntervalSeconds = 0.01f;
    settings.maxPassiveMobsNearPlayer = 0;
    settings.maxHostileMobsNearPlayer = 1;
    settings.spawnMinHorizontalDistance = 6.0f;
    settings.spawnMaxHorizontalDistance = 6.0f;
    settings.mobMoveSpeed = 3.0f;
    vibecraft::game::MobSpawnSystem sys(settings);
    sys.setRngSeedForTests(4'242u);

    vibecraft::game::PlayerVitals vitals;
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    const glm::vec3 playerFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};

    for (int i = 0; i < 600 && sys.mobs().empty(); ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            playerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Night,
            true,
            vitals);
    }
    REQUIRE(!sys.mobs().empty());

    const vibecraft::game::MobInstance spawned = sys.mobs().front();
    const std::uint32_t targetId = spawned.id;
    const float initialY = spawned.feetY;
    const int mobBlockX = static_cast<int>(std::floor(spawned.feetX));
    const int mobBlockZ = static_cast<int>(std::floor(spawned.feetZ));
    const float dxToPlayer = playerFeet.x - spawned.feetX;
    const float dzToPlayer = playerFeet.z - spawned.feetZ;
    const bool alongX = std::abs(dxToPlayer) >= std::abs(dzToPlayer);
    const int dirX = dxToPlayer >= 0.0f ? 1 : -1;
    const int dirZ = dzToPlayer >= 0.0f ? 1 : -1;

    const int wallBaseY = static_cast<int>(std::floor(spawned.feetY));
    for (int lateral = -2; lateral <= 2; ++lateral)
    {
        const int wx = alongX ? (mobBlockX + dirX) : (mobBlockX + lateral);
        const int wz = alongX ? (mobBlockZ + lateral) : (mobBlockZ + dirZ);
        REQUIRE(world.applyEditCommand(vibecraft::world::WorldEditCommand{
            .action = vibecraft::world::WorldEditAction::Place,
            .position = glm::ivec3{wx, wallBaseY, wz},
            .blockType = vibecraft::world::BlockType::Stone,
        }));
        REQUIRE(world.applyEditCommand(vibecraft::world::WorldEditCommand{
            .action = vibecraft::world::WorldEditAction::Remove,
            .position = glm::ivec3{wx, wallBaseY + 1, wz},
            .blockType = vibecraft::world::BlockType::Air,
        }));
        REQUIRE(world.applyEditCommand(vibecraft::world::WorldEditCommand{
            .action = vibecraft::world::WorldEditAction::Remove,
            .position = glm::ivec3{wx, wallBaseY + 2, wz},
            .blockType = vibecraft::world::BlockType::Air,
        }));
    }

    float maxObservedY = initialY;
    for (int i = 0; i < 180; ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            playerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Night,
            false,
            vitals);

        const auto it = std::find_if(
            sys.mobs().begin(),
            sys.mobs().end(),
            [targetId](const vibecraft::game::MobInstance& mob)
            {
                return mob.id == targetId;
            });
        REQUIRE(it != sys.mobs().end());
        maxObservedY = std::max(maxObservedY, it->feetY);
    }

    CHECK(maxObservedY >= initialY + 0.8f);
}

TEST_CASE("MobSpawnSystem hostiles can swim upward toward a player")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    world.generateRadius(terrainGenerator, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.spawnAttemptIntervalSeconds = 0.01f;
    settings.maxPassiveMobsNearPlayer = 0;
    settings.maxHostileMobsNearPlayer = 1;
    settings.spawnMinHorizontalDistance = 6.0f;
    settings.spawnMaxHorizontalDistance = 6.0f;
    settings.mobMoveSpeed = 3.0f;
    vibecraft::game::MobSpawnSystem sys(settings);
    sys.setRngSeedForTests(8'804u);

    vibecraft::game::PlayerVitals vitals;
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    const glm::vec3 spawnPlayerFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};

    for (int i = 0; i < 600 && sys.mobs().empty(); ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            spawnPlayerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Night,
            true,
            vitals);
    }
    REQUIRE(!sys.mobs().empty());

    const vibecraft::game::MobInstance spawned = sys.mobs().front();
    const std::uint32_t targetId = spawned.id;
    const int mobBlockX = static_cast<int>(std::floor(spawned.feetX));
    const int mobBlockY = static_cast<int>(std::floor(spawned.feetY));
    const int mobBlockZ = static_cast<int>(std::floor(spawned.feetZ));

    for (int z = mobBlockZ - 1; z <= mobBlockZ + 1; ++z)
    {
        for (int x = mobBlockX - 1; x <= mobBlockX + 1; ++x)
        {
            for (int y = mobBlockY; y <= mobBlockY + 3; ++y)
            {
                REQUIRE(world.applyEditCommand(vibecraft::world::WorldEditCommand{
                    .action = vibecraft::world::WorldEditAction::Place,
                    .position = glm::ivec3{x, y, z},
                    .blockType = vibecraft::world::BlockType::Water,
                }));
            }
        }
    }

    const glm::vec3 chasePlayerFeet{spawned.feetX, spawned.feetY + 4.0f, spawned.feetZ};
    float maxObservedY = spawned.feetY;
    for (int i = 0; i < 160; ++i)
    {
        sys.tick(
            world,
            terrainGenerator,
            chasePlayerFeet,
            0.3f,
            0.02f,
            vibecraft::game::TimeOfDayPeriod::Night,
            false,
            vitals);

        const auto it = std::find_if(
            sys.mobs().begin(),
            sys.mobs().end(),
            [targetId](const vibecraft::game::MobInstance& mob)
            {
                return mob.id == targetId;
            });
        REQUIRE(it != sys.mobs().end());
        maxObservedY = std::max(maxObservedY, it->feetY);
    }

    CHECK(maxObservedY >= spawned.feetY + 1.2f);
}

TEST_CASE("terrain generator biome override forces requested biome surface blocks")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x2468ace0U);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Jungle);

    const int jungleSurfaceY = terrainGenerator.surfaceHeightAt(128, -96);
    CHECK(terrainGenerator.surfaceBiomeAt(128, -96) == vibecraft::world::SurfaceBiome::Jungle);
    CHECK(terrainGenerator.blockTypeAt(128, jungleSurfaceY, -96) == vibecraft::world::BlockType::JungleGrass);

    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Desert);
    const int sandySurfaceY = terrainGenerator.surfaceHeightAt(128, -96);
    CHECK(terrainGenerator.surfaceBiomeAt(128, -96) == vibecraft::world::SurfaceBiome::Desert);
    CHECK(terrainGenerator.blockTypeAt(128, sandySurfaceY, -96) == vibecraft::world::BlockType::Sand);

    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Forest);
    const int temperateSurfaceY = terrainGenerator.surfaceHeightAt(128, -96);
    CHECK(terrainGenerator.surfaceBiomeAt(128, -96) == vibecraft::world::SurfaceBiome::Forest);
    CHECK(terrainGenerator.blockTypeAt(128, temperateSurfaceY, -96) == vibecraft::world::BlockType::Grass);
    CHECK((jungleSurfaceY != sandySurfaceY || sandySurfaceY != temperateSurfaceY));

    terrainGenerator.setBiomeOverride(std::nullopt);
    CHECK(terrainGenerator.biomeOverride() == std::nullopt);
}
