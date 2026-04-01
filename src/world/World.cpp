#include "vibecraft/world/World.hpp"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <limits>
#include <utility>
#include <vector>

#include "WorldGeneration.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"
#include "vibecraft/world/WorldSerializer.hpp"

namespace vibecraft::world
{
std::uint32_t World::generationSeed() const
{
    return generationSeed_;
}

void World::setGenerationSeed(const std::uint32_t generationSeed)
{
    generationSeed_ = generationSeed;
}

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
    static_cast<void>(stepSize);
    if (maxDistance <= 0.0f || glm::dot(direction, direction) == 0.0f)
    {
        return std::nullopt;
    }

    const glm::vec3 normalizedDirection = glm::normalize(direction);
    glm::ivec3 cell(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z)));
    glm::ivec3 previousCell = cell;

    const BlockType startingBlock = blockAt(cell.x, cell.y, cell.z);
    if (isRaycastTarget(startingBlock))
    {
        return RaycastHit{
            .solidBlock = cell,
            .buildTarget = previousCell,
            .blockType = startingBlock,
        };
    }

    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const glm::ivec3 step(
        normalizedDirection.x > 0.0f ? 1 : (normalizedDirection.x < 0.0f ? -1 : 0),
        normalizedDirection.y > 0.0f ? 1 : (normalizedDirection.y < 0.0f ? -1 : 0),
        normalizedDirection.z > 0.0f ? 1 : (normalizedDirection.z < 0.0f ? -1 : 0));
    const auto firstBoundaryDistance =
        [](const float originCoord, const float directionCoord, const int cellCoord)
    {
        if (directionCoord > 0.0f)
        {
            return (static_cast<float>(cellCoord + 1) - originCoord) / directionCoord;
        }
        if (directionCoord < 0.0f)
        {
            return (originCoord - static_cast<float>(cellCoord)) / -directionCoord;
        }
        return kInfinity;
    };
    const auto deltaDistance = [](const float directionCoord)
    {
        return directionCoord != 0.0f ? 1.0f / std::abs(directionCoord) : kInfinity;
    };

    float nextX = firstBoundaryDistance(origin.x, normalizedDirection.x, cell.x);
    float nextY = firstBoundaryDistance(origin.y, normalizedDirection.y, cell.y);
    float nextZ = firstBoundaryDistance(origin.z, normalizedDirection.z, cell.z);
    const float deltaX = deltaDistance(normalizedDirection.x);
    const float deltaY = deltaDistance(normalizedDirection.y);
    const float deltaZ = deltaDistance(normalizedDirection.z);

    while (true)
    {
        const float travelDistance = std::min(nextX, std::min(nextY, nextZ));
        if (travelDistance > maxDistance)
        {
            break;
        }

        previousCell = cell;
        if (nextX <= nextY && nextX <= nextZ)
        {
            cell.x += step.x;
            nextX += deltaX;
        }
        else if (nextY <= nextZ)
        {
            cell.y += step.y;
            nextY += deltaY;
        }
        else
        {
            cell.z += step.z;
            nextZ += deltaZ;
        }

        const BlockType blockType = blockAt(cell.x, cell.y, cell.z);
        if (isRaycastTarget(blockType))
        {
            return RaycastHit{
                .solidBlock = cell,
                .buildTarget = previousCell,
                .blockType = blockType,
            };
        }
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

void World::replaceChunk(Chunk chunk)
{
    const ChunkCoord coord = chunk.coord();
    const auto existingIt = chunks_.find(coord);
    if (existingIt != chunks_.end() && existingIt->second.blockStorage() == chunk.blockStorage())
    {
        return;
    }

    chunks_[coord] = std::move(chunk);

    // A streamed chunk update can change faces on the chunk itself and along all four borders.
    for (const ChunkCoord& dirtyCoord : neighboringChunkCoords(coord))
    {
        markChunkDirty(dirtyCoord);
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
