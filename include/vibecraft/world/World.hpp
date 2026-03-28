#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "vibecraft/world/Chunk.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::meshing
{
struct ChunkMeshData;
class ChunkMesher;
}  // namespace vibecraft::meshing

namespace vibecraft::world
{
class TerrainGenerator;

struct RaycastHit
{
    glm::ivec3 solidBlock{0, 0, 0};
    glm::ivec3 buildTarget{0, 0, 0};
    BlockType blockType = BlockType::Air;
};

struct ChunkMeshStats
{
    std::uint32_t faceCount = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t indexCount = 0;
};

class World
{
  public:
    using ChunkMap = std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>;

    void generateRadius(const TerrainGenerator& terrainGenerator, int chunkRadius);
    bool applyEditCommand(const WorldEditCommand& command);
    bool save(const std::filesystem::path& outputPath) const;
    bool load(const std::filesystem::path& inputPath);

    [[nodiscard]] BlockType blockAt(int worldX, int y, int worldZ) const;
    [[nodiscard]] std::optional<RaycastHit> raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float maxDistance,
        float stepSize = 0.1f) const;

    [[nodiscard]] const ChunkMap& chunks() const;
    [[nodiscard]] const std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash>& meshStats() const;
    [[nodiscard]] std::size_t dirtyChunkCount() const;
    [[nodiscard]] std::uint32_t totalVisibleFaces() const;

    void rebuildDirtyMeshes(const vibecraft::meshing::ChunkMesher& chunkMesher);
    void replaceChunks(ChunkMap chunks);

  private:
    Chunk& ensureChunk(const ChunkCoord& coord);
    void markChunkDirty(const ChunkCoord& coord);

    ChunkMap chunks_;
    std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash> meshStats_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks_;
};
}  // namespace vibecraft::world
