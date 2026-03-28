#include "vibecraft/meshing/ChunkMesher.hpp"

#include <array>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::meshing
{
namespace
{
using vibecraft::world::BlockType;

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
}  // namespace

ChunkMeshData ChunkMesher::buildMesh(
    const vibecraft::world::World& world,
    const vibecraft::world::ChunkCoord& coord) const
{
    ChunkMeshData meshData;

    for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
        {
            for (int y = 0; y < vibecraft::world::Chunk::kHeight; ++y)
            {
                const int worldX = coord.x * vibecraft::world::Chunk::kSize + localX;
                const int worldZ = coord.z * vibecraft::world::Chunk::kSize + localZ;
                const BlockType blockType = world.blockAt(worldX, y, worldZ);
                if (!vibecraft::world::isRenderable(blockType))
                {
                    continue;
                }

                for (const FaceDefinition& face : kFaces)
                {
                    const BlockType neighborBlock =
                        world.blockAt(worldX + face.offsetX, y + face.offsetY, worldZ + face.offsetZ);
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
                            .u = atlasUv[0],
                            .v = atlasUv[1],
                            .abgr = metadata.debugColor,
                        });
                    }

                    meshData.indices.insert(
                        meshData.indices.end(),
                        {baseIndex, baseIndex + 1, baseIndex + 2, baseIndex, baseIndex + 2, baseIndex + 3});
                    ++meshData.faceCount;
                }
            }
        }
    }

    return meshData;
}
}  // namespace vibecraft::meshing
