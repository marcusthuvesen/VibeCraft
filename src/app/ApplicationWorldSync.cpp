#include "vibecraft/app/Application.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationChunkStreaming.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"

namespace vibecraft::world
{
void populateChunkFromTerrain(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator);
}

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::size_t recommendedWorkerCount(const std::size_t hardCap)
{
    const std::size_t hardwareThreads = std::max<std::size_t>(std::thread::hardware_concurrency(), 2);
    return std::clamp(hardwareThreads > 1 ? hardwareThreads - 1 : 1, std::size_t{2}, hardCap);
}

[[nodiscard]] glm::vec3 chunkBoundsMin(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>(coord.x * world::Chunk::kSize),
        static_cast<float>(world::kWorldMinY),
        static_cast<float>(coord.z * world::Chunk::kSize),
    };
}

[[nodiscard]] glm::vec3 chunkBoundsMax(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>((coord.x + 1) * world::Chunk::kSize),
        static_cast<float>(world::kWorldMaxY + 1),
        static_cast<float>((coord.z + 1) * world::Chunk::kSize),
    };
}

inline constexpr int kMeshingVerticalFocusBandHeight = 24;

[[nodiscard]] int verticalFocusBandForY(const int worldY)
{
    const int yFromFloor = worldY - world::kWorldMinY;
    return yFromFloor >= 0 ? yFromFloor / kMeshingVerticalFocusBandHeight
                           : -((-yFromFloor + kMeshingVerticalFocusBandHeight - 1) / kMeshingVerticalFocusBandHeight);
}

[[nodiscard]] meshing::ChunkMeshBuildSettings focusedMeshBuildSettings(const int cameraWorldY)
{
    return meshing::ChunkMeshBuildSettings{
        .prioritizeVerticalWindow = true,
        .focusCenterY = cameraWorldY,
        .renderAboveBlocks = 88,
        .renderBelowBlocks = 52,
    };
}

[[nodiscard]] std::vector<world::ChunkCoord> prioritizedChunkCoordsAround(
    const world::ChunkCoord& center,
    const int chunkRadius)
{
    std::vector<world::ChunkCoord> coords;
    coords.reserve(static_cast<std::size_t>((chunkRadius * 2 + 1) * (chunkRadius * 2 + 1)));
    for (int chunkZ = center.z - chunkRadius; chunkZ <= center.z + chunkRadius; ++chunkZ)
    {
        for (int chunkX = center.x - chunkRadius; chunkX <= center.x + chunkRadius; ++chunkX)
        {
            coords.push_back(world::ChunkCoord{chunkX, chunkZ});
        }
    }

    std::sort(
        coords.begin(),
        coords.end(),
        [&center](const world::ChunkCoord& lhs, const world::ChunkCoord& rhs)
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
    return coords;
}
}  // namespace

void Application::startChunkGenerationWorker()
{
    stopChunkGenerationWorker();
    resetChunkGenerationPipeline();
    chunkGenerationStopRequested_ = false;
    const std::size_t workerCount = recommendedWorkerCount(4);
    chunkGenerationWorkerThreads_.clear();
    chunkGenerationWorkerThreads_.reserve(workerCount);
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
    {
        chunkGenerationWorkerThreads_.emplace_back([this]()
        {
            for (;;)
            {
                AsyncChunkGenerationJob job;
                {
                    std::unique_lock<std::mutex> lock(chunkGenerationMutex_);
                    chunkGenerationCv_.wait(lock, [this]()
                    {
                        return chunkGenerationStopRequested_ || !chunkGenerationPendingJobs_.empty();
                    });
                    if (chunkGenerationStopRequested_ && chunkGenerationPendingJobs_.empty())
                    {
                        return;
                    }
                    job = std::move(chunkGenerationPendingJobs_.front());
                    chunkGenerationPendingJobs_.pop_front();
                }

                world::Chunk chunk(job.coord);
                world::populateChunkFromTerrain(chunk, job.coord, job.terrainGenerator);

                AsyncChunkGenerationResult result{
                    .coord = job.coord,
                    .generation = job.generation,
                    .chunk = std::move(chunk),
                };

                std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
                chunkGenerationCompletedResults_.push_back(std::move(result));
            }
        });
    }
}

