#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/WeatherSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

TEST_CASE("bedrock floor uses five layers like Minecraft Java 1.18+ overworld bottom")
{
    CHECK(vibecraft::world::kMinecraftOverworldBedrockLayerCount == 5);
    CHECK(
        vibecraft::world::kBedrockFloorMaxY - vibecraft::world::kBedrockFloorMinY + 1
        == vibecraft::world::kMinecraftOverworldBedrockLayerCount);
}

TEST_CASE("lava and water are fluids for collision and face culling")
{
    using vibecraft::world::BlockType;
    CHECK(vibecraft::world::isFluid(BlockType::Water));
    CHECK(vibecraft::world::isFluid(BlockType::Lava));
    CHECK_FALSE(vibecraft::world::isSolid(BlockType::Lava));
    CHECK_FALSE(vibecraft::world::occludesFaces(BlockType::Lava));
}

TEST_CASE("single block creates six exposed faces")
{
    vibecraft::world::World world;
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::Stone,
    }));

    vibecraft::meshing::ChunkMesher mesher;
    world.rebuildDirtyMeshes(mesher);

    const auto statsIt = world.meshStats().find(vibecraft::world::ChunkCoord{0, 0});
    REQUIRE(statsIt != world.meshStats().end());
    CHECK(statsIt->second.faceCount == 6);
    CHECK(statsIt->second.vertexCount == 24);
    CHECK(statsIt->second.indexCount == 36);
}

TEST_CASE("terrain generator produces solid ground and non-solid space above it")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    const int surface = terrainGenerator.surfaceHeightAt(12, -9);

    CHECK(vibecraft::world::isSolid(terrainGenerator.blockTypeAt(12, surface, -9)));
    CHECK(!vibecraft::world::isSolid(terrainGenerator.blockTypeAt(12, surface + 1, -9)));
}

TEST_CASE("terrain generator carves underground non-solid cave space without breaking the surface")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundUndergroundCaveSpace = false;

    for (int worldX = -48; worldX <= 48 && !foundUndergroundCaveSpace; ++worldX)
    {
        for (int worldZ = -48; worldZ <= 48 && !foundUndergroundCaveSpace; ++worldZ)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);

            CHECK(vibecraft::world::isSolid(terrainGenerator.blockTypeAt(worldX, surface, worldZ)));
            CHECK(vibecraft::world::isSolid(terrainGenerator.blockTypeAt(worldX, surface - 1, worldZ)));

            for (int y = 5; y <= surface - 5; ++y)
            {
                if (!vibecraft::world::isSolid(terrainGenerator.blockTypeAt(worldX, y, worldZ)))
                {
                    foundUndergroundCaveSpace = true;
                    break;
                }
            }
        }
    }

    CHECK(foundUndergroundCaveSpace);
}

TEST_CASE("terrain generator keeps common layers broad and clusters rare ore")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundDeepslate = false;
    bool foundSandSurface = false;
    bool foundCoalCluster = false;
    std::size_t commonRockCount = 0;
    std::size_t coalOreCount = 0;

    for (int worldX = -32; worldX <= 32; ++worldX)
    {
        for (int worldZ = -32; worldZ <= 32; ++worldZ)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const vibecraft::world::BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surface, worldZ);
            foundSandSurface = foundSandSurface || surfaceBlock == vibecraft::world::BlockType::Sand;

            for (int y = 1; y <= surface - 4; ++y)
            {
                const vibecraft::world::BlockType blockType = terrainGenerator.blockTypeAt(worldX, y, worldZ);
                foundDeepslate = foundDeepslate || blockType == vibecraft::world::BlockType::Deepslate;

                if (blockType == vibecraft::world::BlockType::Stone || blockType == vibecraft::world::BlockType::Deepslate)
                {
                    ++commonRockCount;
                }

                if (blockType == vibecraft::world::BlockType::CoalOre)
                {
                    ++coalOreCount;

                    if (!foundCoalCluster)
                    {
                        foundCoalCluster =
                            terrainGenerator.blockTypeAt(worldX + 1, y, worldZ) == vibecraft::world::BlockType::CoalOre
                            || terrainGenerator.blockTypeAt(worldX - 1, y, worldZ) == vibecraft::world::BlockType::CoalOre
                            || terrainGenerator.blockTypeAt(worldX, y + 1, worldZ) == vibecraft::world::BlockType::CoalOre
                            || terrainGenerator.blockTypeAt(worldX, y - 1, worldZ) == vibecraft::world::BlockType::CoalOre
                            || terrainGenerator.blockTypeAt(worldX, y, worldZ + 1) == vibecraft::world::BlockType::CoalOre
                            || terrainGenerator.blockTypeAt(worldX, y, worldZ - 1) == vibecraft::world::BlockType::CoalOre;
                    }
                }
            }
        }
    }

    CHECK(foundDeepslate);
    CHECK(foundSandSurface);
    CHECK(coalOreCount > 0);
    CHECK(foundCoalCluster);
    CHECK(commonRockCount > coalOreCount * 12);
}

