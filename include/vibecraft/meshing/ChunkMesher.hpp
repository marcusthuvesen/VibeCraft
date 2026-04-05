#pragma once

#include <cstdint>
#include <vector>

#include "vibecraft/world/Chunk.hpp"

namespace vibecraft::world
{
class World;
}

namespace vibecraft::meshing
{
struct DebugVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    std::uint32_t abgr = 0;
};

struct ChunkMeshData
{
    std::vector<DebugVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t faceCount = 0;
};

struct ChunkMeshBuildSettings
{
    bool prioritizeVerticalWindow = false;
    int focusCenterY = 64;
    int renderAboveBlocks = 80;
    int renderBelowBlocks = 48;
};

class ChunkMesher
{
  public:
    [[nodiscard]] ChunkMeshData buildMesh(
        const vibecraft::world::World& world,
        const vibecraft::world::ChunkCoord& coord,
        const ChunkMeshBuildSettings& settings = {}) const;
};
}  // namespace vibecraft::meshing
