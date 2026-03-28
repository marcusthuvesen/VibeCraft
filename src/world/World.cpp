#include "vibecraft/world/World.hpp"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"
#include "vibecraft/world/WorldSerializer.hpp"

namespace vibecraft::world
{
namespace
{
constexpr int kTreeCellSize = 6;
constexpr int kTreeCrownRadius = 2;
constexpr int kTreeMinTrunkHeight = 4;
constexpr int kTreeMaxTrunkHeight = 6;
constexpr int kTreeMinSurfaceY = 27;
constexpr std::uint32_t kTreeSeed = 0x0ea52f4dU;
constexpr std::uint32_t kTreeChanceSeed = 0x6f4a31deU;
constexpr std::uint32_t kTreeOffsetXSeed = 0x34b6f0e1U;
constexpr std::uint32_t kTreeOffsetZSeed = 0x5cd2a907U;
constexpr std::uint32_t kTreeShapeSeed = 0x72f1a4b3U;

[[nodiscard]] int floorDiv(const int value, const int divisor)
{
    return value >= 0 ? value / divisor : (value - (divisor - 1)) / divisor;
}

[[nodiscard]] bool canGrowTreeAt(
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY,
    const int trunkHeight)
{
    if (surfaceY < kTreeMinSurfaceY)
    {
        return false;
    }
    if (terrainGenerator.blockTypeAt(worldX, surfaceY, worldZ) != BlockType::Grass)
    {
        return false;
    }

    const int canopyTopY = surfaceY + trunkHeight + 2;
    if (canopyTopY > kWorldMaxY)
    {
        return false;
    }

    for (int y = surfaceY + 1; y <= canopyTopY; ++y)
    {
        if (terrainGenerator.blockTypeAt(worldX, y, worldZ) != BlockType::Air)
        {
            return false;
        }
    }

    return true;
}

void placeBlockIfInsideChunk(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType blockType)
{
    if (y < kWorldMinY || y > kWorldMaxY)
    {
        return;
    }

    const int minWorldX = coord.x * Chunk::kSize;
    const int minWorldZ = coord.z * Chunk::kSize;
    const int maxWorldX = minWorldX + Chunk::kSize - 1;
    const int maxWorldZ = minWorldZ + Chunk::kSize - 1;
    if (worldX < minWorldX || worldX > maxWorldX || worldZ < minWorldZ || worldZ > maxWorldZ)
    {
        return;
    }

    const int localX = worldToLocalCoord(worldX);
    const int localZ = worldToLocalCoord(worldZ);
    if (chunk.blockAt(localX, y, localZ) != BlockType::Air)
    {
        return;
    }

    chunk.setBlock(localX, y, localZ, blockType);
}

void placeTreeForColumn(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int treeX,
    const int treeZ)
{
    const int surfaceY = terrainGenerator.surfaceHeightAt(treeX, treeZ);
    const std::uint32_t treeHash = noise::hashCoordinates(treeX, treeZ, kTreeSeed);
    const int trunkHeight = kTreeMinTrunkHeight
        + static_cast<int>(treeHash % static_cast<std::uint32_t>(kTreeMaxTrunkHeight - kTreeMinTrunkHeight + 1));
    if (!canGrowTreeAt(terrainGenerator, treeX, treeZ, surfaceY, trunkHeight))
    {
        return;
    }

    for (int y = surfaceY + 1; y <= surfaceY + trunkHeight; ++y)
    {
        placeBlockIfInsideChunk(chunk, coord, treeX, y, treeZ, BlockType::TreeTrunk);
    }

    const int crownCenterY = surfaceY + trunkHeight;
    for (int dy = -2; dy <= 1; ++dy)
    {
        const int radius = dy <= -1 ? 2 : 1;
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (dx == 0 && dz == 0)
                {
                    continue;
                }

                const int crownX = treeX + dx;
                const int crownY = crownCenterY + dy;
                const int crownZ = treeZ + dz;
                const bool outerCorner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (outerCorner
                    && noise::random01(crownX, crownZ, kTreeShapeSeed + static_cast<std::uint32_t>(crownY)) > 0.4)
                {
                    continue;
                }
                placeBlockIfInsideChunk(chunk, coord, crownX, crownY, crownZ, BlockType::TreeCrown);
            }
        }
    }

    placeBlockIfInsideChunk(chunk, coord, treeX, crownCenterY + 2, treeZ, BlockType::TreeCrown);
}

