#pragma once

#include <cstddef>

#include "vibecraft/world/Chunk.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world::detail
{
[[nodiscard]] constexpr std::size_t chunkStorageIndex(const int localX, const int y, const int localZ)
{
    const int localY = y - kWorldMinY;
    return static_cast<std::size_t>(localY * Chunk::kSize * Chunk::kSize + localZ * Chunk::kSize + localX);
}

void populateTreesForChunk(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator);
void populateSurfaceFloraForChunk(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator);
}  // namespace vibecraft::world::detail
