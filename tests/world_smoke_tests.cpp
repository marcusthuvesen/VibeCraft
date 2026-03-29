#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

#include <glm/vec3.hpp>

#include "vibecraft/app/Crafting.hpp"
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

TEST_CASE("terrain generator fast column fill matches per-block sampling")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    std::array<vibecraft::world::BlockType, vibecraft::world::kWorldHeight> columnBlocks{};

    const std::array<std::pair<int, int>, 3> sampleColumns{{{12, -9}, {-24, 31}, {7, 7}}};
    for (const auto& [worldX, worldZ] : sampleColumns)
    {
        terrainGenerator.fillColumn(worldX, worldZ, columnBlocks.data());
        for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
        {
            CHECK(columnBlocks[y - vibecraft::world::kWorldMinY] == terrainGenerator.blockTypeAt(worldX, y, worldZ));
        }
    }
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
    CHECK(vibecraft::world::textureTileIndex(BlockType::Lava, BlockFace::Side) == 13);
    CHECK(vibecraft::world::textureTileIndex(BlockType::OakPlanks, BlockFace::Side) == 17);
    CHECK(vibecraft::world::textureTileIndex(BlockType::CraftingTable, BlockFace::Top) == 20);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Chest, BlockFace::Top) == 27);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SnowGrass, BlockFace::Top) == 30);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SnowGrass, BlockFace::Side) == 31);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JungleGrass, BlockFace::Top) == 32);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JungleGrass, BlockFace::Side) == 33);
    CHECK(vibecraft::world::blockMetadata(BlockType::CoalOre).debugColor == 0xffffffff);
}

TEST_CASE("chunk mesher emits axis-aligned face normals")
{
    vibecraft::world::World world;
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {0, 10, 0},
        .blockType = vibecraft::world::BlockType::Stone,
    }));

    vibecraft::meshing::ChunkMesher mesher;
    const vibecraft::meshing::ChunkMeshData meshData = mesher.buildMesh(world, vibecraft::world::ChunkCoord{0, 0});
    REQUIRE(meshData.vertices.size() == 24);

    for (const vibecraft::meshing::DebugVertex& vertex : meshData.vertices)
    {
        const float axisMagnitude = std::abs(vertex.nx) + std::abs(vertex.ny) + std::abs(vertex.nz);
        CHECK(axisMagnitude == doctest::Approx(1.0f));
    }
}

TEST_CASE("crafting recipes cover inventory basics and workbench-only outputs")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots inventoryGrid{};
    inventoryGrid[0].blockType = BlockType::TreeTrunk;
    inventoryGrid[0].count = 1;
    const auto planksMatch = vibecraft::app::evaluateCraftingGrid(
        inventoryGrid,
        CraftingMode::Inventory2x2);
    REQUIRE(planksMatch.has_value());
    CHECK(planksMatch->output.blockType == BlockType::OakPlanks);
    CHECK(planksMatch->output.count == 4);

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
    CHECK(ovenMatch->output.blockType == BlockType::Oven);
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

TEST_CASE("terrain generator produces snowy, jungle, and temperate surface biomes")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundTemperateSurface = false;
    bool foundSnowySurface = false;
    bool foundJungleSurface = false;

    for (int worldX = -384; worldX <= 384; worldX += 12)
    {
        for (int worldZ = -384; worldZ <= 384; worldZ += 12)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surface, worldZ);
            foundTemperateSurface = foundTemperateSurface || surfaceBlock == BlockType::Grass;
            foundSnowySurface = foundSnowySurface || surfaceBlock == BlockType::SnowGrass;
            foundJungleSurface = foundJungleSurface || surfaceBlock == BlockType::JungleGrass;
            if (foundTemperateSurface && foundSnowySurface && foundJungleSurface)
            {
                break;
            }
        }
        if (foundTemperateSurface && foundSnowySurface && foundJungleSurface)
        {
            break;
        }
    }

    CHECK(foundTemperateSurface);
    CHECK(foundSnowySurface);
    CHECK(foundJungleSurface);
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
