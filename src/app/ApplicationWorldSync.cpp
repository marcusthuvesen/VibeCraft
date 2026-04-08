#include "vibecraft/app/Application.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fmt/format.h>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationChunkStreaming.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/core/Logger.hpp"

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

inline constexpr int kMeshingVerticalFocusBandHeight = 24;

[[nodiscard]] int verticalFocusBandForY(const int worldY)
{
    const int yFromFloor = worldY - world::kWorldMinY;
    return yFromFloor >= 0 ? yFromFloor / kMeshingVerticalFocusBandHeight
                           : -((-yFromFloor + kMeshingVerticalFocusBandHeight - 1) / kMeshingVerticalFocusBandHeight);
}

[[nodiscard]] meshing::ChunkMeshBuildSettings focusedMeshBuildSettings(const int cameraWorldY)
{
    static_cast<void>(cameraWorldY);
    // Full-height columns: vertical windows caused missing faces (x-ray) when terrain sat outside the camera Y slice.
    return meshing::ChunkMeshBuildSettings{.prioritizeVerticalWindow = false};
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

[[nodiscard]] bool isChunkCoordWithinRadius(
    const world::ChunkCoord& coord,
    const world::ChunkCoord& center,
    const int radius)
{
    return std::abs(coord.x - center.x) <= radius && std::abs(coord.z - center.z) <= radius;
}

[[nodiscard]] double generationApplyTimeBudgetMs(const float smoothedFrameTimeMs)
{
    if (smoothedFrameTimeMs >= 32.0f)
    {
        return 0.8;
    }
    if (smoothedFrameTimeMs >= 24.0f)
    {
        return 1.0;
    }
    if (smoothedFrameTimeMs >= 18.0f)
    {
        return 1.5;
    }
    return 2.2;
}

[[nodiscard]] std::size_t generationApplyCountBudget(
    const float smoothedFrameTimeMs,
    const std::size_t defaultBudget)
{
    if (smoothedFrameTimeMs >= 32.0f)
    {
        return 1;
    }
    if (smoothedFrameTimeMs >= 24.0f)
    {
        return std::min<std::size_t>(defaultBudget, 2);
    }
    if (smoothedFrameTimeMs >= 18.0f)
    {
        return std::min<std::size_t>(defaultBudget, 3);
    }
    return std::min<std::size_t>(defaultBudget, 5);
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
                const std::vector<meshing::ChunkSectionMeshData> sectionMeshes =
                    chunkMesher_.buildSectionMeshes(snapshotWorld, job.coord, job.buildSettings);

                AsyncChunkMeshResult result{
                    .coord = job.coord,
                    .dirtyRevision = job.dirtyRevision,
                    .generation = job.generation,
                    .verticalFocusBand = job.verticalFocusBand,
                    .wasDirtyWhenQueued = job.wasDirtyWhenQueued,
                    .stats = chunkMeshStatsFromSections(sectionMeshes),
                    .hasRenderableMesh = !sectionMeshes.empty(),
                };
                result.sceneMeshes = buildSceneMeshesForChunkSections(job.coord, sectionMeshes);
                result.removedMeshIds = buildRemovedChunkSectionMeshIds(job.coord, sectionMeshes);

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
    using PerfClock = std::chrono::steady_clock;
    using PerfMs = std::chrono::duration<double, std::milli>;
    struct SyncPerfAccumulator
    {
        PerfClock::time_point windowStart = PerfClock::now();
        int frameCount = 0;
        double sumTotalMs = 0.0;
        double maxTotalMs = 0.0;
        double sumGenerationApplyMs = 0.0;
        double sumGenerationQueueMs = 0.0;
        double sumDirtyGatherMs = 0.0;
        double sumDesiredResidentBuildMs = 0.0;
        double sumResidentPruneIdsMs = 0.0;
        double sumQueueResidentMeshMs = 0.0;
        double sumQueueOffResidentMeshMs = 0.0;
        double sumDrainCompletedMeshMs = 0.0;
        double sumApplyCompletedMeshMs = 0.0;
        double sumPruneResidentCoordsMs = 0.0;
        double sumApplyGpuMs = 0.0;
        double sumApplyDirtyStatsMs = 0.0;
        double sumQueuedNearbyGeneration = 0.0;
        double sumQueuedPrefetchGeneration = 0.0;
        double sumQueuedResidentMeshJobs = 0.0;
        double sumQueuedOffResidentMeshJobs = 0.0;
        double sumCompletedMeshResults = 0.0;
        double sumUploadedResidentMeshes = 0.0;
        double sumDesiredResidentCoords = 0.0;
        double sumDirtyCoords = 0.0;
        double sumPendingGenQueue = 0.0;
        double sumInFlightGen = 0.0;
        double sumCompletedGenQueue = 0.0;
        double sumPendingMeshQueue = 0.0;
        double sumInFlightMesh = 0.0;
        double sumCompletedMeshQueue = 0.0;
    };
    static SyncPerfAccumulator perf{};

    const auto syncStart = PerfClock::now();
    double generationApplyMs = 0.0;
    double generationQueueMs = 0.0;
    double dirtyGatherMs = 0.0;
    double desiredResidentBuildMs = 0.0;
    double residentPruneIdsMs = 0.0;
    double queueResidentMeshMs = 0.0;
    double queueOffResidentMeshMs = 0.0;
    double drainCompletedMeshMs = 0.0;
    double applyCompletedMeshMs = 0.0;
    double pruneResidentCoordsMs = 0.0;
    double applyGpuMs = 0.0;
    double applyDirtyStatsMs = 0.0;
    std::size_t queuedNearbyChunks = 0;
    std::size_t queuedPrefetchChunks = 0;
    std::size_t queuedResidentMeshJobs = 0;
    std::size_t queuedOffResident = 0;
    std::size_t completedMeshResultCount = 0;
    std::size_t uploadedMeshesThisFrame = 0;

    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    const int cameraWorldY = static_cast<int>(std::floor(camera_.position().y));
    const int currentVerticalFocusBand = verticalFocusBandForY(cameraWorldY);
    const meshing::ChunkMeshBuildSettings meshBuildSettings = focusedMeshBuildSettings(cameraWorldY);
    const int generationRetainRadius = kStreamingSettings.residentChunkRadius + 2;
    const int meshRetainRadius = kStreamingSettings.generationChunkRadius + 3;
    const int worldRetainRadius = kStreamingSettings.generationChunkRadius + 3;
    // Clients must only render chunks sent by the host; local procedural fill would desync terrain.
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        const auto generationApplyStart = PerfClock::now();
        {
            std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
            for (auto it = chunkGenerationPendingJobs_.begin(); it != chunkGenerationPendingJobs_.end();)
            {
                if (!isChunkCoordWithinRadius(it->coord, cameraChunk, generationRetainRadius))
                {
                    chunkGenerationInFlightCoords_.erase(it->coord);
                    it = chunkGenerationPendingJobs_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            for (auto it = chunkGenerationCompletedResults_.begin(); it != chunkGenerationCompletedResults_.end();)
            {
                if (!isChunkCoordWithinRadius(it->coord, cameraChunk, generationRetainRadius))
                {
                    chunkGenerationInFlightCoords_.erase(it->coord);
                    it = chunkGenerationCompletedResults_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        std::size_t pendingGenerationJobs = 0;
        std::size_t generationOutstanding = 0;
        std::size_t completedGenerationBacklog = 0;
        {
            std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
            pendingGenerationJobs = chunkGenerationPendingJobs_.size();
            generationOutstanding = chunkGenerationInFlightCoords_.size();
            completedGenerationBacklog = chunkGenerationCompletedResults_.size();
        }
        const std::size_t applyCountBudget =
            generationApplyCountBudget(smoothedFrameTimeMs_, kStreamingSettings.generationApplyBudgetPerFrame);
        const double applyTimeBudgetMs = generationApplyTimeBudgetMs(smoothedFrameTimeMs_);
        std::size_t appliedGenerationResults = 0;
        while (appliedGenerationResults < applyCountBudget)
        {
            AsyncChunkGenerationResult result;
            {
                std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
                if (chunkGenerationCompletedResults_.empty())
                {
                    break;
                }
                result = std::move(chunkGenerationCompletedResults_.front());
                chunkGenerationCompletedResults_.pop_front();
                chunkGenerationInFlightCoords_.erase(result.coord);
            }
            if (result.generation != chunkGenerationGeneration_)
            {
                continue;
            }
            if (!isChunkCoordWithinRadius(result.coord, cameraChunk, generationRetainRadius))
            {
                continue;
            }
            if (world_.chunks().contains(result.coord))
            {
                continue;
            }
            world_.replaceChunk(std::move(result.chunk));
            ++appliedGenerationResults;
            if (PerfMs(PerfClock::now() - generationApplyStart).count() >= applyTimeBudgetMs)
            {
                break;
            }
        }
        generationApplyMs = PerfMs(PerfClock::now() - generationApplyStart).count();

        std::size_t maxQueuedGenerationJobs = kStreamingSettings.maxQueuedGenerationJobs;
        if (smoothedFrameTimeMs_ >= 30.0f)
        {
            maxQueuedGenerationJobs = std::min<std::size_t>(maxQueuedGenerationJobs, 48);
        }
        else if (smoothedFrameTimeMs_ >= 20.0f)
        {
            maxQueuedGenerationJobs = std::min<std::size_t>(maxQueuedGenerationJobs, 72);
        }
        else if (smoothedFrameTimeMs_ >= 16.0f)
        {
            maxQueuedGenerationJobs = std::min<std::size_t>(maxQueuedGenerationJobs, 96);
        }

        const auto tryQueueGenerationJob = [this, maxQueuedGenerationJobs](const world::ChunkCoord& coord)
        {
            if (world_.chunks().contains(coord))
            {
                return false;
            }

            std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
            if (chunkGenerationPendingJobs_.size() >= maxQueuedGenerationJobs)
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

        const std::size_t generationTotalBacklog =
            pendingGenerationJobs + generationOutstanding + completedGenerationBacklog;
        const bool generationBacklogHigh =
            generationTotalBacklog >= kStreamingSettings.maxQueuedGenerationJobs / 2
            || completedGenerationBacklog >= kStreamingSettings.generationApplyBudgetPerFrame * 3;
        const bool generationBacklogCritical =
            generationTotalBacklog >= (kStreamingSettings.maxQueuedGenerationJobs * 2) / 3
            || completedGenerationBacklog >= kStreamingSettings.generationApplyBudgetPerFrame * 6;
        const bool frameUnderPressure = smoothedFrameTimeMs_ >= 20.0f;
        const bool frameSeverelyUnderPressure = smoothedFrameTimeMs_ >= 30.0f;
        std::size_t nearbyGenerationBudget = kStreamingSettings.generationChunkBudgetPerFrame;
        if (generationBacklogCritical || frameSeverelyUnderPressure)
        {
            nearbyGenerationBudget = std::min<std::size_t>(nearbyGenerationBudget, 2);
        }
        else if (generationBacklogHigh || frameUnderPressure)
        {
            nearbyGenerationBudget = std::max<std::size_t>(1, nearbyGenerationBudget / 3);
        }
        std::size_t prefetchGenerationBudget = kStreamingSettings.prefetchGenerationBudgetPerFrame;
        if (generationTotalBacklog >= 32 || generationBacklogCritical || frameSeverelyUnderPressure)
        {
            prefetchGenerationBudget = 0;
        }
        else if (generationTotalBacklog >= 20 || generationBacklogHigh || frameUnderPressure)
        {
            prefetchGenerationBudget = std::min<std::size_t>(prefetchGenerationBudget, 1);
        }

        const auto generationQueueStart = PerfClock::now();
        for (const world::ChunkCoord& coord :
             prioritizedChunkCoordsAround(cameraChunk, kStreamingSettings.generationChunkRadius))
        {
            if (queuedNearbyChunks >= nearbyGenerationBudget)
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
            if (!(prefetchChunk == cameraChunk) && prefetchGenerationBudget > 0)
            {
                for (const world::ChunkCoord& coord :
                     prioritizedChunkCoordsAround(prefetchChunk, kStreamingSettings.generationChunkRadius))
                {
                    if (queuedPrefetchChunks >= prefetchGenerationBudget)
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
        generationQueueMs = PerfMs(PerfClock::now() - generationQueueStart).count();
    }

    {
        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        for (auto it = chunkMeshingPendingJobs_.begin(); it != chunkMeshingPendingJobs_.end();)
        {
            if (!isChunkCoordWithinRadius(it->coord, cameraChunk, meshRetainRadius))
            {
                chunkMeshingInFlightCoords_.erase(it->coord);
                it = chunkMeshingPendingJobs_.erase(it);
            }
            else
            {
                ++it;
            }
        }
        for (auto it = chunkMeshingCompletedResults_.begin(); it != chunkMeshingCompletedResults_.end();)
        {
            if (!isChunkCoordWithinRadius(it->coord, cameraChunk, meshRetainRadius))
            {
                chunkMeshingInFlightCoords_.erase(it->coord);
                it = chunkMeshingCompletedResults_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    if (multiplayerMode_ == MultiplayerRuntimeMode::SinglePlayer)
    {
        std::size_t unloadBudget = 12;
        if (smoothedFrameTimeMs_ >= 28.0f)
        {
            unloadBudget = 4;
        }
        else if (smoothedFrameTimeMs_ >= 20.0f)
        {
            unloadBudget = 6;
        }
        const std::size_t unloadedChunks =
            world_.unloadChunksOutsideRadius(cameraChunk, worldRetainRadius, unloadBudget);
        if (unloadedChunks > 0)
        {
            for (auto it = residentChunkVerticalBandByCoord_.begin(); it != residentChunkVerticalBandByCoord_.end();)
            {
                if (!world_.chunks().contains(it->first))
                {
                    it = residentChunkVerticalBandByCoord_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    const auto dirtyGatherStart = PerfClock::now();
    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    dirtyGatherMs = PerfMs(PerfClock::now() - dirtyGatherStart).count();
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(kStreamingSettings.meshUploadBudgetPerFrame);
    cpuData.removedMeshIds.reserve(residentChunkMeshIds_.size());
    cpuData.residentCoordsToAdd.reserve(kStreamingSettings.meshUploadBudgetPerFrame);
    cpuData.removedResidentCoords.reserve(residentChunkCoords_.size());
    cpuData.dirtyResidentMeshUpdates.reserve(kStreamingSettings.meshUploadBudgetPerFrame);

    const auto desiredResidentBuildStart = PerfClock::now();
    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            cpuData.desiredResidentCoords.insert(coord);
            for (int sectionIndex = 0; sectionIndex < meshing::kChunkRenderSectionCount; ++sectionIndex)
            {
                cpuData.desiredResidentIds.insert(chunkSectionMeshId(coord, sectionIndex));
            }
        }
    }
    desiredResidentBuildMs = PerfMs(PerfClock::now() - desiredResidentBuildStart).count();

    const auto residentPruneIdsStart = PerfClock::now();
    for (const std::uint64_t residentId : residentChunkMeshIds_)
    {
        if (!cpuData.desiredResidentIds.contains(residentId))
        {
            cpuData.removedMeshIds.push_back(residentId);
        }
    }
    residentPruneIdsMs = PerfMs(PerfClock::now() - residentPruneIdsStart).count();

    const auto tryQueueMeshJob =
        [this, &dirtyCoordSet, currentVerticalFocusBand, &meshBuildSettings](const world::ChunkCoord& coord, const bool wasDirty)
    {
        if (!world_.chunks().contains(coord))
        {
            return false;
        }

            {
                std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
                if (chunkMeshingPendingJobs_.size() >= kStreamingSettings.maxQueuedMeshJobs)
                {
                    return false;
                }
                if (chunkMeshingInFlightCoords_.contains(coord))
                {
                    return false;
                }
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

        std::size_t meshPendingJobs = 0;
        std::size_t meshInFlightJobs = 0;
        std::size_t meshCompletedResults = 0;
        {
            std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
            meshPendingJobs = chunkMeshingPendingJobs_.size();
            meshInFlightJobs = chunkMeshingInFlightCoords_.size();
            meshCompletedResults = chunkMeshingCompletedResults_.size();
        }
        const std::size_t meshBacklog = meshPendingJobs + meshInFlightJobs + meshCompletedResults;
        std::size_t residentMeshQueueBudget = static_cast<std::size_t>(-1);
        if (meshBacklog >= (kStreamingSettings.maxQueuedMeshJobs * 3) / 4 || smoothedFrameTimeMs_ >= 28.0f)
        {
            residentMeshQueueBudget = 2;
        }
        else if (meshBacklog >= kStreamingSettings.maxQueuedMeshJobs / 2 || smoothedFrameTimeMs_ >= 20.0f)
        {
            residentMeshQueueBudget = 5;
        }
        else if (meshBacklog >= kStreamingSettings.maxQueuedMeshJobs / 3 || smoothedFrameTimeMs_ >= 16.0f)
        {
            residentMeshQueueBudget = 9;
        }

    const auto queueResidentMeshStart = PerfClock::now();
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
            const bool hasResidentMesh = residentChunkCoords_.contains(coord);
            const auto existingBandIt = residentChunkVerticalBandByCoord_.find(coord);
            const bool meshFocusBandMatches =
                existingBandIt != residentChunkVerticalBandByCoord_.end()
                && existingBandIt->second == currentVerticalFocusBand;
            if (!isDirty && hasResidentMesh && meshFocusBandMatches)
            {
                continue;
            }
                if (queuedResidentMeshJobs >= residentMeshQueueBudget)
                {
                    continue;
                }
            if (tryQueueMeshJob(coord, isDirty))
            {
                ++queuedResidentMeshJobs;
            }
        }
    }
    queueResidentMeshMs = PerfMs(PerfClock::now() - queueResidentMeshStart).count();

        std::size_t offResidentDirtyBudget = kStreamingSettings.offResidentDirtyRebuildBudget;
        if (meshBacklog >= (kStreamingSettings.maxQueuedMeshJobs * 3) / 4 || smoothedFrameTimeMs_ >= 28.0f)
        {
            offResidentDirtyBudget = 0;
        }
        else if (meshBacklog >= kStreamingSettings.maxQueuedMeshJobs / 2 || smoothedFrameTimeMs_ >= 20.0f)
        {
            offResidentDirtyBudget = std::min<std::size_t>(offResidentDirtyBudget, 2);
        }

    const auto queueOffResidentMeshStart = PerfClock::now();
    for (const world::ChunkCoord& coord : dirtyCoords)
    {
        if (cpuData.desiredResidentCoords.contains(coord))
        {
            continue;
        }
            if (queuedOffResident >= offResidentDirtyBudget)
        {
            break;
        }
        if (tryQueueMeshJob(coord, true))
        {
            ++queuedOffResident;
        }
    }
    queueOffResidentMeshMs = PerfMs(PerfClock::now() - queueOffResidentMeshStart).count();

    const auto drainCompletedMeshStart = PerfClock::now();
    std::vector<AsyncChunkMeshResult> completedResults;
        completedResults.reserve(kStreamingSettings.meshUploadBudgetPerFrame + offResidentDirtyBudget);
    {
        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        const std::size_t resultBudget = std::max<std::size_t>(
            kStreamingSettings.meshUploadBudgetPerFrame,
                offResidentDirtyBudget);
        while (!chunkMeshingCompletedResults_.empty() && completedResults.size() < resultBudget)
        {
            completedResults.push_back(std::move(chunkMeshingCompletedResults_.front()));
            chunkMeshingCompletedResults_.pop_front();
            chunkMeshingInFlightCoords_.erase(completedResults.back().coord);
        }
    }
    drainCompletedMeshMs = PerfMs(PerfClock::now() - drainCompletedMeshStart).count();
    completedMeshResultCount = completedResults.size();

    const auto applyCompletedMeshStart = PerfClock::now();
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

        const bool desiredResident = cpuData.desiredResidentCoords.contains(result.coord);
        if (desiredResident && uploadedMeshesThisFrame < kStreamingSettings.meshUploadBudgetPerFrame)
        {
            cpuData.sceneMeshesToUpload.insert(
                cpuData.sceneMeshesToUpload.end(),
                result.sceneMeshes.begin(),
                result.sceneMeshes.end());
            cpuData.removedMeshIds.insert(
                cpuData.removedMeshIds.end(),
                result.removedMeshIds.begin(),
                result.removedMeshIds.end());
            if (result.hasRenderableMesh)
            {
                cpuData.residentCoordsToAdd.push_back(result.coord);
                residentChunkVerticalBandByCoord_[result.coord] = result.verticalFocusBand;
            }
            else
            {
                cpuData.removedResidentCoords.push_back(result.coord);
                residentChunkVerticalBandByCoord_.erase(result.coord);
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
    applyCompletedMeshMs = PerfMs(PerfClock::now() - applyCompletedMeshStart).count();

    const auto pruneResidentCoordsStart = PerfClock::now();
    for (const world::ChunkCoord& residentCoord : residentChunkCoords_)
    {
        if (!cpuData.desiredResidentCoords.contains(residentCoord))
        {
            cpuData.removedResidentCoords.push_back(residentCoord);
            residentChunkVerticalBandByCoord_.erase(residentCoord);
        }
    }
    pruneResidentCoordsMs = PerfMs(PerfClock::now() - pruneResidentCoordsStart).count();

    const auto applyGpuStart = PerfClock::now();
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_, residentChunkCoords_);
    applyGpuMs = PerfMs(PerfClock::now() - applyGpuStart).count();

    const auto applyDirtyStatsStart = PerfClock::now();
    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }
    applyDirtyStatsMs = PerfMs(PerfClock::now() - applyDirtyStatsStart).count();

    const double totalSyncMs = PerfMs(PerfClock::now() - syncStart).count();

    std::size_t pendingGenQueue = 0;
    std::size_t inFlightGen = 0;
    std::size_t completedGenQueue = 0;
    {
        std::lock_guard<std::mutex> lock(chunkGenerationMutex_);
        pendingGenQueue = chunkGenerationPendingJobs_.size();
        inFlightGen = chunkGenerationInFlightCoords_.size();
        completedGenQueue = chunkGenerationCompletedResults_.size();
    }

    std::size_t pendingMeshQueue = 0;
    std::size_t inFlightMesh = 0;
    std::size_t completedMeshQueue = 0;
    {
        std::lock_guard<std::mutex> lock(chunkMeshingMutex_);
        pendingMeshQueue = chunkMeshingPendingJobs_.size();
        inFlightMesh = chunkMeshingInFlightCoords_.size();
        completedMeshQueue = chunkMeshingCompletedResults_.size();
    }

    const auto now = PerfClock::now();
    ++perf.frameCount;
    perf.sumTotalMs += totalSyncMs;
    perf.maxTotalMs = std::max(perf.maxTotalMs, totalSyncMs);
    perf.sumGenerationApplyMs += generationApplyMs;
    perf.sumGenerationQueueMs += generationQueueMs;
    perf.sumDirtyGatherMs += dirtyGatherMs;
    perf.sumDesiredResidentBuildMs += desiredResidentBuildMs;
    perf.sumResidentPruneIdsMs += residentPruneIdsMs;
    perf.sumQueueResidentMeshMs += queueResidentMeshMs;
    perf.sumQueueOffResidentMeshMs += queueOffResidentMeshMs;
    perf.sumDrainCompletedMeshMs += drainCompletedMeshMs;
    perf.sumApplyCompletedMeshMs += applyCompletedMeshMs;
    perf.sumPruneResidentCoordsMs += pruneResidentCoordsMs;
    perf.sumApplyGpuMs += applyGpuMs;
    perf.sumApplyDirtyStatsMs += applyDirtyStatsMs;
    perf.sumQueuedNearbyGeneration += static_cast<double>(queuedNearbyChunks);
    perf.sumQueuedPrefetchGeneration += static_cast<double>(queuedPrefetchChunks);
    perf.sumQueuedResidentMeshJobs += static_cast<double>(queuedResidentMeshJobs);
    perf.sumQueuedOffResidentMeshJobs += static_cast<double>(queuedOffResident);
    perf.sumCompletedMeshResults += static_cast<double>(completedMeshResultCount);
    perf.sumUploadedResidentMeshes += static_cast<double>(uploadedMeshesThisFrame);
    perf.sumDesiredResidentCoords += static_cast<double>(cpuData.desiredResidentCoords.size());
    perf.sumDirtyCoords += static_cast<double>(dirtyCoords.size());
    perf.sumPendingGenQueue += static_cast<double>(pendingGenQueue);
    perf.sumInFlightGen += static_cast<double>(inFlightGen);
    perf.sumCompletedGenQueue += static_cast<double>(completedGenQueue);
    perf.sumPendingMeshQueue += static_cast<double>(pendingMeshQueue);
    perf.sumInFlightMesh += static_cast<double>(inFlightMesh);
    perf.sumCompletedMeshQueue += static_cast<double>(completedMeshQueue);

    if (std::chrono::duration<double>(now - perf.windowStart).count() >= 1.0 && perf.frameCount > 0)
    {
        const double frameCount = static_cast<double>(perf.frameCount);
        core::logInfo(fmt::format(
            "[perf-sync] total_ms={:.2f} max_ms={:.2f} genApply={:.2f} genQueue={:.2f} dirtyGather={:.2f} "
            "desiredBuild={:.2f} pruneIds={:.2f} qResident={:.2f} qOffResident={:.2f} drainResults={:.2f} "
            "applyResults={:.2f} pruneCoords={:.2f} gpuApply={:.2f} applyDirtyStats={:.2f}",
            perf.sumTotalMs / frameCount,
            perf.maxTotalMs,
            perf.sumGenerationApplyMs / frameCount,
            perf.sumGenerationQueueMs / frameCount,
            perf.sumDirtyGatherMs / frameCount,
            perf.sumDesiredResidentBuildMs / frameCount,
            perf.sumResidentPruneIdsMs / frameCount,
            perf.sumQueueResidentMeshMs / frameCount,
            perf.sumQueueOffResidentMeshMs / frameCount,
            perf.sumDrainCompletedMeshMs / frameCount,
            perf.sumApplyCompletedMeshMs / frameCount,
            perf.sumPruneResidentCoordsMs / frameCount,
            perf.sumApplyGpuMs / frameCount,
            perf.sumApplyDirtyStatsMs / frameCount));
        core::logInfo(fmt::format(
            "[perf-sync-counts] desiredResident={:.1f} dirty={:.1f} qGenNear={:.1f} qGenPrefetch={:.1f} "
            "qMeshResident={:.1f} qMeshOffResident={:.1f} completedMeshResults={:.1f} uploadedResident={:.1f} "
            "genQ/pending={:.1f}/{:.1f}/{:.1f} meshQ/pending={:.1f}/{:.1f}/{:.1f}",
            perf.sumDesiredResidentCoords / frameCount,
            perf.sumDirtyCoords / frameCount,
            perf.sumQueuedNearbyGeneration / frameCount,
            perf.sumQueuedPrefetchGeneration / frameCount,
            perf.sumQueuedResidentMeshJobs / frameCount,
            perf.sumQueuedOffResidentMeshJobs / frameCount,
            perf.sumCompletedMeshResults / frameCount,
            perf.sumUploadedResidentMeshes / frameCount,
            perf.sumPendingGenQueue / frameCount,
            perf.sumInFlightGen / frameCount,
            perf.sumCompletedGenQueue / frameCount,
            perf.sumPendingMeshQueue / frameCount,
            perf.sumInFlightMesh / frameCount,
            perf.sumCompletedMeshQueue / frameCount));
        perf = {};
        perf.windowStart = now;
    }
}
}  // namespace vibecraft::app
