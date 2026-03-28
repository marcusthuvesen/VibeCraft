#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include <glm/vec3.hpp>

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/MobSpawnSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/multiplayer/Protocol.hpp"
#include "vibecraft/multiplayer/Session.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"
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

TEST_CASE("multiplayer protocol round-trips chunk and snapshot messages")
{
    using namespace vibecraft::multiplayer::protocol;

    ChunkSnapshotMessage chunk{
        .coord = {2, -3},
    };
    for (std::size_t i = 0; i < chunk.blocks.size(); ++i)
    {
        chunk.blocks[i] = static_cast<std::uint8_t>(i % 16);
    }

    const MessageHeader chunkHeader{
        .type = MessageType::ChunkSnapshot,
        .sequence = 42,
        .tick = 7,
    };
    const std::vector<std::uint8_t> encodedChunk = encodeMessage(chunkHeader, chunk);
    const std::optional<DecodedMessage> decodedChunk = decodeMessage(encodedChunk);
    REQUIRE(decodedChunk.has_value());
    CHECK(decodedChunk->header.type == MessageType::ChunkSnapshot);
    const auto* decodedChunkPayload = std::get_if<ChunkSnapshotMessage>(&decodedChunk->payload);
    REQUIRE(decodedChunkPayload != nullptr);
    CHECK(decodedChunkPayload->coord == chunk.coord);
    CHECK(decodedChunkPayload->blocks[0] == chunk.blocks[0]);
    CHECK(decodedChunkPayload->blocks[100] == chunk.blocks[100]);
    CHECK(decodedChunkPayload->blocks.back() == chunk.blocks.back());

    ServerSnapshotMessage snapshot{
        .serverTick = 123,
        .dayNightElapsedSeconds = 44.0f,
        .weatherElapsedSeconds = 12.0f,
        .players =
            {{
                .clientId = 1,
                .posX = 10.0f,
                .posY = 64.0f,
                .posZ = -5.0f,
                .yawDegrees = 90.0f,
                .pitchDegrees = -10.0f,
                .health = 18.0f,
                .air = 7.5f,
            }},
    };
    const MessageHeader snapshotHeader{
        .type = MessageType::ServerSnapshot,
        .sequence = 43,
        .tick = 8,
    };
    const std::vector<std::uint8_t> encodedSnapshot = encodeMessage(snapshotHeader, snapshot);
    const std::optional<DecodedMessage> decodedSnapshot = decodeMessage(encodedSnapshot);
    REQUIRE(decodedSnapshot.has_value());
    const auto* decodedSnapshotPayload = std::get_if<ServerSnapshotMessage>(&decodedSnapshot->payload);
    REQUIRE(decodedSnapshotPayload != nullptr);
    CHECK(decodedSnapshotPayload->serverTick == snapshot.serverTick);
    REQUIRE(decodedSnapshotPayload->players.size() == 1);
    CHECK(decodedSnapshotPayload->players[0].clientId == 1);
    CHECK(decodedSnapshotPayload->players[0].posX == doctest::Approx(10.0f));
}

TEST_CASE("host and client sessions establish and exchange snapshots")
{
    constexpr std::uint16_t kTestPort = 45123;
    vibecraft::multiplayer::HostSession host(std::make_unique<vibecraft::multiplayer::UdpTransport>());
    vibecraft::multiplayer::ClientSession client(std::make_unique<vibecraft::multiplayer::UdpTransport>());

    REQUIRE(host.start(kTestPort));
    REQUIRE(client.connect("127.0.0.1", kTestPort, "test-player"));

    bool connected = false;
    for (int i = 0; i < 200 && !connected; ++i)
    {
        host.poll();
        client.poll();
        connected = client.connected() && !host.clients().empty();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(connected);

    host.broadcastSnapshot({
        .serverTick = 55,
        .dayNightElapsedSeconds = 12.0f,
        .weatherElapsedSeconds = 8.0f,
        .players =
            {{
                .clientId = 0,
                .posX = 1.0f,
                .posY = 2.0f,
                .posZ = 3.0f,
                .yawDegrees = 45.0f,
                .pitchDegrees = 0.0f,
                .health = 20.0f,
                .air = 10.0f,
            }},
    });

    bool receivedSnapshot = false;
    for (int i = 0; i < 200 && !receivedSnapshot; ++i)
    {
        host.poll();
        client.poll();
        const auto snapshots = client.takeSnapshots();
        if (!snapshots.empty())
        {
            CHECK(snapshots.back().serverTick == 55);
            receivedSnapshot = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK(receivedSnapshot);
    client.disconnect();
    host.shutdown();
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

TEST_CASE("MobSpawnSystem clearAllMobs leaves empty enemy list")
{
    vibecraft::game::MobSpawnSystem sys;
    CHECK(sys.enemies().empty());
    sys.clearAllMobs();
    CHECK(sys.enemies().empty());
}

TEST_CASE("MobSpawnSystem does not spawn hostiles during daytime")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    world.generateRadius(terrainGenerator, 3);

    vibecraft::game::MobSpawnSettings settings;
    settings.spawnAttemptIntervalSeconds = 0.01f;
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

    CHECK(sys.enemies().empty());
}
