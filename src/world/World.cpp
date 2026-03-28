#include "vibecraft/world/World.hpp"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <utility>
#include <vector>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/WorldSerializer.hpp"

namespace vibecraft::world
{
namespace
{
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

            for (int y = 0; y < Chunk::kHeight; ++y)
            {
                chunk.setBlock(localX, y, localZ, terrainGenerator.blockTypeAt(worldX, y, worldZ));
            }
        }
    }
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
    if (command.position.y < 0 || command.position.y >= Chunk::kHeight)
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
    // Below the chunk column, behave like an unbroken bedrock floor so physics never hits "void".
    if (y < 0)
    {
        return BlockType::Bedrock;
    }
    if (y >= Chunk::kHeight)
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