TEST_CASE("block metadata exposes stable texture tile indices for current block set")
{
    using vibecraft::world::BlockFace;
    using vibecraft::world::BlockType;

    CHECK(vibecraft::world::textureTileIndex(BlockType::Grass, BlockFace::Top) == 0);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Grass, BlockFace::Bottom) == 2);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Dirt, BlockFace::Side) == 2);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Stone, BlockFace::Top) == 3);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Sand, BlockFace::Side) == 7);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Water, BlockFace::Side) == 6);
    CHECK(vibecraft::world::blockMetadata(BlockType::CoalOre).debugColor == 0xffffffff);
}

TEST_CASE("deeper block families are harder and bedrock is unbreakable")
{
    using vibecraft::world::BlockType;

    CHECK(vibecraft::world::blockMetadata(BlockType::Dirt).hardness < vibecraft::world::blockMetadata(BlockType::Stone).hardness);
    CHECK(vibecraft::world::blockMetadata(BlockType::Stone).hardness < vibecraft::world::blockMetadata(BlockType::Deepslate).hardness);
    CHECK(vibecraft::world::blockMetadata(BlockType::Deepslate).hardness < vibecraft::world::blockMetadata(BlockType::Bedrock).hardness);
    CHECK_FALSE(vibecraft::world::blockMetadata(BlockType::Bedrock).breakable);
}

TEST_CASE("day night cycle keeps five minute daylight and five minute night timing")
{
    using vibecraft::game::DayNightCycle;

    CHECK(DayNightCycle::kDaylightDurationSeconds == doctest::Approx(300.0f));
    CHECK(DayNightCycle::kNightDurationSeconds == doctest::Approx(300.0f));
    CHECK(DayNightCycle::kFullCycleDurationSeconds == doctest::Approx(600.0f));
    CHECK(DayNightCycle::wrapCycleSeconds(660.0f) == doctest::Approx(60.0f));
    CHECK(DayNightCycle::wrapCycleSeconds(-30.0f) == doctest::Approx(570.0f));
}

TEST_CASE("day night cycle exposes dawn day dusk and night periods")
{
    using vibecraft::game::DayNightCycle;
    using vibecraft::game::TimeOfDayPeriod;

    CHECK(DayNightCycle::sampleAtElapsedSeconds(0.0f).period == TimeOfDayPeriod::Dawn);
    CHECK(DayNightCycle::sampleAtElapsedSeconds(120.0f).period == TimeOfDayPeriod::Day);
    CHECK(DayNightCycle::sampleAtElapsedSeconds(270.0f).period == TimeOfDayPeriod::Dusk);
    CHECK(DayNightCycle::sampleAtElapsedSeconds(420.0f).period == TimeOfDayPeriod::Night);
}

TEST_CASE("sun travels east to west across a full 360 degree orbit and moon stays opposite")
{
    using vibecraft::game::DayNightCycle;

    const vibecraft::game::DayNightSample sunrise = DayNightCycle::sampleAtElapsedSeconds(0.0f);
    CHECK(sunrise.sunOrbitDegrees360 == doctest::Approx(0.0f));
    CHECK(sunrise.sunDirection.x == doctest::Approx(1.0f));
    CHECK(std::abs(sunrise.sunDirection.y) < 0.0001f);
    CHECK(sunrise.moonOrbitDegrees360 == doctest::Approx(180.0f));
    CHECK(sunrise.moonDirection.x == doctest::Approx(-1.0f));

    const vibecraft::game::DayNightSample noon = DayNightCycle::sampleAtElapsedSeconds(150.0f);
    CHECK(noon.sunOrbitDegrees360 == doctest::Approx(90.0f));
    CHECK(std::abs(noon.sunDirection.x) < 0.0001f);
    CHECK(noon.sunDirection.y == doctest::Approx(1.0f));
    CHECK(noon.moonDirection.y == doctest::Approx(-1.0f));

    const vibecraft::game::DayNightSample sunset = DayNightCycle::sampleAtElapsedSeconds(300.0f);
    CHECK(sunset.sunOrbitDegrees360 == doctest::Approx(180.0f));
    CHECK(sunset.sunDirection.x == doctest::Approx(-1.0f));
    CHECK(sunset.moonDirection.x == doctest::Approx(1.0f));

    const vibecraft::game::DayNightSample midnight = DayNightCycle::sampleAtElapsedSeconds(450.0f);
    CHECK(midnight.sunOrbitDegrees360 == doctest::Approx(270.0f));
    CHECK(midnight.sunDirection.y == doctest::Approx(-1.0f));
    CHECK(midnight.moonOrbitDegrees360 == doctest::Approx(90.0f));
    CHECK(midnight.moonDirection.y == doctest::Approx(1.0f));
}

