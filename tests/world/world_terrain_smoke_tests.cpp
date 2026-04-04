#include <algorithm>
#include <array>
#include <limits>

#include <doctest/doctest.h>

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

TEST_CASE("terrain generator spans dramatic elevation across macro terrain samples")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    int minSurface = std::numeric_limits<int>::max();
    int maxSurface = std::numeric_limits<int>::min();

    for (int worldX = -512; worldX <= 512; worldX += 32)
    {
        for (int worldZ = -512; worldZ <= 512; worldZ += 32)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            minSurface = std::min(minSurface, surface);
            maxSurface = std::max(maxSurface, surface);
        }
    }

    CHECK(maxSurface - minSurface >= 20);
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

TEST_CASE("terrain generator places cave water and deep lava like Minecraft")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundCaveWater = false;
    bool foundDeepLava = false;

    for (int worldX = -96; worldX <= 96 && (!foundCaveWater || !foundDeepLava); worldX += 2)
    {
        for (int worldZ = -96; worldZ <= 96 && (!foundCaveWater || !foundDeepLava); worldZ += 2)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            for (int y = vibecraft::world::kWorldMinY + 8; y <= surface - 8; ++y)
            {
                const auto blockType = terrainGenerator.blockTypeAt(worldX, y, worldZ);
                foundCaveWater = foundCaveWater || blockType == vibecraft::world::BlockType::Water;
                foundDeepLava = foundDeepLava || (blockType == vibecraft::world::BlockType::Lava && y <= -20);
            }
        }
    }

    CHECK(foundCaveWater);
    CHECK(foundDeepLava);
}

TEST_CASE("world fluids fall spread and lava cools against water")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::World;
    using vibecraft::world::WorldEditAction;
    using vibecraft::world::WorldEditCommand;

    World world;
    CHECK(world.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 9, 0}, .blockType = BlockType::Stone}));
    CHECK(world.applyEditCommand({.action = WorldEditAction::Place, .position = {1, 9, 0}, .blockType = BlockType::Stone}));
    CHECK(world.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 10, 0}, .blockType = BlockType::Water}));
    world.tickFluids(64);
    CHECK(world.blockAt(1, 10, 0) == BlockType::Water);

    World waterfallWorld;
    CHECK(waterfallWorld.applyEditCommand(
        {.action = WorldEditAction::Place, .position = {0, 12, 0}, .blockType = BlockType::Water}));
    waterfallWorld.tickFluids(64);
    waterfallWorld.tickFluids(64);
    CHECK(waterfallWorld.blockAt(0, 11, 0) == BlockType::Water);

    World lavaWorld;
    CHECK(lavaWorld.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 9, 0}, .blockType = BlockType::Stone}));
    CHECK(lavaWorld.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 10, 0}, .blockType = BlockType::Lava}));
    CHECK(lavaWorld.applyEditCommand({.action = WorldEditAction::Place, .position = {1, 10, 0}, .blockType = BlockType::Water}));
    lavaWorld.tickFluids(64);
    const BlockType cooledBlock = lavaWorld.blockAt(0, 10, 0);
    const bool isExpectedCooledBlock =
        cooledBlock == BlockType::Obsidian || cooledBlock == BlockType::Cobblestone;
    CHECK(isExpectedCooledBlock);
}

