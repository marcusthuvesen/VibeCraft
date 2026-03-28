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
    std::uint32_t abgr = 0;
};

struct ChunkMeshData
{
    std::vector<DebugVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t faceCount = 0;
};

class ChunkMesher
{
  public:
    [[nodiscard]] ChunkMeshData buildMesh(
        const vibecraft::world::World& world,
        const vibecraft::world::ChunkCoord& coord) const;
};
}  // namespace vibecraft::meshing
