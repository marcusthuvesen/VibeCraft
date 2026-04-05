#pragma once

#include <glm/vec3.hpp>

#include <deque>
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

    [[nodiscard]] std::uint32_t generationSeed() const;
    void setGenerationSeed(std::uint32_t generationSeed);
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
    [[nodiscard]] std::uint64_t dirtyRevisionForChunk(const ChunkCoord& coord) const;
    [[nodiscard]] std::uint32_t totalVisibleFaces() const;
    void tickFluids(std::size_t maxUpdates = 96);
    void tickLeafDecay(std::size_t maxUpdates = 8);

    void rebuildDirtyMeshes(const vibecraft::meshing::ChunkMesher& chunkMesher);
    void rebuildDirtyMeshes(
        const vibecraft::meshing::ChunkMesher& chunkMesher,
        std::span<const ChunkCoord> chunkCoords);
    void applyMeshStatsAndClearDirty(std::span<const ChunkMeshUpdate> updates);
    void replaceChunk(Chunk chunk);
    void replaceChunks(ChunkMap chunks);

  private:
    struct FluidCell
    {
        int x = 0;
        int y = 0;
        int z = 0;

        auto operator<=>(const FluidCell&) const = default;
    };

    struct FluidCellHash
    {
        std::size_t operator()(const FluidCell& cell) const noexcept;
    };

    struct FlowingFluidState
    {
        BlockType type = BlockType::Air;
        std::uint8_t horizontalDistance = 0;
    };

    struct OrganicDecayCell
    {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const OrganicDecayCell&) const = default;
    };

    struct OrganicDecayCellHash
    {
        std::size_t operator()(const OrganicDecayCell& cell) const noexcept;
    };

    Chunk& ensureChunk(const ChunkCoord& coord);
    void markChunkDirty(const ChunkCoord& coord);
    bool setBlockUnchecked(int worldX, int y, int worldZ, BlockType blockType);
    void scheduleFluidNeighborhood(int worldX, int y, int worldZ);
    void enqueueLeafDecayCell(int worldX, int y, int worldZ);
    void scheduleLeafDecayNeighborhood(int worldX, int y, int worldZ, BlockType removedType);
    [[nodiscard]] bool leafHasRootSupport(int worldX, int y, int worldZ, BlockType leafType) const;
    void processLeafDecayCell(const OrganicDecayCell& cell);
    void clearFluidStateForChunk(const ChunkCoord& coord);
    void registerFluidStateForChunk(const Chunk& chunk);
    void processFluidCell(const FluidCell& cell);

    std::uint32_t generationSeed_ = 0;
    ChunkMap chunks_;
    std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash> meshStats_;
    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyChunks_;
    std::unordered_map<ChunkCoord, std::uint64_t, ChunkCoordHash> dirtyRevisionByChunk_;
    std::unordered_map<FluidCell, BlockType, FluidCellHash> fluidSources_;
    std::unordered_map<FluidCell, FlowingFluidState, FluidCellHash> flowingFluids_;
    std::unordered_set<FluidCell, FluidCellHash> activeFluidCells_;
    std::uint32_t fluidTickCounter_ = 0;
    std::deque<OrganicDecayCell> activeLeafDecayCells_;
    std::unordered_set<OrganicDecayCell, OrganicDecayCellHash> queuedLeafDecayCells_;
};
}  // namespace vibecraft::world