TEST_CASE("tree crown leaves decay after the rooted trunk base is removed")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::World;
    using vibecraft::world::WorldEditAction;
    using vibecraft::world::WorldEditCommand;

    World world;
    REQUIRE(world.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 9, 0}, .blockType = BlockType::Dirt}));
    REQUIRE(world.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 10, 0}, .blockType = BlockType::OakLog}));
    REQUIRE(world.applyEditCommand({.action = WorldEditAction::Place, .position = {0, 11, 0}, .blockType = BlockType::OakLog}));

    constexpr std::array<glm::ivec3, 5> kCrownLeaves{{
        {0, 12, 0},
        {1, 12, 0},
        {-1, 12, 0},
        {0, 12, 1},
        {0, 12, -1},
    }};
    for (const glm::ivec3& pos : kCrownLeaves)
    {
        REQUIRE(world.applyEditCommand({.action = WorldEditAction::Place, .position = pos, .blockType = BlockType::OakLeaves}));
    }

    REQUIRE(world.applyEditCommand({.action = WorldEditAction::Remove, .position = {0, 10, 0}, .blockType = BlockType::Air}));
    CHECK(world.blockAt(0, 12, 0) == BlockType::OakLeaves);

    for (int i = 0; i < 24; ++i)
    {
        world.tickLeafDecay(2);
    }

    for (const glm::ivec3& pos : kCrownLeaves)
    {
        CHECK(world.blockAt(pos.x, pos.y, pos.z) == BlockType::Air);
    }
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

    // Spawn starter area forces forest woodland (no desert sand at the surface). With a desert biome override, topsoil
    // still includes sand (surface block may be stone on high mountains).
    if (!foundSandSurface)
    {
        vibecraft::world::TerrainGenerator sandyProbe;
        sandyProbe.setWorldSeed(terrainGenerator.worldSeed());
        sandyProbe.setBiomeOverride(vibecraft::world::SurfaceBiome::Desert);
        for (int worldX = -32; worldX <= 32 && !foundSandSurface; ++worldX)
        {
            for (int worldZ = -32; worldZ <= 32 && !foundSandSurface; ++worldZ)
            {
                const int surface = sandyProbe.surfaceHeightAt(worldX, worldZ);
                for (int y = 1; y <= surface; ++y)
                {
                    if (sandyProbe.blockTypeAt(worldX, y, worldZ) == vibecraft::world::BlockType::Sand)
                    {
                        foundSandSurface = true;
                        break;
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
    CHECK(vibecraft::world::textureTileIndex(BlockType::CraftingTable, BlockFace::Bottom) == 23);
    CHECK(vibecraft::world::textureTileIndex(BlockType::CraftingTable, BlockFace::South) == 22);
    CHECK(vibecraft::world::textureTileIndex(BlockType::CraftingTable, BlockFace::East) == 21);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Chest, BlockFace::Top) == 27);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Chest, BlockFace::South) == 28);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Chest, BlockFace::East) == 29);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Furnace, BlockFace::South) == 26);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Furnace, BlockFace::East) == 25);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FurnaceNorth, BlockFace::North) == 26);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FurnaceEast, BlockFace::East) == 26);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FurnaceWest, BlockFace::West) == 26);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SnowGrass, BlockFace::Top) == 30);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SnowGrass, BlockFace::Side) == 31);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JungleGrass, BlockFace::Top) == 32);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JungleGrass, BlockFace::Side) == 33);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JunglePlanks, BlockFace::Side) == 65);
    CHECK(vibecraft::world::textureTileIndex(BlockType::MossBlock, BlockFace::Top) == 66);
    CHECK(vibecraft::world::textureTileIndex(BlockType::MossyCobblestone, BlockFace::Side) == 67);
    CHECK(vibecraft::world::textureTileIndex(BlockType::OakDoor, BlockFace::Side) == 108);
    CHECK(vibecraft::world::textureTileIndex(BlockType::OakDoorUpperNorth, BlockFace::Side) == 109);
    CHECK(vibecraft::world::textureTileIndex(BlockType::JungleDoor, BlockFace::Side) == 110);
    CHECK(vibecraft::world::textureTileIndex(BlockType::IronDoorUpperNorth, BlockFace::Side) == 113);
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

TEST_CASE("door collision slabs rotate when the door opens")
{
    using vibecraft::world::BlockType;

    const vibecraft::world::BlockCollisionBox closedBox =
        vibecraft::world::collisionBoxForBlockType(BlockType::OakDoorLowerNorth);
    const vibecraft::world::BlockCollisionBox openBox =
        vibecraft::world::collisionBoxForBlockType(BlockType::OakDoorLowerNorthOpen);

    CHECK(closedBox.maxZ - closedBox.minZ < 0.25f);
    CHECK(closedBox.maxX - closedBox.minX == doctest::Approx(1.0f));
    CHECK(openBox.maxX - openBox.minX < 0.25f);
    CHECK(openBox.maxZ - openBox.minZ == doctest::Approx(1.0f));
}

