#pragma once

#include "vibecraft/world/Chunk.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"

namespace vibecraft::world
{
void populateChunkFromTerrain(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator);
}
