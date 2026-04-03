#include "WorldGeneration.hpp"

#include <array>

#include "WorldGenerationDetail.hpp"

namespace vibecraft::world
{
namespace
{
void populateTerrainColumns(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator)
{
    std::array<BlockType, kWorldHeight> columnBlocks{};
    auto& storage = chunk.mutableBlockStorage();
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = coord.x * Chunk::kSize + localX;
            const int worldZ = coord.z * Chunk::kSize + localZ;
            terrainGenerator.fillColumn(worldX, worldZ, columnBlocks.data());
            for (int y = kWorldMinY; y <= kWorldMaxY; ++y)
            {
                BlockType blockType = columnBlocks[y - kWorldMinY];
                if (!isNaturalTerrainBlock(blockType))
                {
                    blockType = BlockType::Air;
                }
                storage[detail::chunkStorageIndex(localX, y, localZ)] = blockType;
            }
        }
    }
}
}  // namespace

void populateChunkFromTerrain(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator)
{
    populateTerrainColumns(chunk, coord, terrainGenerator);
    detail::populateCaveDecorForChunk(chunk, coord, terrainGenerator);
    detail::populateTreesForChunk(chunk, coord, terrainGenerator);
    detail::populateSurfaceFloraForChunk(chunk, coord, terrainGenerator);
}
}  // namespace vibecraft::world