TEST_CASE("door facing variants preserve axis and provide mirrored pairing states")
{
    using vibecraft::world::DoorFacing;

    CHECK(vibecraft::world::doorUsesXAxisPlane(DoorFacing::East));
    CHECK(vibecraft::world::doorUsesXAxisPlane(DoorFacing::West));
    CHECK_FALSE(vibecraft::world::doorUsesXAxisPlane(DoorFacing::North));
    CHECK_FALSE(vibecraft::world::doorUsesXAxisPlane(DoorFacing::South));
    CHECK(vibecraft::world::oppositeDoorFacingWithinAxis(DoorFacing::North) == DoorFacing::South);
    CHECK(vibecraft::world::oppositeDoorFacingWithinAxis(DoorFacing::South) == DoorFacing::North);
    CHECK(vibecraft::world::oppositeDoorFacingWithinAxis(DoorFacing::East) == DoorFacing::West);
    CHECK(vibecraft::world::oppositeDoorFacingWithinAxis(DoorFacing::West) == DoorFacing::East);
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

    // Minecraft-like plains around spawn should still roll gently without the exaggerated older terrain swings.
    CHECK(maxSurface - minSurface >= 5);
    CHECK(foundWater);
}

TEST_CASE("terrain generator produces snowy, jungle, and forested surface materials")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundForestSurface = false;
    bool foundSnowySurface = false;
    bool foundJungleSurface = false;

    for (int worldX = -16384; worldX <= 16384; worldX += 128)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 128)
        {
            const int surface = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surface, worldZ);
            foundForestSurface = foundForestSurface || surfaceBlock == BlockType::Grass
                || surfaceBlock == BlockType::Podzol
                || surfaceBlock == BlockType::CoarseDirt;
            foundSnowySurface = foundSnowySurface || surfaceBlock == BlockType::SnowGrass;
            foundJungleSurface = foundJungleSurface || surfaceBlock == BlockType::JungleGrass;
            if (foundForestSurface && foundSnowySurface && foundJungleSurface)
            {
                break;
            }
        }
        if (foundForestSurface && foundSnowySurface && foundJungleSurface)
        {
            break;
        }
    }

    CHECK(foundForestSurface);
    CHECK(foundSnowySurface);
    CHECK(foundJungleSurface);
}

TEST_CASE("forest terrain generates dense woodland pockets by default")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Forest);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 8);

    const auto isTreeLog = [](const BlockType blockType)
    {
        return blockType == BlockType::OakLog
            || blockType == BlockType::BirchLog
            || blockType == BlockType::DarkOakLog
            || blockType == BlockType::SpruceLog
            || blockType == BlockType::JungleLog;
    };

    int treeColumnCount = 0;
    for (int worldX = -128; worldX <= 128; ++worldX)
    {
        for (int worldZ = -128; worldZ <= 128; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (isTreeLog(world.blockAt(worldX, surfaceY + 1, worldZ))
                || isTreeLog(world.blockAt(worldX, surfaceY + 2, worldZ))
                || isTreeLog(world.blockAt(worldX, surfaceY + 3, worldZ)))
            {
                ++treeColumnCount;
            }
        }
    }

    CHECK(treeColumnCount >= 120);
}

TEST_CASE("forest terrain generates birch trees and fern undergrowth")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Forest);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundBirch = false;
    bool foundFern = false;
    for (int worldX = -160; worldX <= 160 && (!foundBirch || !foundFern); ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && (!foundBirch || !foundFern); ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundBirch = foundBirch
                || world.blockAt(worldX, surfaceY + 1, worldZ) == BlockType::BirchLog
                || world.blockAt(worldX, surfaceY + 2, worldZ) == BlockType::BirchLog
                || world.blockAt(worldX, surfaceY + 3, worldZ) == BlockType::BirchLeaves;
            foundFern = foundFern || world.blockAt(worldX, surfaceY + 1, worldZ) == BlockType::Fern;
        }
    }

    CHECK(foundBirch);
    CHECK(foundFern);
}