TEST_CASE("dawn and dusk expose warm tint data for future shader use")
{
    using vibecraft::game::DayNightCycle;

    const vibecraft::game::DayNightSample dawn = DayNightCycle::sampleAtElapsedSeconds(30.0f);
    const vibecraft::game::DayNightSample day = DayNightCycle::sampleAtElapsedSeconds(150.0f);
    const vibecraft::game::DayNightSample dusk = DayNightCycle::sampleAtElapsedSeconds(270.0f);
    const vibecraft::game::DayNightSample night = DayNightCycle::sampleAtElapsedSeconds(450.0f);

    CHECK(dawn.horizonTint.x > dawn.horizonTint.z);
    CHECK(dusk.horizonTint.x > dusk.horizonTint.z);
    CHECK(dawn.sunLightTint.z < day.sunLightTint.z);
    CHECK(dusk.sunLightTint.z < day.sunLightTint.z);
    CHECK(day.skyTint.z > day.skyTint.x);
    CHECK(night.moonLightTint.z > night.moonLightTint.x);
}

TEST_CASE("weather system alternates clear cloudy and rain phases over time")
{
    using vibecraft::game::WeatherSystem;
    using vibecraft::game::WeatherType;

    const vibecraft::game::WeatherSample clear = WeatherSystem::sampleAtElapsedSeconds(20.0f);
    const vibecraft::game::WeatherSample cloudy = WeatherSystem::sampleAtElapsedSeconds(150.0f);
    const vibecraft::game::WeatherSample rainy = WeatherSystem::sampleAtElapsedSeconds(260.0f);

    CHECK(clear.type == WeatherType::Clear);
    CHECK(clear.rainIntensity == doctest::Approx(0.0f));
    CHECK(clear.cloudCoverage < 0.35f);

    CHECK(cloudy.type == WeatherType::Cloudy);
    CHECK(cloudy.cloudCoverage > clear.cloudCoverage);
    CHECK(cloudy.rainIntensity < 0.4f);

    CHECK(rainy.type == WeatherType::Rain);
    CHECK(rainy.rainIntensity > 0.8f);
    CHECK(rainy.cloudCoverage > cloudy.cloudCoverage);
}

TEST_CASE("weather system wraps and smoothly transitions between presets")
{
    using vibecraft::game::WeatherSystem;

    CHECK(WeatherSystem::wrapCycleSeconds(
              WeatherSystem::kWeatherCycleDurationSeconds + 15.0f)
        == doctest::Approx(15.0f));

    const vibecraft::game::WeatherSample transition = WeatherSystem::sampleAtElapsedSeconds(110.0f);
    CHECK(transition.transitionProgress > 0.0f);
    CHECK(transition.transitionProgress < 1.0f);
    CHECK(transition.cloudCoverage > 0.18f);
    CHECK(transition.cloudCoverage < 0.55f);
}

TEST_CASE("terrain generator varies surface height and produces water")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    int minSurface = 999;
    int maxSurface = -999;
    bool foundWater = false;

    for (int worldX = -96; worldX <= 96; worldX += 8)
    {
        for (int worldZ = -96; worldZ <= 96; worldZ += 8)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            minSurface = std::min(minSurface, surface);
            maxSurface = std::max(maxSurface, surface);
            foundWater = foundWater
                || terrainGenerator.blockTypeAt(worldX, surface + 1, worldZ) == vibecraft::world::BlockType::Water;
        }
    }

    CHECK(maxSurface - minSurface >= 10);
    CHECK(foundWater);
}

TEST_CASE("world save and load round-trips edited blocks")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 1);
    REQUIRE(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, 40, 2},
        .blockType = vibecraft::world::BlockType::CoalOre,
    }));

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "vibecraft_world_smoke_test.bin";

    REQUIRE(world.save(tempPath));

    vibecraft::world::World loadedWorld;
    REQUIRE(loadedWorld.load(tempPath));
    CHECK(loadedWorld.blockAt(2, 40, 2) == vibecraft::world::BlockType::CoalOre);

    std::error_code errorCode;
    std::filesystem::remove(tempPath, errorCode);
}

