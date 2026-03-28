#include "vibecraft/meshing/ChunkMesher.hpp"

#include <array>

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
    std::array<std::array<float, 3>, 4> corners;
};

constexpr std::array<FaceDefinition, 6> kFaces{{
    {1, 0, 0, {{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}}},
    {-1, 0, 0, {{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}}},
    {0, 1, 0, {{{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}},
    {0, -1, 0, {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}}},
    {0, 0, 1, {{{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}}},
    {0, 0, -1, {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}}},
}};

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
                if (!vibecraft::world::isSolid(blockType))
                {
                    continue;
                }

                for (const FaceDefinition& face : kFaces)
                {
                    if (vibecraft::world::isSolid(
                            world.blockAt(worldX + face.offsetX, y + face.offsetY, worldZ + face.offsetZ)))
                    {
                        continue;
                    }

                    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
                    for (const auto& corner : face.corners)
                    {
                        meshData.vertices.push_back(DebugVertex{
                            .x = static_cast<float>(worldX) + corner[0],
                            .y = static_cast<float>(y) + corner[1],
                            .z = static_cast<float>(worldZ) + corner[2],
                            .abgr = vibecraft::world::blockMetadata(blockType).debugColor,
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
