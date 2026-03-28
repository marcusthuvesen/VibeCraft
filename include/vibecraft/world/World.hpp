#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
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

struct ChunkMeshUpdate
{
    ChunkCoord coord{};
    ChunkMeshStats stats{};
};

class World
{
  public:
    using ChunkMap = std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash>;

    void generateRadius(const TerrainGenerator& terrainGenerator, int chunkRadius);
    void generateMissingChunksAround(
        const TerrainGenerator& terrainGenerator,
        const ChunkCoord& center,
        int chunkRadius,
        std::size_t maxChunksToGenerate = static_cast<std::size_t>(-1));
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
    [[nodiscard]] std::vector<ChunkCoord> dirtyChunkCoords() const;
    [[nodiscard]] std::uint32_t totalVisibleFaces() const;

    void rebuildDirtyMeshes(const vibecraft::meshing::ChunkMesher& chunkMesher);
    void rebuildDirtyMeshes(
        const vibecraft::meshing::ChunkMesher& chunkMesher,
        std::span<const ChunkCoord> chunkCoords);
    void applyMeshStatsAndClearDirty(std::span<const ChunkMeshUpdate> updates);
    void replaceChunks(ChunkMap chunks);

  private:
    Chunk& ensureChunk(const ChunkCoord& coord);
    void markChunkDirty(const ChunkCoord& coord);

    ChunkMap chunks_;
    std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash> meshStats_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks_;
};
}  // namespace vibecraft::world