TEST_CASE("world queries below y=0 return bedrock for collision")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 0);
    CHECK(world.blockAt(0, -1, 0) == vibecraft::world::BlockType::Bedrock);
    CHECK(world.blockAt(0, -50, 0) == vibecraft::world::BlockType::Bedrock);
}

TEST_CASE("loading a legacy save restores the bedrock floor")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::Chunk;
    using vibecraft::world::ChunkCoord;

    vibecraft::world::World legacyWorld;
    vibecraft::world::World::ChunkMap legacyChunks;
    Chunk legacyChunk(ChunkCoord{0, 0});
    legacyChunk.mutableBlockStorage().fill(BlockType::Air);
    legacyChunks.emplace(ChunkCoord{0, 0}, legacyChunk);
    legacyWorld.replaceChunks(std::move(legacyChunks));

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "vibecraft_world_bedrock_repair_test.bin";

    REQUIRE(legacyWorld.save(tempPath));

    vibecraft::world::World loadedWorld;
    REQUIRE(loadedWorld.load(tempPath));

    for (int y = vibecraft::world::kBedrockFloorMinY; y <= vibecraft::world::kBedrockFloorMaxY; ++y)
    {
        CHECK(loadedWorld.blockAt(0, y, 0) == BlockType::Bedrock);
    }
    CHECK(loadedWorld.blockAt(0, vibecraft::world::kUndergroundStartY, 0) == BlockType::Air);

    std::error_code errorCode;
    std::filesystem::remove(tempPath, errorCode);
}

TEST_CASE("world edit commands cannot break bedrock")
{
    vibecraft::world::World world;
    world.generateRadius(vibecraft::world::TerrainGenerator{}, 0);

    CHECK(world.blockAt(0, 0, 0) == vibecraft::world::BlockType::Bedrock);
    CHECK_FALSE(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Remove,
        .position = {0, 0, 0},
        .blockType = vibecraft::world::BlockType::Air,
    }));
    CHECK(world.blockAt(0, 0, 0) == vibecraft::world::BlockType::Bedrock);
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

TEST_CASE("player vitals use a Minecraft-style 20 health baseline")
{
    vibecraft::game::PlayerVitals vitals;
    CHECK(vitals.health() == doctest::Approx(20.0f));
    CHECK(vitals.maxHealth() == doctest::Approx(20.0f));
    CHECK(vitals.air() == doctest::Approx(10.0f));
    CHECK(vitals.maxAir() == doctest::Approx(10.0f));
}

TEST_CASE("player vitals apply fall damage only beyond three blocks")
{
    vibecraft::game::PlayerVitals vitals;

    CHECK(vitals.applyLandingImpact(3.0f, false) == doctest::Approx(0.0f));
    CHECK(vitals.applyLandingImpact(4.0f, false) == doctest::Approx(1.0f));
    CHECK(vitals.health() == doctest::Approx(19.0f));
}

TEST_CASE("player vitals ignore fall damage when landing in water")
{
    vibecraft::game::PlayerVitals vitals;

    CHECK(vitals.applyLandingImpact(18.0f, true) == doctest::Approx(0.0f));
    CHECK(vitals.health() == doctest::Approx(vitals.maxHealth()));
}

TEST_CASE("player vitals lose air underwater and start drowning after air is gone")
{
    vibecraft::game::PlayerVitals vitals;
    const vibecraft::game::EnvironmentalHazards underwater{
        .bodyInWater = true,
        .bodyInLava = false,
        .headSubmergedInWater = true,
    };

    vitals.tickEnvironment(10.0f, underwater);
    CHECK(vitals.air() == doctest::Approx(0.0f));
    CHECK(vitals.health() == doctest::Approx(vitals.maxHealth()));

    vitals.tickEnvironment(1.0f, underwater);
    CHECK(vitals.health() == doctest::Approx(18.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::Drowning);
}

TEST_CASE("player vitals take periodic lava damage and support future enemy damage")
{
    vibecraft::game::PlayerVitals vitals;
    const vibecraft::game::EnvironmentalHazards lava{
        .bodyInWater = false,
        .bodyInLava = true,
        .headSubmergedInWater = false,
    };

    vitals.tickEnvironment(0.5f, lava);
    CHECK(vitals.health() == doctest::Approx(16.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::Lava);

    CHECK(vitals.applyDamage({
        .cause = vibecraft::game::DamageCause::EnemyAttack,
        .amount = 3.0f,
    }) == doctest::Approx(3.0f));
    CHECK(vitals.health() == doctest::Approx(13.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::EnemyAttack);
}

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
