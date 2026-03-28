#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

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

            for (int y = 4; y <= surface - 5; ++y)
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
    CHECK(vibecraft::world::blockMetadata(BlockType::CoalOre).debugColor == 0xff607d8b);
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