void populateTreesForChunk(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator)
{
    const int chunkWorldMinX = coord.x * Chunk::kSize;
    const int chunkWorldMinZ = coord.z * Chunk::kSize;
    const int sampleMinX = chunkWorldMinX - kTreeCrownRadius;
    const int sampleMinZ = chunkWorldMinZ - kTreeCrownRadius;
    const int sampleMaxX = chunkWorldMinX + Chunk::kSize - 1 + kTreeCrownRadius;
    const int sampleMaxZ = chunkWorldMinZ + Chunk::kSize - 1 + kTreeCrownRadius;

    const int minCellX = floorDiv(sampleMinX, kTreeCellSize);
    const int maxCellX = floorDiv(sampleMaxX, kTreeCellSize);
    const int minCellZ = floorDiv(sampleMinZ, kTreeCellSize);
    const int maxCellZ = floorDiv(sampleMaxZ, kTreeCellSize);

    for (int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ)
    {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX)
        {
            if (noise::random01(cellX, cellZ, kTreeChanceSeed) > 0.28)
            {
                continue;
            }

            const int treeX = cellX * kTreeCellSize
                + static_cast<int>(noise::hashCoordinates(cellX, cellZ, kTreeOffsetXSeed)
                                    % static_cast<std::uint32_t>(kTreeCellSize));
            const int treeZ = cellZ * kTreeCellSize
                + static_cast<int>(noise::hashCoordinates(cellX, cellZ, kTreeOffsetZSeed)
                                    % static_cast<std::uint32_t>(kTreeCellSize));
            placeTreeForColumn(chunk, coord, terrainGenerator, treeX, treeZ);
        }
    }
}

void populateChunkFromTerrain(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator)
{
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = coord.x * Chunk::kSize + localX;
            const int worldZ = coord.z * Chunk::kSize + localZ;

            for (int y = kWorldMinY; y <= kWorldMaxY; ++y)
            {
                chunk.setBlock(localX, y, localZ, terrainGenerator.blockTypeAt(worldX, y, worldZ));
            }
        }
    }

    populateTreesForChunk(chunk, coord, terrainGenerator);
}
}  // namespace

void World::generateRadius(const TerrainGenerator& terrainGenerator, const int chunkRadius)
{
    generateMissingChunksAround(terrainGenerator, ChunkCoord{0, 0}, chunkRadius);
}

void World::generateMissingChunksAround(
    const TerrainGenerator& terrainGenerator,
    const ChunkCoord& center,
    const int chunkRadius,
    const std::size_t maxChunksToGenerate)
{
    std::vector<ChunkCoord> pendingCoords;
    pendingCoords.reserve(static_cast<std::size_t>((chunkRadius * 2 + 1) * (chunkRadius * 2 + 1)));

    for (int chunkZ = center.z - chunkRadius; chunkZ <= center.z + chunkRadius; ++chunkZ)
    {
        for (int chunkX = center.x - chunkRadius; chunkX <= center.x + chunkRadius; ++chunkX)
        {
            const ChunkCoord coord{chunkX, chunkZ};
            if (chunks_.contains(coord))
            {
                continue;
            }
            pendingCoords.push_back(coord);
        }
    }

    std::sort(
        pendingCoords.begin(),
        pendingCoords.end(),
        [&center](const ChunkCoord& lhs, const ChunkCoord& rhs)
        {
            const int lhsDx = lhs.x - center.x;
            const int lhsDz = lhs.z - center.z;
            const int rhsDx = rhs.x - center.x;
            const int rhsDz = rhs.z - center.z;
            const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
            const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
            if (lhsDistanceSq != rhsDistanceSq)
            {
                return lhsDistanceSq < rhsDistanceSq;
            }
            if (lhs.z != rhs.z)
            {
                return lhs.z < rhs.z;
            }
            return lhs.x < rhs.x;
        });

    std::size_t generatedCount = 0;
    for (const ChunkCoord& coord : pendingCoords)
    {
        if (generatedCount >= maxChunksToGenerate)
        {
            break;
        }
        if (chunks_.contains(coord))
        {
            continue;
        }

        Chunk& chunk = ensureChunk(coord);
        populateChunkFromTerrain(chunk, coord, terrainGenerator);
        markChunkDirty(coord);
        ++generatedCount;
    }
}

bool World::applyEditCommand(const WorldEditCommand& command)
{
    if (command.position.y < kWorldMinY || command.position.y > kWorldMaxY)
    {
        return false;
    }

    const ChunkCoord coord = worldToChunkCoord(command.position.x, command.position.z);
    Chunk& chunk = ensureChunk(coord);
    const int localX = worldToLocalCoord(command.position.x);
    const int localZ = worldToLocalCoord(command.position.z);
    const BlockType existingType = chunk.blockAt(localX, command.position.y, localZ);

    if (existingType != BlockType::Air && !blockMetadata(existingType).breakable)
    {
        return false;
    }

    const BlockType targetType =
        command.action == WorldEditAction::Place ? command.blockType : BlockType::Air;

    if (!chunk.setBlock(localX, command.position.y, localZ, targetType))
    {
        return false;
    }

    for (const ChunkCoord& dirtyCoord : neighboringChunkCoords(coord))
    {
        markChunkDirty(dirtyCoord);
    }

    return true;
}

