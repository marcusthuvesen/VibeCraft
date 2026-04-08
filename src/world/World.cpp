#include "vibecraft/world/World.hpp"

#include <algorithm>
#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <array>
#include <limits>
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
namespace
{
constexpr int kWaterHorizontalReach = 7;
constexpr int kLavaHorizontalReach = 3;
constexpr std::uint32_t kLavaHorizontalCadenceTicks = 3;
constexpr int kLeafDecayQueueRadius = 6;
constexpr int kLeafDecayQueueBelow = 2;
constexpr int kLeafDecayQueueAbove = 8;
constexpr int kLeafSupportSearchRadius = 7;
constexpr std::size_t kLeafSupportTraversalLimit = 256;

[[nodiscard]] bool isFluidReplaceable(const BlockType blockType)
{
    return blockType == BlockType::Air || isFluid(blockType);
}

[[nodiscard]] bool isChunkCoordInRange(const int worldX, const int worldZ, const ChunkCoord& coord)
{
    return worldToChunkCoord(worldX, worldZ) == coord;
}

[[nodiscard]] bool isGroundedTreeSupport(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && !isLeafBlock(blockType);
}
}  // namespace

std::uint32_t World::generationSeed() const
{
    return generationSeed_;
}

std::size_t World::FluidCellHash::operator()(const FluidCell& cell) const noexcept
{
    std::size_t seed = static_cast<std::size_t>(static_cast<std::uint32_t>(cell.x));
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.y + 0x9e3779b9U)) + (seed << 6U) + (seed >> 2U);
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.z + 0x85ebca6bU)) + (seed << 6U) + (seed >> 2U);
    return seed;
}

