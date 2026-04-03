#include <optional>

#include <doctest/doctest.h>
#include <glm/vec3.hpp>

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
