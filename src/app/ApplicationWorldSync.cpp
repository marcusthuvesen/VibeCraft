#include "vibecraft/app/Application.hpp"

#include <cmath>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationChunkStreaming.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"

namespace vibecraft::app
{
void Application::syncWorldData()
{
    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    // Clients must only render chunks sent by the host; local procedural fill would desync terrain.
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        world_.generateMissingChunksAround(
            terrainGenerator_,
            cameraChunk,
            kStreamingSettings.generationChunkRadius,
            kStreamingSettings.generationChunkBudgetPerFrame);

        glm::vec3 prefetchDirection = camera_.forward();
        prefetchDirection.y = 0.0f;
        if (glm::dot(prefetchDirection, prefetchDirection) > 0.0f)
        {
            prefetchDirection = glm::normalize(prefetchDirection);
            const float prefetchDistanceWorldUnits =
                static_cast<float>(kStreamingSettings.forwardPrefetchChunks * world::Chunk::kSize);
            const int prefetchWorldX =
                static_cast<int>(std::floor(camera_.position().x + prefetchDirection.x * prefetchDistanceWorldUnits));
            const int prefetchWorldZ =
                static_cast<int>(std::floor(camera_.position().z + prefetchDirection.z * prefetchDistanceWorldUnits));
            const world::ChunkCoord prefetchChunk = world::worldToChunkCoord(prefetchWorldX, prefetchWorldZ);
            if (!(prefetchChunk == cameraChunk))
            {
                world_.generateMissingChunksAround(
                    terrainGenerator_,
                    prefetchChunk,
                    kStreamingSettings.generationChunkRadius,
                    kStreamingSettings.prefetchGenerationBudgetPerFrame);
            }
        }
    }

    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
        world_,
        chunkMesher_,
        residentChunkMeshIds_,
        dirtyCoordSet,
        cameraChunk,
        kStreamingSettings.residentChunkRadius,
        kStreamingSettings.meshBuildBudgetPerFrame);
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);

    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }

    std::vector<world::ChunkCoord> offResidentDirtyCoords;
    offResidentDirtyCoords.reserve(kStreamingSettings.offResidentDirtyRebuildBudget);
    for (const world::ChunkCoord& coord : dirtyCoords)
    {
        if (cpuData.desiredResidentIds.contains(chunkMeshId(coord)))
        {
            continue;
        }

        offResidentDirtyCoords.push_back(coord);
        if (offResidentDirtyCoords.size() >= kStreamingSettings.offResidentDirtyRebuildBudget)
        {
            break;
        }
    }

    if (!offResidentDirtyCoords.empty())
    {
        world_.rebuildDirtyMeshes(chunkMesher_, offResidentDirtyCoords);
    }
}
}  // namespace vibecraft::app