std::size_t World::OrganicDecayCellHash::operator()(const OrganicDecayCell& cell) const noexcept
{
    std::size_t seed = static_cast<std::size_t>(static_cast<std::uint32_t>(cell.x));
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.y + 0x9e3779b9U)) + (seed << 6U) + (seed >> 2U);
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.z + 0x85ebca6bU)) + (seed << 6U) + (seed >> 2U);
    return seed;
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
        registerFluidStateForChunk(chunk);
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

    const auto clearRemovedBlockSideEffects = [this](const int worldX, const int y, const int worldZ)
    {
        const FluidCell cell{worldX, y, worldZ};
        fluidSources_.erase(cell);
        flowingFluids_.erase(cell);
        scheduleFluidNeighborhood(worldX, y, worldZ);
        scheduleGravityBlock(worldX, y, worldZ);
        scheduleGravityBlock(worldX, y + 1, worldZ);
    };

    if (!chunk.setBlock(localX, command.position.y, localZ, targetType))
    {
        return false;
    }

    if (command.action == WorldEditAction::Remove && isLogBlock(existingType))
    {
        scheduleLeafDecayNeighborhood(command.position.x, command.position.y, command.position.z, existingType);
    }

    clearRemovedBlockSideEffects(command.position.x, command.position.y, command.position.z);

    if (command.action == WorldEditAction::Remove && existingType == BlockType::Bamboo)
    {
        for (int bambooY = command.position.y + 1; bambooY <= kWorldMaxY; ++bambooY)
        {
            if (blockAt(command.position.x, bambooY, command.position.z) != BlockType::Bamboo)
            {
                break;
            }
            if (!setBlockUnchecked(command.position.x, bambooY, command.position.z, BlockType::Air))
            {
                break;
            }
            clearRemovedBlockSideEffects(command.position.x, bambooY, command.position.z);
        }
    }

    const FluidCell cell{command.position.x, command.position.y, command.position.z};
    if (isFluid(targetType))
    {
        fluidSources_[cell] = targetType;
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

FluidRenderState World::fluidRenderStateAt(const int worldX, const int y, const int worldZ) const
{
    const BlockType blockType = blockAt(worldX, y, worldZ);
    if (!isFluid(blockType))
    {
        return {};
    }

    const FluidCell cell{worldX, y, worldZ};
    if (const auto sourceIt = fluidSources_.find(cell);
        sourceIt != fluidSources_.end() && sourceIt->second == blockType)
    {
        return FluidRenderState{
            .type = blockType,
            .isSource = true,
            .horizontalDistance = 0,
        };
    }

    if (const auto flowIt = flowingFluids_.find(cell);
        flowIt != flowingFluids_.end() && flowIt->second.type == blockType)
    {
        return FluidRenderState{
            .type = blockType,
            .isSource = false,
            .horizontalDistance = flowIt->second.horizontalDistance,
        };
    }

    return FluidRenderState{
        .type = blockType,
        .isSource = true,
        .horizontalDistance = 0,
    };
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

std::uint64_t World::dirtyRevisionForChunk(const ChunkCoord& coord) const
{
    const auto it = dirtyRevisionByChunk_.find(coord);
    if (it == dirtyRevisionByChunk_.end())
    {
        return 0;
    }
    return it->second;
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

void World::tickFluids(const std::size_t maxUpdates)
{
    if (maxUpdates == 0 || activeFluidCells_.empty())
    {
        return;
    }
    ++fluidTickCounter_;

    std::vector<FluidCell> pending;
    pending.reserve(std::min(maxUpdates, activeFluidCells_.size()));
    auto it = activeFluidCells_.begin();
    while (it != activeFluidCells_.end() && pending.size() < maxUpdates)
    {
        pending.push_back(*it);
        it = activeFluidCells_.erase(it);
    }

    for (const FluidCell& cell : pending)
    {
        processFluidCell(cell);
    }
}

void World::tickLeafDecay(const std::size_t maxUpdates)
{
    if (maxUpdates == 0 || activeLeafDecayCells_.empty())
    {
        return;
    }

    std::size_t processed = 0;
    while (processed < maxUpdates && !activeLeafDecayCells_.empty())
    {
        const OrganicDecayCell cell = activeLeafDecayCells_.front();
        activeLeafDecayCells_.pop_front();
        queuedLeafDecayCells_.erase(cell);
        processLeafDecayCell(cell);
        ++processed;
    }
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
    const bool hadExistingChunk = existingIt != chunks_.end();
    if (existingIt != chunks_.end() && existingIt->second.blockStorage() == chunk.blockStorage())
    {
        return;
    }

    chunks_[coord] = std::move(chunk);
    if (hadExistingChunk)
    {
        clearFluidStateForChunk(coord);
    }
    registerFluidStateForChunk(chunks_.at(coord));

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
    dirtyRevisionByChunk_.clear();
    meshStats_.clear();
    fluidSources_.clear();
    flowingFluids_.clear();
    activeFluidCells_.clear();
    fluidTickCounter_ = 0;
    activeLeafDecayCells_.clear();
    queuedLeafDecayCells_.clear();
    activeGravityBlocks_.clear();

    for (const auto& [coord, chunk] : chunks_)
    {
        registerFluidStateForChunk(chunk);
        markChunkDirty(coord);
    }
}

std::size_t World::unloadChunksOutsideRadius(
    const ChunkCoord& center,
    const int keepChunkRadius,
    const std::size_t maxChunksToUnload)
{
    if (keepChunkRadius < 0 || maxChunksToUnload == 0 || chunks_.empty())
    {
        return 0;
    }

    std::vector<ChunkCoord> unloadCoords;
    unloadCoords.reserve(std::min(maxChunksToUnload, chunks_.size()));
    for (const auto& [coord, chunk] : chunks_)
    {
        static_cast<void>(chunk);
        if (std::abs(coord.x - center.x) <= keepChunkRadius
            && std::abs(coord.z - center.z) <= keepChunkRadius)
        {
            continue;
        }
        unloadCoords.push_back(coord);
        if (unloadCoords.size() >= maxChunksToUnload)
        {
            break;
        }
    }

    if (unloadCoords.empty())
    {
        return 0;
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> unloadCoordSet(unloadCoords.begin(), unloadCoords.end());
    for (const ChunkCoord& coord : unloadCoords)
    {
        chunks_.erase(coord);
        meshStats_.erase(coord);
        dirtyChunks_.erase(coord);
        dirtyRevisionByChunk_.erase(coord);
    }

    const auto isUnloadedChunkWorldPos = [&unloadCoordSet](const int worldX, const int worldZ)
    {
        return unloadCoordSet.contains(worldToChunkCoord(worldX, worldZ));
    };

    for (auto it = fluidSources_.begin(); it != fluidSources_.end();)
    {
        if (isUnloadedChunkWorldPos(it->first.x, it->first.z))
        {
            it = fluidSources_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = flowingFluids_.begin(); it != flowingFluids_.end();)
    {
        if (isUnloadedChunkWorldPos(it->first.x, it->first.z))
        {
            it = flowingFluids_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = activeFluidCells_.begin(); it != activeFluidCells_.end();)
    {
        if (isUnloadedChunkWorldPos(it->x, it->z))
        {
            it = activeFluidCells_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    std::deque<OrganicDecayCell> keptLeafCells;
    for (const OrganicDecayCell& cell : activeLeafDecayCells_)
    {
        if (!isUnloadedChunkWorldPos(cell.x, cell.z))
        {
            keptLeafCells.push_back(cell);
        }
    }
    activeLeafDecayCells_ = std::move(keptLeafCells);
    queuedLeafDecayCells_.clear();
    for (const OrganicDecayCell& cell : activeLeafDecayCells_)
    {
        queuedLeafDecayCells_.insert(cell);
    }

    for (auto it = activeGravityBlocks_.begin(); it != activeGravityBlocks_.end();)
    {
        if (isUnloadedChunkWorldPos(it->x, it->z))
        {
            it = activeGravityBlocks_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return unloadCoords.size();
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
    const auto it = dirtyRevisionByChunk_.find(coord);
    if (it == dirtyRevisionByChunk_.end())
    {
        dirtyRevisionByChunk_.emplace(coord, 1);
        return;
    }
    ++it->second;
}

bool World::setBlockUnchecked(const int worldX, const int y, const int worldZ, const BlockType blockType)
{
    if (y < kWorldMinY || y > kWorldMaxY)
    {
        return false;
    }

    Chunk& chunk = ensureChunk(worldToChunkCoord(worldX, worldZ));
    const int localX = worldToLocalCoord(worldX);
    const int localZ = worldToLocalCoord(worldZ);
    if (!chunk.setBlock(localX, y, localZ, blockType))
    {
        return false;
    }

    for (const ChunkCoord& dirtyCoord : neighboringChunkCoords(chunk.coord()))
    {
        markChunkDirty(dirtyCoord);
    }
    return true;
}

void World::enqueueLeafDecayCell(const int worldX, const int y, const int worldZ)
{
    if (y < kWorldMinY || y > kWorldMaxY)
    {
        return;
    }

    const OrganicDecayCell cell{worldX, y, worldZ};
    if (queuedLeafDecayCells_.insert(cell).second)
    {
        activeLeafDecayCells_.push_back(cell);
    }
}

void World::scheduleLeafDecayNeighborhood(
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType removedType)
{
    const BlockType leafType = leafBlockForLog(removedType);
    if (!isLeafBlock(leafType))
    {
        return;
    }

    for (int dy = -kLeafDecayQueueBelow; dy <= kLeafDecayQueueAbove; ++dy)
    {
        for (int dz = -kLeafDecayQueueRadius; dz <= kLeafDecayQueueRadius; ++dz)
        {
            for (int dx = -kLeafDecayQueueRadius; dx <= kLeafDecayQueueRadius; ++dx)
            {
                if (blockAt(worldX + dx, y + dy, worldZ + dz) == leafType)
                {
                    enqueueLeafDecayCell(worldX + dx, y + dy, worldZ + dz);
                }
            }
        }
    }
}

bool World::leafHasRootSupport(const int worldX, const int y, const int worldZ, const BlockType leafType) const
{
    const BlockType logType = logBlockForLeaf(leafType);
    if (!isLogBlock(logType))
    {
        return true;
    }

    std::deque<OrganicDecayCell> pending;
    std::unordered_set<OrganicDecayCell, OrganicDecayCellHash> visited;
    const OrganicDecayCell origin{worldX, y, worldZ};
    pending.push_back(origin);
    visited.insert(origin);

    constexpr std::array<std::array<int, 3>, 6> kNeighborOffsets{{
        {{1, 0, 0}},
        {{-1, 0, 0}},
        {{0, 1, 0}},
        {{0, -1, 0}},
        {{0, 0, 1}},
        {{0, 0, -1}},
    }};

    while (!pending.empty() && visited.size() <= kLeafSupportTraversalLimit)
    {
        const OrganicDecayCell cell = pending.front();
        pending.pop_front();
        const BlockType blockType = blockAt(cell.x, cell.y, cell.z);
        if (blockType != leafType && blockType != logType)
        {
            continue;
        }

        if (blockType == logType)
        {
            const BlockType below = blockAt(cell.x, cell.y - 1, cell.z);
            if (below != logType && isGroundedTreeSupport(below))
            {
                return true;
            }
        }

        for (const auto& offset : kNeighborOffsets)
        {
            const OrganicDecayCell neighbor{
                cell.x + offset[0],
                cell.y + offset[1],
                cell.z + offset[2],
            };
            if (std::abs(neighbor.x - worldX) > kLeafSupportSearchRadius
                || std::abs(neighbor.y - y) > kLeafSupportSearchRadius
                || std::abs(neighbor.z - worldZ) > kLeafSupportSearchRadius)
            {
                continue;
            }
            if (visited.insert(neighbor).second)
            {
                pending.push_back(neighbor);
            }
        }
    }

    return false;
}

void World::processLeafDecayCell(const OrganicDecayCell& cell)
{
    const BlockType leafType = blockAt(cell.x, cell.y, cell.z);
    if (!isLeafBlock(leafType) || leafHasRootSupport(cell.x, cell.y, cell.z, leafType))
    {
        return;
    }

    if (!setBlockUnchecked(cell.x, cell.y, cell.z, BlockType::Air))
    {
        return;
    }

    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0 && dz == 0)
                {
                    continue;
                }
                if (blockAt(cell.x + dx, cell.y + dy, cell.z + dz) == leafType)
                {
                    enqueueLeafDecayCell(cell.x + dx, cell.y + dy, cell.z + dz);
                }
            }
        }
    }
}

void World::scheduleFluidNeighborhood(const int worldX, const int y, const int worldZ)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        activeFluidCells_.insert(FluidCell{worldX, y + dy, worldZ});
    }

    constexpr std::array<std::array<int, 3>, 8> kOffsets{{
        {{1, 0, 0}},
        {{-1, 0, 0}},
        {{0, 0, 1}},
        {{0, 0, -1}},
        {{1, -1, 0}},
        {{-1, -1, 0}},
        {{0, -1, 1}},
        {{0, -1, -1}},
    }};
    for (const auto& offset : kOffsets)
    {
        activeFluidCells_.insert(FluidCell{worldX + offset[0], y + offset[1], worldZ + offset[2]});
        activeFluidCells_.insert(FluidCell{worldX + offset[0], y + 1, worldZ + offset[2]});
    }
}

void World::clearFluidStateForChunk(const ChunkCoord& coord)
{
    for (auto it = fluidSources_.begin(); it != fluidSources_.end();)
    {
        if (isChunkCoordInRange(it->first.x, it->first.z, coord))
        {
            it = fluidSources_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = flowingFluids_.begin(); it != flowingFluids_.end();)
    {
        if (isChunkCoordInRange(it->first.x, it->first.z, coord))
        {
            it = flowingFluids_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = activeFluidCells_.begin(); it != activeFluidCells_.end();)
    {
        if (isChunkCoordInRange(it->x, it->z, coord))
        {
            it = activeFluidCells_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void World::registerFluidStateForChunk(const Chunk& chunk)
{
    const ChunkCoord coord = chunk.coord();
    const int minWorldX = coord.x * Chunk::kSize;
    const int minWorldZ = coord.z * Chunk::kSize;
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = minWorldX + localX;
            const int worldZ = minWorldZ + localZ;
            for (int y = kWorldMinY; y <= kWorldMaxY; ++y)
            {
                const BlockType blockType = chunk.blockAt(localX, y, localZ);
                if (!isFluid(blockType))
                {
                    continue;
                }

                const FluidCell cell{worldX, y, worldZ};
                fluidSources_[cell] = blockType;
                constexpr std::array<std::array<int, 3>, 6> kOffsets{{
                    {{1, 0, 0}},
                    {{-1, 0, 0}},
                    {{0, 0, 1}},
                    {{0, 0, -1}},
                    {{0, 1, 0}},
                    {{0, -1, 0}},
                }};
                for (const auto& offset : kOffsets)
                {
                    const BlockType neighbor = blockAt(worldX + offset[0], y + offset[1], worldZ + offset[2]);
                    if (neighbor == BlockType::Air || (isFluid(neighbor) && neighbor != blockType))
                    {
                        scheduleFluidNeighborhood(worldX, y, worldZ);
                        break;
                    }
                }
            }
        }
    }
}

void World::processFluidCell(const FluidCell& cell)
{
    if (cell.y < kWorldMinY || cell.y > kWorldMaxY)
    {
        return;
    }

    const BlockType currentBlock = blockAt(cell.x, cell.y, cell.z);

    const auto sourceIt = fluidSources_.find(cell);
    const auto flowIt = flowingFluids_.find(cell);
    const bool hasWaterSource = sourceIt != fluidSources_.end() && sourceIt->second == BlockType::Water;
    const bool hasLavaSource = sourceIt != fluidSources_.end() && sourceIt->second == BlockType::Lava;

    if ((currentBlock == BlockType::Water || currentBlock == BlockType::Lava)
        && sourceIt != fluidSources_.end())
    {
        constexpr std::array<std::array<int, 3>, 6> kNeighborOffsets{{
            {{1, 0, 0}},
            {{-1, 0, 0}},
            {{0, 0, 1}},
            {{0, 0, -1}},
            {{0, 1, 0}},
            {{0, -1, 0}},
        }};
        if (currentBlock == BlockType::Lava)
        {
            for (const auto& offset : kNeighborOffsets)
            {
                if (blockAt(cell.x + offset[0], cell.y + offset[1], cell.z + offset[2]) == BlockType::Water)
                {
                    const BlockType cooledBlock = hasLavaSource ? BlockType::Obsidian : BlockType::Cobblestone;
                    if (setBlockUnchecked(cell.x, cell.y, cell.z, cooledBlock))
                    {
                        fluidSources_.erase(cell);
                        flowingFluids_.erase(cell);
                        scheduleFluidNeighborhood(cell.x, cell.y, cell.z);
                    }
                    return;
                }
            }
        }
    }

    struct DesiredFluid
    {
        BlockType type = BlockType::Air;
        int horizontalDistance = 0;
        bool valid = false;
    };

    const auto computeDesiredFluid = [&](const BlockType fluidType, const int horizontalReach)
    {
        DesiredFluid desired{};
        if (!isFluidReplaceable(currentBlock) || (isFluid(currentBlock) && currentBlock != fluidType && currentBlock != BlockType::Air))
        {
            if (currentBlock != fluidType)
            {
                return desired;
            }
        }

        if ((fluidType == BlockType::Water && hasWaterSource) || (fluidType == BlockType::Lava && hasLavaSource))
        {
            desired.type = fluidType;
            desired.horizontalDistance = 0;
            desired.valid = true;
            return desired;
        }

        if (blockAt(cell.x, cell.y + 1, cell.z) == fluidType)
        {
            desired.type = fluidType;
            desired.horizontalDistance = 0;
            desired.valid = true;
            return desired;
        }

        const BlockType below = blockAt(cell.x, cell.y - 1, cell.z);
        if (below == BlockType::Air)
        {
            return desired;
        }

        int bestDistance = std::numeric_limits<int>::max();
        constexpr std::array<std::array<int, 2>, 4> kHorizontalOffsets{{
            {{1, 0}},
            {{-1, 0}},
            {{0, 1}},
            {{0, -1}},
        }};
        for (const auto& offset : kHorizontalOffsets)
        {
            const FluidCell neighbor{cell.x + offset[0], cell.y, cell.z + offset[1]};
            const BlockType neighborBlock = blockAt(neighbor.x, neighbor.y, neighbor.z);
            if (neighborBlock != fluidType)
            {
                continue;
            }

            if (const auto neighborSourceIt = fluidSources_.find(neighbor);
                neighborSourceIt != fluidSources_.end() && neighborSourceIt->second == fluidType)
            {
                bestDistance = std::min(bestDistance, 1);
                continue;
            }

            const auto neighborFlowIt = flowingFluids_.find(neighbor);
            if (neighborFlowIt == flowingFluids_.end() || neighborFlowIt->second.type != fluidType)
            {
                continue;
            }
            bestDistance = std::min(bestDistance, static_cast<int>(neighborFlowIt->second.horizontalDistance) + 1);
        }

        if (bestDistance <= horizontalReach)
        {
            desired.type = fluidType;
            desired.horizontalDistance = bestDistance;
            desired.valid = true;
        }
        return desired;
    };

    const DesiredFluid desiredWater = computeDesiredFluid(BlockType::Water, kWaterHorizontalReach);
    const DesiredFluid desiredLava = computeDesiredFluid(BlockType::Lava, kLavaHorizontalReach);

    BlockType desiredBlock = BlockType::Air;
    int desiredDistance = 0;
    if (desiredWater.valid && desiredLava.valid)
    {
        desiredBlock = desiredWater.horizontalDistance <= desiredLava.horizontalDistance ? BlockType::Water : BlockType::Lava;
        desiredDistance =
            desiredBlock == BlockType::Water ? desiredWater.horizontalDistance : desiredLava.horizontalDistance;
    }
    else if (desiredWater.valid)
    {
        desiredBlock = BlockType::Water;
        desiredDistance = desiredWater.horizontalDistance;
    }
    else if (desiredLava.valid)
    {
        desiredBlock = BlockType::Lava;
        desiredDistance = desiredLava.horizontalDistance;
    }

    if (currentBlock == BlockType::Lava && desiredBlock == BlockType::Water)
    {
        const BlockType cooledBlock = hasLavaSource ? BlockType::Obsidian : BlockType::Cobblestone;
        if (setBlockUnchecked(cell.x, cell.y, cell.z, cooledBlock))
        {
            fluidSources_.erase(cell);
            flowingFluids_.erase(cell);
            scheduleFluidNeighborhood(cell.x, cell.y, cell.z);
        }
        return;
    }

    if (desiredBlock == BlockType::Air)
    {
        if (flowIt != flowingFluids_.end() && currentBlock == flowIt->second.type)
        {
            if (setBlockUnchecked(cell.x, cell.y, cell.z, BlockType::Air))
            {
                flowingFluids_.erase(cell);
                scheduleFluidNeighborhood(cell.x, cell.y, cell.z);
            }
        }
        return;
    }

    if (desiredBlock == BlockType::Lava && desiredDistance > 0)
    {
        const std::uint32_t lavaCadencePhase = fluidTickCounter_
            + static_cast<std::uint32_t>((cell.x * 73856093) ^ (cell.y * 19349663) ^ (cell.z * 83492791));
        if ((lavaCadencePhase % kLavaHorizontalCadenceTicks) != 0U)
        {
            activeFluidCells_.insert(cell);
            return;
        }
    }

    if ((desiredBlock == BlockType::Water && hasWaterSource) || (desiredBlock == BlockType::Lava && hasLavaSource))
    {
        flowingFluids_.erase(cell);
    }
    else
    {
        flowingFluids_[cell] = FlowingFluidState{
            .type = desiredBlock,
            .horizontalDistance = static_cast<std::uint8_t>(std::clamp(desiredDistance, 0, 255)),
        };
    }

    if (currentBlock != desiredBlock && setBlockUnchecked(cell.x, cell.y, cell.z, desiredBlock))
    {
        scheduleFluidNeighborhood(cell.x, cell.y, cell.z);
    }

    if (desiredBlock != BlockType::Air)
    {
        scheduleFluidNeighborhood(cell.x, cell.y - 1, cell.z);
    }
}
}  // namespace vibecraft::world
