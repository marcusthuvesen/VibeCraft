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

inline constexpr int kChunkRenderSectionHeight = 16;
static_assert(
    vibecraft::world::Chunk::kHeight % kChunkRenderSectionHeight == 0,
    "Chunk render section height must evenly divide chunk height.");
inline constexpr int kChunkRenderSectionCount =
    vibecraft::world::Chunk::kHeight / kChunkRenderSectionHeight;

[[nodiscard]] constexpr int chunkRenderSectionIndexForY(const int y)
{
    return (y - vibecraft::world::kWorldMinY) / kChunkRenderSectionHeight;
}

[[nodiscard]] constexpr int chunkRenderSectionMinY(const int sectionIndex)
{
    return vibecraft::world::kWorldMinY + sectionIndex * kChunkRenderSectionHeight;
}

[[nodiscard]] constexpr int chunkRenderSectionMaxY(const int sectionIndex)
{
    return chunkRenderSectionMinY(sectionIndex) + kChunkRenderSectionHeight - 1;
}

struct ChunkSectionMeshData
{
    int sectionIndex = 0;
    ChunkMeshData mesh;
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
    [[nodiscard]] std::vector<ChunkSectionMeshData> buildSectionMeshes(
        const vibecraft::world::World& world,
        const vibecraft::world::ChunkCoord& coord,
        const ChunkMeshBuildSettings& settings = {}) const;

    [[nodiscard]] ChunkMeshData buildMesh(
        const vibecraft::world::World& world,
        const vibecraft::world::ChunkCoord& coord,
        const ChunkMeshBuildSettings& settings = {}) const;
};
}  // namespace vibecraft::meshing
