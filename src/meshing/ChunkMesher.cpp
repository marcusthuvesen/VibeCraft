#include "vibecraft/meshing/ChunkMesher.hpp"

#include <array>
#include <limits>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::meshing
{
namespace
{
using vibecraft::world::BlockType;
using BlockStorage = std::array<BlockType, vibecraft::world::Chunk::kBlockCount>;

struct FaceDefinition
{
    int offsetX;
    int offsetY;
    int offsetZ;
    vibecraft::world::BlockFace blockFace;
    std::array<std::array<float, 3>, 4> corners;
    std::array<std::array<float, 2>, 4> uvs;
};

constexpr std::array<FaceDefinition, 6> kFaces{{
    {1, 0, 0, vibecraft::world::BlockFace::Side,
     {{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {-1, 0, 0, vibecraft::world::BlockFace::Side,
     {{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {0, 1, 0, vibecraft::world::BlockFace::Top,
     {{{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}}}},
    {0, -1, 0, vibecraft::world::BlockFace::Bottom,
     {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}}}},
    {0, 0, 1, vibecraft::world::BlockFace::Side,
     {{{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {0, 0, -1, vibecraft::world::BlockFace::Side,
     {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
}};

[[nodiscard]] constexpr bool usesCrossPlantMesh(const BlockType blockType)
{
    return blockType == BlockType::Dandelion || blockType == BlockType::Poppy
        || blockType == BlockType::BlueOrchid || blockType == BlockType::Allium
        || blockType == BlockType::OxeyeDaisy || blockType == BlockType::BrownMushroom
        || blockType == BlockType::RedMushroom || blockType == BlockType::Vines
        || blockType == BlockType::Bamboo;
}

constexpr std::uint16_t kAtlasColumns = vibecraft::kChunkAtlasTileColumns;
constexpr std::uint16_t kAtlasRows = vibecraft::kChunkAtlasTileRows;
constexpr float kTileInsetU = 0.5f / static_cast<float>(vibecraft::kChunkAtlasWidthPx);
constexpr float kTileInsetV = 0.5f / static_cast<float>(vibecraft::kChunkAtlasHeightPx);
[[nodiscard]] std::array<float, 2> atlasUvForBlockType(
    const std::uint8_t tileIndex,
    const std::array<float, 2>& faceUv)
{
    const float tileWidth = 1.0f / static_cast<float>(kAtlasColumns);
    const float tileHeight = 1.0f / static_cast<float>(kAtlasRows);
    const float tileX = static_cast<float>(tileIndex % kAtlasColumns);
    const float tileY = static_cast<float>(tileIndex / kAtlasColumns);
    const float minU = tileX * tileWidth + kTileInsetU;
    const float maxU = (tileX + 1.0f) * tileWidth - kTileInsetU;
    const float minV = tileY * tileHeight + kTileInsetV;
    const float maxV = (tileY + 1.0f) * tileHeight - kTileInsetV;
    const float u = minU + (maxU - minU) * faceUv[0];
    const float v = minV + (maxV - minV) * faceUv[1];
    return {u, v};
}

[[nodiscard]] constexpr std::size_t chunkStorageIndex(
    const int localX,
    const int y,
    const int localZ)
{
    const int localY = y - vibecraft::world::kWorldMinY;
    return static_cast<std::size_t>(localY * vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize
        + localZ * vibecraft::world::Chunk::kSize
        + localX);
}

[[nodiscard]] BlockType blockFromStorage(
    const BlockStorage& storage,
    const int localX,
    const int y,
    const int localZ)
{
    return storage[chunkStorageIndex(localX, y, localZ)];
}

[[nodiscard]] BlockType sampledNeighborBlock(
    const BlockStorage& currentStorage,
    const vibecraft::world::Chunk* const westChunk,
    const vibecraft::world::Chunk* const eastChunk,
    const vibecraft::world::Chunk* const northChunk,
    const vibecraft::world::Chunk* const southChunk,
    const int localX,
    const int y,
    const int localZ)
{
    if (y < vibecraft::world::kWorldMinY)
    {
        return BlockType::Bedrock;
    }
    if (y > vibecraft::world::kWorldMaxY)
    {
        return BlockType::Air;
    }
    if (localX >= 0 && localX < vibecraft::world::Chunk::kSize
        && localZ >= 0 && localZ < vibecraft::world::Chunk::kSize)
    {
        return blockFromStorage(currentStorage, localX, y, localZ);
    }
    if (localX < 0)
    {
        return westChunk != nullptr
            ? westChunk->blockAt(vibecraft::world::Chunk::kSize - 1, y, localZ)
            : BlockType::Air;
    }
    if (localX >= vibecraft::world::Chunk::kSize)
    {
        return eastChunk != nullptr ? eastChunk->blockAt(0, y, localZ) : BlockType::Air;
    }
    if (localZ < 0)
    {
        return northChunk != nullptr
            ? northChunk->blockAt(localX, y, vibecraft::world::Chunk::kSize - 1)
            : BlockType::Air;
    }
    return southChunk != nullptr ? southChunk->blockAt(localX, y, 0) : BlockType::Air;
}
}  // namespace

ChunkMeshData ChunkMesher::buildMesh(
    const vibecraft::world::World& world,
    const vibecraft::world::ChunkCoord& coord) const
{
    ChunkMeshData meshData;
    const auto currentChunkIt = world.chunks().find(coord);
    if (currentChunkIt == world.chunks().end())
    {
        return meshData;
    }

    const auto chunkAt = [&world](const vibecraft::world::ChunkCoord& neighborCoord)
    {
        const auto chunkIt = world.chunks().find(neighborCoord);
        return chunkIt != world.chunks().end() ? &chunkIt->second : nullptr;
    };
    const vibecraft::world::Chunk* const westChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x - 1, coord.z});
    const vibecraft::world::Chunk* const eastChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x + 1, coord.z});
    const vibecraft::world::Chunk* const northChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x, coord.z - 1});
    const vibecraft::world::Chunk* const southChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x, coord.z + 1});
    const BlockStorage& currentStorage = currentChunkIt->second.blockStorage();
    constexpr int kNoRenderableBlock = std::numeric_limits<int>::min();
    std::array<int, vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize> columnMinY;
    std::array<int, vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize> columnMaxY;
    columnMinY.fill(kNoRenderableBlock);
    columnMaxY.fill(kNoRenderableBlock);

    for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
        {
            const std::size_t columnIndex = static_cast<std::size_t>(localZ * vibecraft::world::Chunk::kSize + localX);
            for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
            {
                if (!vibecraft::world::isRenderable(blockFromStorage(currentStorage, localX, y, localZ)))
                {
                    continue;
                }
                if (columnMinY[columnIndex] == kNoRenderableBlock)
                {
                    columnMinY[columnIndex] = y;
                }
                columnMaxY[columnIndex] = y;
            }
        }
    }

    for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
        {
            const std::size_t columnIndex = static_cast<std::size_t>(localZ * vibecraft::world::Chunk::kSize + localX);
            if (columnMinY[columnIndex] == kNoRenderableBlock)
            {
                continue;
            }

            const int worldX = coord.x * vibecraft::world::Chunk::kSize + localX;
            const int worldZ = coord.z * vibecraft::world::Chunk::kSize + localZ;
            for (int y = columnMinY[columnIndex]; y <= columnMaxY[columnIndex]; ++y)
            {
                const BlockType blockType = blockFromStorage(currentStorage, localX, y, localZ);
                if (!vibecraft::world::isRenderable(blockType))
                {
                    continue;
                }

                if (usesCrossPlantMesh(blockType))
                {
                    // Flora meshes are centered crossed quads instead of a full cube.
                    // Larger inset narrows the quads (smaller apparent stem diameter); bamboo uses a
                    // thinner stalk than flowers/mushrooms.
                    const float inset =
                        blockType == BlockType::Bamboo ? 0.33f : 0.146f;
                    const std::array<std::array<std::array<float, 3>, 4>, 2> plantCrossQuads{{
                        {{{inset, 0.0f, inset}, {inset, 1.0f, inset}, {1.0f - inset, 1.0f, 1.0f - inset}, {1.0f - inset, 0.0f, 1.0f - inset}}},
                        {{{1.0f - inset, 0.0f, inset}, {1.0f - inset, 1.0f, inset}, {inset, 1.0f, 1.0f - inset}, {inset, 0.0f, 1.0f - inset}}},
                    }};
                    constexpr std::array<std::array<float, 2>, 4> kPlantUv{{
                        {0.0f, 1.0f},
                        {0.0f, 0.0f},
                        {1.0f, 0.0f},
                        {1.0f, 1.0f},
                    }};

                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const std::uint8_t tileIndex =
                        vibecraft::world::textureTileIndex(blockType, vibecraft::world::BlockFace::Side);
                    for (const auto& quad : plantCrossQuads)
                    {
                        const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
                        for (std::size_t vertexIndex = 0; vertexIndex < quad.size(); ++vertexIndex)
                        {
                            const auto& corner = quad[vertexIndex];
                            const auto atlasUv = atlasUvForBlockType(tileIndex, kPlantUv[vertexIndex]);
                            meshData.vertices.push_back(DebugVertex{
                                .x = static_cast<float>(worldX) + corner[0],
                                .y = static_cast<float>(y) + corner[1],
                                .z = static_cast<float>(worldZ) + corner[2],
                                // Keep plant lighting stable from all directions.
                                .nx = 0.0f,
                                .ny = 1.0f,
                                .nz = 0.0f,
                                .u = atlasUv[0],
                                .v = atlasUv[1],
                                .abgr = metadata.debugColor,
                            });
                        }

                        meshData.indices.push_back(baseIndex);
                        meshData.indices.push_back(baseIndex + 1);
                        meshData.indices.push_back(baseIndex + 2);
                        meshData.indices.push_back(baseIndex);
                        meshData.indices.push_back(baseIndex + 2);
                        meshData.indices.push_back(baseIndex + 3);
                        ++meshData.faceCount;
                    }
                    continue;
                }

                for (const FaceDefinition& face : kFaces)
                {
                    const BlockType neighborBlock = sampledNeighborBlock(
                        currentStorage,
                        westChunk,
                        eastChunk,
                        northChunk,
                        southChunk,
                        localX + face.offsetX,
                        y + face.offsetY,
                        localZ + face.offsetZ);
                    if (neighborBlock == blockType
                        || vibecraft::world::occludesFaces(neighborBlock))
                    {
                        continue;
                    }

                    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(blockType, face.blockFace);
                    for (std::size_t vertexIndex = 0; vertexIndex < face.corners.size(); ++vertexIndex)
                    {
                        const auto& corner = face.corners[vertexIndex];
                        const auto atlasUv = atlasUvForBlockType(tileIndex, face.uvs[vertexIndex]);
                        meshData.vertices.push_back(DebugVertex{
                            .x = static_cast<float>(worldX) + corner[0],
                            .y = static_cast<float>(y) + corner[1],
                            .z = static_cast<float>(worldZ) + corner[2],
                            .nx = static_cast<float>(face.offsetX),
                            .ny = static_cast<float>(face.offsetY),
                            .nz = static_cast<float>(face.offsetZ),
                            .u = atlasUv[0],
                            .v = atlasUv[1],
                            .abgr = metadata.debugColor,
                        });
                    }

                    meshData.indices.push_back(baseIndex);
                    meshData.indices.push_back(baseIndex + 1);
                    meshData.indices.push_back(baseIndex + 2);
                    meshData.indices.push_back(baseIndex);
                    meshData.indices.push_back(baseIndex + 2);
                    meshData.indices.push_back(baseIndex + 3);
                    ++meshData.faceCount;
                }
            }
        }
    }

    return meshData;
}
}  // namespace vibecraft::meshing