TEST_CASE("taiga terrain generates spruce woods with podzol or coarse dirt patches")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Taiga);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundSpruce = false;
    bool foundForestFloorPatch = false;
    for (int worldX = -160; worldX <= 160 && (!foundSpruce || !foundForestFloorPatch); ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && (!foundSpruce || !foundForestFloorPatch); ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const BlockType surfaceBlock = world.blockAt(worldX, surfaceY, worldZ);
            foundSpruce = foundSpruce
                || world.blockAt(worldX, surfaceY + 1, worldZ) == BlockType::SpruceLog
                || world.blockAt(worldX, surfaceY + 2, worldZ) == BlockType::SpruceLog
                || world.blockAt(worldX, surfaceY + 3, worldZ) == BlockType::SpruceLeaves;
            foundForestFloorPatch = foundForestFloorPatch
                || surfaceBlock == BlockType::Podzol
                || surfaceBlock == BlockType::CoarseDirt;
        }
    }

    CHECK(foundSpruce);
    CHECK(foundForestFloorPatch);
}

TEST_CASE("ore distribution favors minecraft-like underground depth bands")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    std::size_t highCoalCount = 0;
    std::size_t deepCoalCount = 0;
    std::size_t midIronCount = 0;
    std::size_t deepIronCount = 0;
    std::size_t shallowGoldCount = 0;
    std::size_t deepGoldCount = 0;
    std::size_t upperDiamondCount = 0;
    std::size_t deepDiamondCount = 0;

    for (int worldX = -96; worldX <= 96; worldX += 2)
    {
        for (int worldZ = -96; worldZ <= 96; worldZ += 2)
        {
            for (int y = -60; y <= 96; y += 2)
            {
                const BlockType blockType = terrainGenerator.blockTypeAt(worldX, y, worldZ);
                switch (blockType)
                {
                case BlockType::CoalOre:
                    if (y >= 32 && y <= 96)
                    {
                        ++highCoalCount;
                    }
                    if (y >= -60 && y <= -16)
                    {
                        ++deepCoalCount;
                    }
                    break;
                case BlockType::IronOre:
                    if (y >= -8 && y <= 40)
                    {
                        ++midIronCount;
                    }
                    if (y >= -60 && y <= -32)
                    {
                        ++deepIronCount;
                    }
                    break;
                case BlockType::GoldOre:
                    if (y >= 8 && y <= 32)
                    {
                        ++shallowGoldCount;
                    }
                    if (y >= -48 && y <= -8)
                    {
                        ++deepGoldCount;
                    }
                    break;
                case BlockType::DiamondOre:
                    if (y >= -16 && y <= 16)
                    {
                        ++upperDiamondCount;
                    }
                    if (y >= -60 && y <= -32)
                    {
                        ++deepDiamondCount;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    CHECK(highCoalCount > 0);
    CHECK(midIronCount > 0);
    CHECK(deepGoldCount > 0);
    CHECK(deepDiamondCount > 0);
    CHECK(highCoalCount > deepCoalCount);
    CHECK(midIronCount > deepIronCount);
    CHECK(deepGoldCount > shallowGoldCount);
    CHECK(deepDiamondCount > upperDiamondCount);
}

TEST_CASE("world generation decorates caves into lush dripstone and deep dark regions")
{
    using vibecraft::world::BlockType;

    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundLushCave = false;
    bool foundDripstoneCave = false;
    bool foundDeepDark = false;

    for (int worldX = -160; worldX <= 160 && (!foundLushCave || !foundDripstoneCave || !foundDeepDark); ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && (!foundLushCave || !foundDripstoneCave || !foundDeepDark); ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            for (int y = -60; y <= surfaceY - 18 && (!foundLushCave || !foundDripstoneCave || !foundDeepDark); ++y)
            {
                const BlockType blockType = world.blockAt(worldX, y, worldZ);
                if (blockType == BlockType::MossBlock && y <= surfaceY - 20)
                {
                    foundLushCave = true;
                }
                else if (blockType == BlockType::DripstoneBlock)
                {
                    foundDripstoneCave = true;
                }
                else if (blockType == BlockType::SculkBlock && y <= -24)
                {
                    foundDeepDark = true;
                }
            }
        }
    }

    CHECK(foundLushCave);
    CHECK(foundDripstoneCave);
    CHECK(foundDeepDark);
}