void Application::stopChunkGenerationWorker()
{
    {
        std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
        chunkGenerationStopRequested_ = true;
    }
    chunkGenerationCv_.notify_all();
    for (std::thread& worker : chunkGenerationWorkerThreads_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    chunkGenerationWorkerThreads_.clear();
}

void Application::resetChunkGenerationPipeline()
{
    std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
    ++chunkGenerationGeneration_;
    chunkGenerationPendingJobs_.clear();
    chunkGenerationCompletedResults_.clear();
    chunkGenerationInFlightCoords_.clear();
}

void Application::startChunkMeshingWorker()
{
    stopChunkMeshingWorker();
    resetChunkMeshingPipeline();
    chunkMeshingStopRequested_ = false;
    const std::size_t workerCount = recommendedWorkerCount(3);
    chunkMeshingWorkerThreads_.clear();
    chunkMeshingWorkerThreads_.reserve(workerCount);
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
    {
        chunkMeshingWorkerThreads_.emplace_back([this]()
        {
            for (;;)
            {
                AsyncChunkMeshJob job;
                {
                    std::unique_lock<std::mutex> lock(chunkMeshingMutex_);
                    chunkMeshingCv_.wait(lock, [this]()
                    {
                        return chunkMeshingStopRequested_ || !chunkMeshingPendingJobs_.empty();
                    });
                    if (chunkMeshingStopRequested_ && chunkMeshingPendingJobs_.empty())
                    {
                        return;
                    }
                    job = std::move(chunkMeshingPendingJobs_.front());
                    chunkMeshingPendingJobs_.pop_front();
                }

                world::World snapshotWorld;
                snapshotWorld.replaceChunks(std::move(job.snapshotChunks));
                const meshing::ChunkMeshData meshData =
                    chunkMesher_.buildMesh(snapshotWorld, job.coord, job.buildSettings);

                AsyncChunkMeshResult result{
                    .coord = job.coord,
                    .meshId = job.meshId,
                    .dirtyRevision = job.dirtyRevision,
                    .generation = job.generation,
                    .verticalFocusBand = job.verticalFocusBand,
                    .wasDirtyWhenQueued = job.wasDirtyWhenQueued,
                    .stats = {
                        .faceCount = meshData.faceCount,
                        .vertexCount = static_cast<std::uint32_t>(meshData.vertices.size()),
                        .indexCount = static_cast<std::uint32_t>(meshData.indices.size()),
                    },
                    .hasRenderableMesh = !meshData.vertices.empty() && !meshData.indices.empty(),
                };

                if (result.hasRenderableMesh)
                {
                    render::SceneMeshData sceneMesh;
                    sceneMesh.id = job.meshId;
                    sceneMesh.indices = meshData.indices;
                    sceneMesh.vertices.reserve(meshData.vertices.size());

                    glm::vec3 tightMin{0.0f};
                    glm::vec3 tightMax{0.0f};
                    bool haveBounds = false;
                    for (const meshing::DebugVertex& vertex : meshData.vertices)
                    {
                        const glm::vec3 p{vertex.x, vertex.y, vertex.z};
                        sceneMesh.vertices.push_back(render::SceneMeshData::Vertex{
                            .position = p,
                            .normal = {vertex.nx, vertex.ny, vertex.nz},
                            .uv = {vertex.u, vertex.v},
                            .abgr = vertex.abgr,
                        });
                        if (!haveBounds)
                        {
                            tightMin = p;
                            tightMax = p;
                            haveBounds = true;
                        }
                        else
                        {
                            tightMin = glm::min(tightMin, p);
                            tightMax = glm::max(tightMax, p);
                        }
                    }

                    constexpr float kMeshBoundsPad = 0.02f;
                    if (haveBounds)
                    {
                        sceneMesh.boundsMin = tightMin - kMeshBoundsPad;
                        sceneMesh.boundsMax = tightMax + kMeshBoundsPad;
                    }
                    else
                    {
                        sceneMesh.boundsMin = chunkBoundsMin(job.coord);
                        sceneMesh.boundsMax = chunkBoundsMax(job.coord);
                    }
                    result.sceneMesh = std::move(sceneMesh);
                }

                std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
                chunkMeshingCompletedResults_.push_back(std::move(result));
            }
        });
    }
}

void Application::stopChunkMeshingWorker()
{
    {
        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        chunkMeshingStopRequested_ = true;
    }
    chunkMeshingCv_.notify_all();
    for (std::thread& worker : chunkMeshingWorkerThreads_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    chunkMeshingWorkerThreads_.clear();
}

void Application::resetChunkMeshingPipeline()
{
    std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
    ++chunkMeshingGeneration_;
    chunkMeshingPendingJobs_.clear();
    chunkMeshingCompletedResults_.clear();
    chunkMeshingInFlightCoords_.clear();
}

void Application::syncWorldData()
{
    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    const int cameraWorldY = static_cast<int>(std::floor(camera_.position().y));
    const int currentVerticalFocusBand = verticalFocusBandForY(cameraWorldY);
    const meshing::ChunkMeshBuildSettings meshBuildSettings = focusedMeshBuildSettings(cameraWorldY);
    // Clients must only render chunks sent by the host; local procedural fill would desync terrain.
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        std::vector<AsyncChunkGenerationResult> completedGenerationResults;
        {
            std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
            while (!chunkGenerationCompletedResults_.empty()
                   && completedGenerationResults.size() < kStreamingSettings.generationApplyBudgetPerFrame)
            {
                completedGenerationResults.push_back(std::move(chunkGenerationCompletedResults_.front()));
                chunkGenerationCompletedResults_.pop_front();
                chunkGenerationInFlightCoords_.erase(completedGenerationResults.back().coord);
            }
        }

        for (AsyncChunkGenerationResult& result : completedGenerationResults)
        {
            if (result.generation != chunkGenerationGeneration_)
            {
                continue;
            }
            if (world_.chunks().contains(result.coord))
            {
                continue;
            }
            world_.replaceChunk(std::move(result.chunk));
        }

        const auto tryQueueGenerationJob = [this](const world::ChunkCoord& coord)
        {
            if (world_.chunks().contains(coord))
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
            if (chunkGenerationPendingJobs_.size() >= kStreamingSettings.maxQueuedGenerationJobs)
            {
                return false;
            }
            if (chunkGenerationInFlightCoords_.contains(coord))
            {
                return false;
            }

            chunkGenerationInFlightCoords_.insert(coord);
            chunkGenerationPendingJobs_.push_back(AsyncChunkGenerationJob{
                .coord = coord,
                .generation = chunkGenerationGeneration_,
                .terrainGenerator = terrainGenerator_,
            });
            chunkGenerationCv_.notify_one();
            return true;
        };

        std::size_t queuedNearbyChunks = 0;
        for (const world::ChunkCoord& coord :
             prioritizedChunkCoordsAround(cameraChunk, kStreamingSettings.generationChunkRadius))
        {
            if (queuedNearbyChunks >= kStreamingSettings.generationChunkBudgetPerFrame)
            {
                break;
            }
            if (tryQueueGenerationJob(coord))
            {
                ++queuedNearbyChunks;
            }
        }

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
                std::size_t queuedPrefetchChunks = 0;
                for (const world::ChunkCoord& coord :
                     prioritizedChunkCoordsAround(prefetchChunk, kStreamingSettings.generationChunkRadius))
                {
                    if (queuedPrefetchChunks >= kStreamingSettings.prefetchGenerationBudgetPerFrame)
                    {
                        break;
                    }
                    if (tryQueueGenerationJob(coord))
                    {
                        ++queuedPrefetchChunks;
                    }
                }
            }
        }
    }

    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(kStreamingSettings.meshUploadBudgetPerFrame);
    cpuData.removedMeshIds.reserve(residentChunkMeshIds_.size());
    cpuData.dirtyResidentMeshUpdates.reserve(kStreamingSettings.meshUploadBudgetPerFrame);

    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            cpuData.desiredResidentIds.insert(chunkMeshId(world::ChunkCoord{chunkX, chunkZ}));
        }
    }

    for (const std::uint64_t residentId : residentChunkMeshIds_)
    {
        if (!cpuData.desiredResidentIds.contains(residentId))
        {
            cpuData.removedMeshIds.push_back(residentId);
        }
    }

    const auto tryQueueMeshJob =
        [this, &dirtyCoordSet, currentVerticalFocusBand, &meshBuildSettings](const world::ChunkCoord& coord, const bool wasDirty)
    {
        if (!world_.chunks().contains(coord))
        {
            return false;
        }

        world::World::ChunkMap snapshotChunks;
        bool hasCenterChunk = false;
        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const world::ChunkCoord sampleCoord{coord.x + dx, coord.z + dz};
                const auto it = world_.chunks().find(sampleCoord);
                if (it == world_.chunks().end())
                {
                    continue;
                }
                snapshotChunks.emplace(sampleCoord, it->second);
                if (sampleCoord == coord)
                {
                    hasCenterChunk = true;
                }
            }
        }
        if (!hasCenterChunk)
        {
            return false;
        }

        AsyncChunkMeshJob job{
            .coord = coord,
            .meshId = chunkMeshId(coord),
            .dirtyRevision = world_.dirtyRevisionForChunk(coord),
            .generation = chunkMeshingGeneration_,
            .verticalFocusBand = currentVerticalFocusBand,
            .wasDirtyWhenQueued = wasDirty || dirtyCoordSet.contains(coord),
            .buildSettings = meshBuildSettings,
            .snapshotChunks = std::move(snapshotChunks),
        };

        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        if (chunkMeshingPendingJobs_.size() >= kStreamingSettings.maxQueuedMeshJobs)
        {
            return false;
        }
        if (chunkMeshingInFlightCoords_.contains(coord))
        {
            return false;
        }
        chunkMeshingInFlightCoords_.insert(coord);
        chunkMeshingPendingJobs_.push_back(std::move(job));
        chunkMeshingCv_.notify_one();
        return true;
    };

    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            if (!world_.chunks().contains(coord))
            {
                continue;
            }

            const bool isDirty = dirtyCoordSet.contains(coord);
            const std::uint64_t meshId = chunkMeshId(coord);
            const bool hasResidentMesh = residentChunkMeshIds_.contains(meshId);
            const auto existingBandIt = residentChunkMeshVerticalBandById_.find(meshId);
            const bool meshFocusBandMatches =
                existingBandIt != residentChunkMeshVerticalBandById_.end()
                && existingBandIt->second == currentVerticalFocusBand;
            if (!isDirty && hasResidentMesh && meshFocusBandMatches)
            {
                continue;
            }
            static_cast<void>(tryQueueMeshJob(coord, isDirty));
        }
    }

    std::size_t queuedOffResident = 0;
    for (const world::ChunkCoord& coord : dirtyCoords)
    {
        if (cpuData.desiredResidentIds.contains(chunkMeshId(coord)))
        {
            continue;
        }
        if (queuedOffResident >= kStreamingSettings.offResidentDirtyRebuildBudget)
        {
            break;
        }
        if (tryQueueMeshJob(coord, true))
        {
            ++queuedOffResident;
        }
    }

    std::vector<AsyncChunkMeshResult> completedResults;
    completedResults.reserve(kStreamingSettings.meshUploadBudgetPerFrame + kStreamingSettings.offResidentDirtyRebuildBudget);
    {
        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        const std::size_t resultBudget = std::max<std::size_t>(
            kStreamingSettings.meshUploadBudgetPerFrame,
            kStreamingSettings.offResidentDirtyRebuildBudget);
        while (!chunkMeshingCompletedResults_.empty() && completedResults.size() < resultBudget)
        {
            completedResults.push_back(std::move(chunkMeshingCompletedResults_.front()));
            chunkMeshingCompletedResults_.pop_front();
            chunkMeshingInFlightCoords_.erase(completedResults.back().coord);
        }
    }

    std::size_t uploadedMeshesThisFrame = 0;
    for (AsyncChunkMeshResult& result : completedResults)
    {
        if (result.generation != chunkMeshingGeneration_)
        {
            continue;
        }
        if (result.verticalFocusBand != currentVerticalFocusBand)
        {
            continue;
        }
        if (result.dirtyRevision != world_.dirtyRevisionForChunk(result.coord))
        {
            continue;
        }
        if (!world_.chunks().contains(result.coord))
        {
            continue;
        }

        const bool desiredResident = cpuData.desiredResidentIds.contains(result.meshId);
        if (desiredResident && uploadedMeshesThisFrame < kStreamingSettings.meshUploadBudgetPerFrame)
        {
            if (result.hasRenderableMesh)
            {
                cpuData.sceneMeshesToUpload.push_back(std::move(result.sceneMesh));
                residentChunkMeshVerticalBandById_[result.meshId] = result.verticalFocusBand;
            }
            else
            {
                cpuData.removedMeshIds.push_back(result.meshId);
                residentChunkMeshVerticalBandById_.erase(result.meshId);
            }
            ++uploadedMeshesThisFrame;
        }

        if (result.wasDirtyWhenQueued)
        {
            cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                .coord = result.coord,
                .stats = result.stats,
            });
        }
    }

    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);
    for (const std::uint64_t removedId : cpuData.removedMeshIds)
    {
        residentChunkMeshVerticalBandById_.erase(removedId);
    }

    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }
}
}  // namespace vibecraft::app