bool World::save(const std::filesystem::path& outputPath) const
{
    return WorldSerializer::save(*this, outputPath);
}

bool World::load(const std::filesystem::path& inputPath)
{
    return WorldSerializer::load(*this, inputPath);
}

BlockType World::blockAt(const int worldX, const int y, const int worldZ) const
{
    // Below the supported world depth, behave like an unbroken bedrock floor so physics
    // never drops into a void.
    if (y < kWorldMinY)
    {
        return BlockType::Bedrock;
    }
    if (y > kWorldMaxY)
    {
        return BlockType::Air;
    }

    const ChunkCoord coord = worldToChunkCoord(worldX, worldZ);
    const auto chunkIt = chunks_.find(coord);
    if (chunkIt == chunks_.end())
    {
        return BlockType::Air;
    }

    return chunkIt->second.blockAt(worldToLocalCoord(worldX), y, worldToLocalCoord(worldZ));
}

std::optional<RaycastHit> World::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const float maxDistance,
    const float stepSize) const
{
    const glm::vec3 normalizedDirection = glm::normalize(direction);
    glm::ivec3 previousCell(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z)));

    for (float distance = 0.0f; distance <= maxDistance; distance += stepSize)
    {
        const glm::vec3 samplePoint = origin + normalizedDirection * distance;
        const glm::ivec3 cell(
            static_cast<int>(std::floor(samplePoint.x)),
            static_cast<int>(std::floor(samplePoint.y)),
            static_cast<int>(std::floor(samplePoint.z)));

        const BlockType blockType = blockAt(cell.x, cell.y, cell.z);
        if (isSolid(blockType))
        {
            return RaycastHit{
                .solidBlock = cell,
                .buildTarget = previousCell,
                .blockType = blockType,
            };
        }

        previousCell = cell;
    }

    return std::nullopt;
}

const World::ChunkMap& World::chunks() const
{
    return chunks_;
}

const std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash>& World::meshStats() const
{
    return meshStats_;
}

std::size_t World::dirtyChunkCount() const
{
    return dirtyChunks_.size();
}

std::vector<ChunkCoord> World::dirtyChunkCoords() const
{
    return std::vector<ChunkCoord>(dirtyChunks_.begin(), dirtyChunks_.end());
}

std::uint32_t World::totalVisibleFaces() const
{
    std::uint32_t faceCount = 0;
    for (const auto& [coord, stats] : meshStats_)
    {
        static_cast<void>(coord);
        faceCount += stats.faceCount;
    }
    return faceCount;
}

void World::rebuildDirtyMeshes(const vibecraft::meshing::ChunkMesher& chunkMesher)
{
    const std::vector<ChunkCoord> dirtyCoords(dirtyChunks_.begin(), dirtyChunks_.end());
    rebuildDirtyMeshes(chunkMesher, dirtyCoords);
}

void World::rebuildDirtyMeshes(
    const vibecraft::meshing::ChunkMesher& chunkMesher,
    const std::span<const ChunkCoord> chunkCoords)
{
    for (const ChunkCoord& coord : chunkCoords)
    {
        if (!dirtyChunks_.contains(coord))
        {
            continue;
        }

        if (chunks_.find(coord) == chunks_.end())
        {
            dirtyChunks_.erase(coord);
            continue;
        }

        const vibecraft::meshing::ChunkMeshData meshData = chunkMesher.buildMesh(*this, coord);
        meshStats_[coord] = ChunkMeshStats{
            .faceCount = meshData.faceCount,
            .vertexCount = static_cast<std::uint32_t>(meshData.vertices.size()),
            .indexCount = static_cast<std::uint32_t>(meshData.indices.size()),
        };
        dirtyChunks_.erase(coord);
    }
}

void World::applyMeshStatsAndClearDirty(const std::span<const ChunkMeshUpdate> updates)
{
    for (const ChunkMeshUpdate& update : updates)
    {
        if (chunks_.find(update.coord) == chunks_.end())
        {
            dirtyChunks_.erase(update.coord);
            meshStats_.erase(update.coord);
            continue;
        }

        meshStats_[update.coord] = update.stats;
        dirtyChunks_.erase(update.coord);
    }
}

void World::replaceChunks(ChunkMap chunks)
{
    chunks_ = std::move(chunks);
    dirtyChunks_.clear();
    meshStats_.clear();

    for (const auto& [coord, chunk] : chunks_)
    {
        static_cast<void>(chunk);
        markChunkDirty(coord);
    }
}

Chunk& World::ensureChunk(const ChunkCoord& coord)
{
    auto [chunkIt, inserted] = chunks_.try_emplace(coord, coord);
    static_cast<void>(inserted);
    return chunkIt->second;
}

void World::markChunkDirty(const ChunkCoord& coord)
{
    dirtyChunks_.insert(coord);
}
}  // namespace vibecraft::world
