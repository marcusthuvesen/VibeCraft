#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <vector>

#include "vibecraft/core/Logger.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
struct WindowSettings
{
    std::uint32_t width = 1600;
    std::uint32_t height = 900;
};

struct StreamingSettings
{
    int bootstrapChunkRadius = 2;
    int residentChunkRadius = 3;
    int generationChunkRadius = 5;
    std::size_t generationChunkBudgetPerFrame = 12;
    std::size_t prefetchGenerationBudgetPerFrame = 6;
    std::size_t meshBuildBudgetPerFrame = 8;
    std::size_t offResidentDirtyRebuildBudget = 8;
    int forwardPrefetchChunks = 2;
};

struct InputTuning
{
    float moveSpeed = 4.317f;
    float sneakSpeedMultiplier = 0.3f;
    float sprintSpeedMultiplier = 1.3f;
    float mouseSensitivity = 0.09f;
    float reachDistance = 6.0f;
};

struct PlayerMovementSettings
{
    float colliderHalfWidth = 0.3f;
    float standingColliderHeight = 1.8f;
    float sneakingColliderHeight = 1.5f;
    float standingEyeHeight = 1.62f;
    float sneakingEyeHeight = 1.27f;
    float gravity = 32.0f;
    float jumpVelocity = 8.4f;
    float terminalFallVelocity = 45.0f;
    float collisionSweepStep = 0.2f;
    float groundProbeDistance = 0.05f;
};

constexpr WindowSettings kWindowSettings{};
constexpr StreamingSettings kStreamingSettings{};
constexpr InputTuning kInputTuning{};
constexpr PlayerMovementSettings kPlayerMovementSettings{};
constexpr float kFloatEpsilon = 0.0001f;

[[nodiscard]] const char* timeOfDayLabel(const game::TimeOfDayPeriod period)
{
    switch (period)
    {
    case game::TimeOfDayPeriod::Dawn:
        return "dawn";
    case game::TimeOfDayPeriod::Day:
        return "day";
    case game::TimeOfDayPeriod::Dusk:
        return "dusk";
    case game::TimeOfDayPeriod::Night:
    default:
        return "night";
    }
}

[[nodiscard]] const char* weatherLabel(const game::WeatherType weatherType)
{
    switch (weatherType)
    {
    case game::WeatherType::Cloudy:
        return "cloudy";
    case game::WeatherType::Rain:
        return "rain";
    case game::WeatherType::Clear:
    default:
        return "clear";
    }
}

struct Aabb
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
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

void buildVertexNormals(render::SceneMeshData& sceneMesh)
{
    for (render::SceneMeshData::Vertex& vertex : sceneMesh.vertices)
    {
        vertex.normal = glm::vec3(0.0f);
    }

    for (std::size_t index = 0; index + 2 < sceneMesh.indices.size(); index += 3)
    {
        const std::uint32_t index0 = sceneMesh.indices[index + 0];
        const std::uint32_t index1 = sceneMesh.indices[index + 1];
        const std::uint32_t index2 = sceneMesh.indices[index + 2];

        if (index0 >= sceneMesh.vertices.size() || index1 >= sceneMesh.vertices.size()
            || index2 >= sceneMesh.vertices.size())
        {
            continue;
        }

        const glm::vec3 edge01 = sceneMesh.vertices[index1].position - sceneMesh.vertices[index0].position;
        const glm::vec3 edge02 = sceneMesh.vertices[index2].position - sceneMesh.vertices[index0].position;
        const glm::vec3 triangleNormal = glm::cross(edge01, edge02);

        if (glm::dot(triangleNormal, triangleNormal) == 0.0f)
        {
            continue;
        }

        const glm::vec3 faceNormal = glm::normalize(triangleNormal);
        sceneMesh.vertices[index0].normal += faceNormal;
        sceneMesh.vertices[index1].normal += faceNormal;
        sceneMesh.vertices[index2].normal += faceNormal;
    }

    for (render::SceneMeshData::Vertex& vertex : sceneMesh.vertices)
    {
        if (glm::dot(vertex.normal, vertex.normal) == 0.0f)
        {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            continue;
        }

        vertex.normal = glm::normalize(vertex.normal);
    }
}

struct MeshSyncCpuData
{
    std::unordered_set<std::uint64_t> desiredResidentIds;
    std::vector<render::SceneMeshData> sceneMeshesToUpload;
    std::vector<std::uint64_t> removedMeshIds;
    std::vector<world::ChunkCoord> residentDirtyCoords;
};

[[nodiscard]] MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    const std::size_t meshBuildBudget)
{
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(dirtyCoordSet.size());
    cpuData.removedMeshIds.reserve(dirtyCoordSet.size() + residentChunkMeshIds.size());
    cpuData.residentDirtyCoords.reserve(dirtyCoordSet.size());
    std::vector<std::pair<world::ChunkCoord, bool>> pendingResidentCoords;
    pendingResidentCoords.reserve(static_cast<std::size_t>(
        (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1)));

    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            const std::uint64_t meshId = chunkMeshId(coord);
            cpuData.desiredResidentIds.insert(meshId);

            const bool isDirty = dirtyCoordSet.contains(coord);
            const auto chunkIt = world.chunks().find(coord);
            if (chunkIt == world.chunks().end())
            {
                continue;
            }

            const bool isResident = residentChunkMeshIds.contains(meshId);
            if (isResident && !isDirty)
            {
                continue;
            }
            pendingResidentCoords.push_back({coord, isDirty});
        }
    }

    std::sort(
        pendingResidentCoords.begin(),
        pendingResidentCoords.end(),
        [&cameraChunk](const auto& lhs, const auto& rhs)
        {
            const int lhsDx = lhs.first.x - cameraChunk.x;
            const int lhsDz = lhs.first.z - cameraChunk.z;
            const int rhsDx = rhs.first.x - cameraChunk.x;
            const int rhsDz = rhs.first.z - cameraChunk.z;
            const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
            const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
            if (lhsDistanceSq != rhsDistanceSq)
            {
                return lhsDistanceSq < rhsDistanceSq;
            }
            if (lhs.first.z != rhs.first.z)
            {
                return lhs.first.z < rhs.first.z;
            }
            return lhs.first.x < rhs.first.x;
        });

    std::size_t builtThisFrame = 0;
    for (const auto& [coord, isDirty] : pendingResidentCoords)
    {
        if (builtThisFrame >= meshBuildBudget)
        {
            break;
        }

        const std::uint64_t meshId = chunkMeshId(coord);
        meshing::ChunkMeshData meshData = chunkMesher.buildMesh(world, coord);
        if (meshData.vertices.empty() || meshData.indices.empty())
        {
            cpuData.removedMeshIds.push_back(meshId);
            if (isDirty)
            {
                cpuData.residentDirtyCoords.push_back(coord);
            }
            ++builtThisFrame;
            continue;
        }

        render::SceneMeshData sceneMesh;
        sceneMesh.id = meshId;
        sceneMesh.boundsMin = chunkBoundsMin(coord);
        sceneMesh.boundsMax = chunkBoundsMax(coord);
        sceneMesh.indices = std::move(meshData.indices);
        sceneMesh.vertices.reserve(meshData.vertices.size());

        for (const meshing::DebugVertex& vertex : meshData.vertices)
        {
            sceneMesh.vertices.push_back(render::SceneMeshData::Vertex{
                .position = {vertex.x, vertex.y, vertex.z},
                .uv = {vertex.u, vertex.v},
                .abgr = vertex.abgr,
            });
        }

        buildVertexNormals(sceneMesh);
        cpuData.sceneMeshesToUpload.push_back(std::move(sceneMesh));
        if (isDirty)
        {
            cpuData.residentDirtyCoords.push_back(coord);
        }
        ++builtThisFrame;
    }

    for (const std::uint64_t residentId : residentChunkMeshIds)
    {
        if (!cpuData.desiredResidentIds.contains(residentId))
        {
            cpuData.removedMeshIds.push_back(residentId);
        }
    }

    return cpuData;
}

void applyMeshSyncGpuData(
    render::Renderer& renderer,
    const MeshSyncCpuData& cpuData,
    std::unordered_set<std::uint64_t>& residentChunkMeshIds)
{
    if (!cpuData.sceneMeshesToUpload.empty() || !cpuData.removedMeshIds.empty())
    {
        renderer.updateSceneMeshes(cpuData.sceneMeshesToUpload, cpuData.removedMeshIds);
    }

    for (const std::uint64_t removedId : cpuData.removedMeshIds)
    {
        residentChunkMeshIds.erase(removedId);
    }

    for (const render::SceneMeshData& sceneMesh : cpuData.sceneMeshesToUpload)
    {
        residentChunkMeshIds.insert(sceneMesh.id);
    }
}

[[nodiscard]] Aabb playerAabbAt(const glm::vec3& feetPosition, const float colliderHeight)
{
    return Aabb{
        .min = {
            feetPosition.x - kPlayerMovementSettings.colliderHalfWidth,
            feetPosition.y,
            feetPosition.z - kPlayerMovementSettings.colliderHalfWidth,
        },
        .max = {
            feetPosition.x + kPlayerMovementSettings.colliderHalfWidth,
            feetPosition.y + colliderHeight,
            feetPosition.z + kPlayerMovementSettings.colliderHalfWidth,
        },
    };
}

[[nodiscard]] bool collidesWithSolidBlock(const world::World& worldState, const Aabb& aabb);

[[nodiscard]] glm::vec3 findInitialSpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& preferredCameraPosition,
    const float colliderHeight)
{
    const int spawnWorldX = static_cast<int>(std::floor(preferredCameraPosition.x));
    const int spawnWorldZ = static_cast<int>(std::floor(preferredCameraPosition.z));
    glm::vec3 spawnFeetPosition(
        preferredCameraPosition.x,
        static_cast<float>(terrainGenerator.surfaceHeightAt(spawnWorldX, spawnWorldZ) + 1),
        preferredCameraPosition.z);

    // The expanded modern world height moved the terrain surface far above the legacy dev-camera
    // spawn. Start on top of the actual surface, then keep stepping upward until the player
    // capsule has full headroom (e.g. if a tree generated at the spawn column).
    constexpr int kSpawnClearanceSearchLimit = 64;
    for (int attempt = 0; attempt <= kSpawnClearanceSearchLimit; ++attempt)
    {
        if (!collidesWithSolidBlock(worldState, playerAabbAt(spawnFeetPosition, colliderHeight)))
        {
            return spawnFeetPosition;
        }
        spawnFeetPosition.y += 1.0f;
    }

    return spawnFeetPosition;
}

[[nodiscard]] bool collidesWithSolidBlock(const world::World& worldState, const Aabb& aabb)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - kFloatEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - kFloatEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - kFloatEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (world::isSolid(worldState.blockAt(x, y, z)))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool movePlayerAxisWithCollision(
    const world::World& worldState,
    glm::vec3& feetPosition,
    const int axisIndex,
    const float displacement,
    const float colliderHeight)
{
    if (std::abs(displacement) <= kFloatEpsilon)
    {
        return false;
    }

    float remaining = displacement;
    bool blocked = false;

    while (std::abs(remaining) > kFloatEpsilon)
    {
        const float step = std::clamp(
            remaining,
            -kPlayerMovementSettings.collisionSweepStep,
            kPlayerMovementSettings.collisionSweepStep);
        const glm::vec3 basePosition = feetPosition;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        if (!collidesWithSolidBlock(worldState, playerAabbAt(candidatePosition, colliderHeight)))
        {
            feetPosition = candidatePosition;
            remaining -= step;
            continue;
        }

        float low = 0.0f;
        float high = 1.0f;
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float mid = (low + high) * 0.5f;
            glm::vec3 sweepPosition = basePosition;
            sweepPosition[axisIndex] += step * mid;
            if (collidesWithSolidBlock(worldState, playerAabbAt(sweepPosition, colliderHeight)))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feetPosition[axisIndex] += step * low;
        blocked = true;
        break;
    }

    return blocked;
}

[[nodiscard]] bool isGroundedAtFeetPosition(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight)
{
    glm::vec3 groundedProbe = feetPosition;
    groundedProbe.y -= kPlayerMovementSettings.groundProbeDistance;
    return collidesWithSolidBlock(worldState, playerAabbAt(groundedProbe, colliderHeight));
}

[[nodiscard]] bool aabbTouchesBlockType(
    const world::World& worldState,
    const Aabb& aabb,
    const world::BlockType blockType)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - kFloatEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - kFloatEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - kFloatEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (worldState.blockAt(x, y, z) == blockType)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

[[nodiscard]] game::EnvironmentalHazards samplePlayerHazards(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight,
    const float eyeHeight)
{
    const Aabb playerBody = playerAabbAt(feetPosition, colliderHeight);
    const glm::vec3 eyeSamplePosition = feetPosition + glm::vec3(0.0f, eyeHeight - 0.1f, 0.0f);
    const world::BlockType eyeBlock = worldState.blockAt(
        static_cast<int>(std::floor(eyeSamplePosition.x)),
        static_cast<int>(std::floor(eyeSamplePosition.y)),
        static_cast<int>(std::floor(eyeSamplePosition.z)));

    return game::EnvironmentalHazards{
        .bodyInWater = aabbTouchesBlockType(worldState, playerBody, world::BlockType::Water),
        .bodyInLava = aabbTouchesBlockType(worldState, playerBody, world::BlockType::Lava),
        .headSubmergedInWater = eyeBlock == world::BlockType::Water,
    };
}

[[nodiscard]] const char* hazardLabel(const game::EnvironmentalHazards& hazards)
{
    if (hazards.bodyInLava)
    {
        return "lava";
    }
    if (hazards.headSubmergedInWater)
    {
        return "underwater";
    }
    if (hazards.bodyInWater)
    {
        return "water";
    }
    return "clear";
}

}

bool Application::initialize()
{
    core::initializeLogger();

    if (!window_.create("VibeCraft", kWindowSettings.width, kWindowSettings.height))
    {
        return false;
    }

    // Always start from a fresh origin-centered dev world. Persisted worlds are currently easy
    // to invalidate while terrain/render experiments are in flight, so skip loading and clear any
    // previous save to keep reruns deterministic.
    {
        std::error_code errorCode;
        std::filesystem::remove(savePath_, errorCode);
        vibecraft::world::World::ChunkMap emptyChunks;
        world_.replaceChunks(std::move(emptyChunks));
        world_.generateRadius(terrainGenerator_, kStreamingSettings.bootstrapChunkRadius);
    }

    if (!renderer_.initialize(window_.nativeWindowHandle(), window_.width(), window_.height()))
    {
        core::logError("Failed to initialize bgfx.");
        return false;
    }

    window_.setRelativeMouseMode(true);
    mouseCaptured_ = true;

    const float spawnHeight = kPlayerMovementSettings.standingColliderHeight;
    playerFeetPosition_ = findInitialSpawnFeetPosition(world_, terrainGenerator_, camera_.position(), spawnHeight);
    isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, spawnHeight);
    camera_.addYawPitch(90.0f, 35.0f);
    spawnFeetPosition_ = playerFeetPosition_;
    accumulatedFallDistance_ = 0.0f;
    playerVitals_.reset();
    playerHazards_ = samplePlayerHazards(
        world_,
        playerFeetPosition_,
        kPlayerMovementSettings.standingColliderHeight,
        kPlayerMovementSettings.standingEyeHeight);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
    dayNightCycle_.setElapsedSeconds(70.0f);
    weatherSystem_.setElapsedSeconds(0.0f);

    syncWorldData();

    gameScreen_ = GameScreen::Playing;
    return true;
}

int Application::run()
{
    std::uint64_t lastCounter = SDL_GetPerformanceCounter();
    const std::uint64_t frequency = SDL_GetPerformanceFrequency();

    while (!inputState_.quitRequested)
    {
        const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
        const float deltaTimeSeconds =
            static_cast<float>(currentCounter - lastCounter) / static_cast<float>(frequency);
        lastCounter = currentCounter;

        window_.pollEvents(inputState_);
        processInput(deltaTimeSeconds);
        update(deltaTimeSeconds);
        ++runFrameIndex_;
    }

    renderer_.shutdown();
    return 0;
}

void Application::update(const float deltaTimeSeconds)
{
    dayNightCycle_.advanceSeconds(deltaTimeSeconds);
    weatherSystem_.advanceSeconds(deltaTimeSeconds);
    const game::DayNightSample dayNightSample = dayNightCycle_.sample();
    const game::WeatherSample weatherSample = weatherSystem_.sample();
    const glm::vec3 clearSkyTint = dayNightSample.skyTint * weatherSample.skyTintMultiplier;
    const glm::vec3 clearHorizonTint = dayNightSample.horizonTint * weatherSample.horizonTintMultiplier;
    const glm::vec3 skyTint =
        glm::mix(clearSkyTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.28f);
    const glm::vec3 horizonTint =
        glm::mix(clearHorizonTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.18f);
    const float sunLightScale = 1.0f - weatherSample.sunOcclusion * 0.55f;
    const float moonLightScale = 1.0f - weatherSample.cloudCoverage * 0.20f;
    const float visibleSunScale = 1.0f - weatherSample.sunOcclusion * 0.35f;
    const float visibleMoonScale = 1.0f - weatherSample.cloudCoverage * 0.12f;

    const float frameTimeMs = deltaTimeSeconds * 1000.0f;
    if (!frameTimeInitialized_)
    {
        smoothedFrameTimeMs_ = frameTimeMs;
        frameTimeInitialized_ = true;
    }
    else
    {
        constexpr float kFrameTimeSmoothingAlpha = 0.1f;
        smoothedFrameTimeMs_ =
            smoothedFrameTimeMs_ + (frameTimeMs - smoothedFrameTimeMs_) * kFrameTimeSmoothingAlpha;
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        mainMenuTimeSeconds_ += deltaTimeSeconds;
    }

    if (inputState_.windowSizeChanged && window_.width() != 0 && window_.height() != 0)
    {
        renderer_.resize(window_.width(), window_.height());
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        syncWorldData();
    }

    std::optional<world::RaycastHit> raycastHit;
    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        raycastHit = world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    }

    render::FrameDebugData frameDebugData;
    frameDebugData.chunkCount = static_cast<std::uint32_t>(world_.chunks().size());
    frameDebugData.dirtyChunkCount = static_cast<std::uint32_t>(world_.dirtyChunkCount());
    frameDebugData.totalFaces = world_.totalVisibleFaces();
    frameDebugData.residentChunkCount = static_cast<std::uint32_t>(residentChunkMeshIds_.size());
    frameDebugData.cameraPosition = camera_.position();
    frameDebugData.health = playerVitals_.health();
    frameDebugData.maxHealth = playerVitals_.maxHealth();
    const float safeFrameTimeMs = std::max(smoothedFrameTimeMs_, 0.001f);
    const float smoothedFps = 1000.0f / safeFrameTimeMs;
    const int cycleSeconds = static_cast<int>(std::floor(dayNightSample.elapsedSeconds));
    const int cycleMinutesComponent = cycleSeconds / 60;
    const int cycleSecondsComponent = cycleSeconds % 60;
    frameDebugData.statusLine = fmt::format(
        "HP: {:.0f}/{:.0f}  Air: {:.0f}/{:.0f}  Hazard: {}  Mouse: {}  Grounded: {}  Time: {} {:02d}:{:02d}  Weather: {}  Save: {}  Frame: {:.2f} ms ({:.1f} fps){}",
        playerVitals_.health(),
        playerVitals_.maxHealth(),
        playerVitals_.air(),
        playerVitals_.maxAir(),
        hazardLabel(playerHazards_),
        mouseCaptured_ ? "captured" : "released",
        isGrounded_ ? "yes" : "no",
        timeOfDayLabel(dayNightSample.period),
        cycleMinutesComponent,
        cycleSecondsComponent,
        weatherLabel(weatherSample.type),
        savePath_.generic_string(),
        safeFrameTimeMs,
        smoothedFps,
        respawnNotice_.empty() ? "" : fmt::format("  {}", respawnNotice_));
    for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
    {
        frameDebugData.hotbarSlots[slotIndex].blockType = hotbarSlots_[slotIndex].blockType;
        frameDebugData.hotbarSlots[slotIndex].count = hotbarSlots_[slotIndex].count;
    }
    frameDebugData.hotbarSelectedIndex = selectedHotbarIndex_;
    for (std::size_t i = 0; i < frameDebugData.bagSlots.size(); ++i)
    {
        frameDebugData.bagSlots[i].blockType = bagSlots_[i].blockType;
        frameDebugData.bagSlots[i].count = bagSlots_[i].count;
    }

    if (raycastHit.has_value())
    {
        frameDebugData.hasTarget = true;
        frameDebugData.targetBlock = raycastHit->solidBlock;
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        frameDebugData.mainMenuActive = true;
        frameDebugData.mainMenuTimeSeconds = mainMenuTimeSeconds_;
        frameDebugData.mainMenuNotice = mainMenuNotice_;
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight = stats != nullptr ? stats->textHeight : 30;
        frameDebugData.mainMenuHoveredButton = render::Renderer::hitTestMainMenu(
            inputState_.mouseWindowX,
            inputState_.mouseWindowY,
            window_.width(),
            window_.height(),
            textWidth,
            textHeight);
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        frameDebugData.pauseMenuActive = true;
        frameDebugData.pauseMenuNotice = pauseMenuNotice_;
        const bgfx::Stats* const pauseStats = bgfx::getStats();
        const std::uint16_t pauseTextWidth =
            pauseStats != nullptr && pauseStats->textWidth > 0 ? pauseStats->textWidth : 100;
        const std::uint16_t pauseTextHeight = pauseStats != nullptr ? pauseStats->textHeight : 30;
        frameDebugData.pauseMenuHoveredButton = render::Renderer::hitTestPauseMenu(
            inputState_.mouseWindowX,
            inputState_.mouseWindowY,
            window_.width(),
            window_.height(),
            pauseTextWidth,
            pauseTextHeight);
    }

    renderer_.renderFrame(
        frameDebugData,
        render::CameraFrameData{
            .position = camera_.position(),
            .forward = camera_.forward(),
            .up = camera_.up(),
            .skyTint = skyTint,
            .horizonTint = horizonTint,
            .sunDirection = dayNightSample.sunDirection,
            .moonDirection = dayNightSample.moonDirection,
            .sunLightTint = dayNightSample.sunLightTint * sunLightScale,
            .moonLightTint = dayNightSample.moonLightTint * moonLightScale,
            .cloudTint = weatherSample.cloudTint,
            .weatherWindDirectionXZ = weatherSample.windDirectionXZ,
            .sunVisibility = dayNightSample.sunVisibility * visibleSunScale,
            .moonVisibility = dayNightSample.moonVisibility * visibleMoonScale,
            .cloudCoverage = weatherSample.cloudCoverage,
            .rainIntensity = weatherSample.rainIntensity,
            .weatherTimeSeconds = weatherSample.elapsedSeconds,
            .weatherWindSpeed = weatherSample.windSpeed,
        });
}

void Application::processInput(const float deltaTimeSeconds)
{
    if (!inputState_.windowFocused)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        return;
    }

    if (inputState_.escapePressed)
    {
        if (gameScreen_ == GameScreen::Playing)
        {
            gameScreen_ = GameScreen::Paused;
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
            pauseMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::Paused)
        {
            gameScreen_ = GameScreen::Playing;
            mouseCaptured_ = true;
            window_.setRelativeMouseMode(true);
            pauseMenuNotice_.clear();
            inputState_.clearMouseMotion();
        }
    }

    if (inputState_.releaseMouseRequested)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }

    if (inputState_.captureMouseRequested && gameScreen_ == GameScreen::Playing)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight = stats != nullptr ? stats->textHeight : 30;

        // First frame often receives a stray mouse-down (e.g. focus / window activation) that maps
        // onto "Singleplayer" and skips the title screen.
        if (inputState_.leftMousePressed && runFrameIndex_ > 0)
        {
            const int hit = render::Renderer::hitTestMainMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                textWidth,
                textHeight);
            switch (hit)
            {
            case 0:
                gameScreen_ = GameScreen::Playing;
                mouseCaptured_ = true;
                window_.setRelativeMouseMode(true);
                mainMenuNotice_.clear();
                inputState_.clearMouseMotion();
                break;
            case 1:
                mainMenuNotice_ = "Multiplayer is not available yet.";
                break;
            case 2:
                mainMenuNotice_ = "VibeCraft Realms is not available yet.";
                break;
            case 3:
                mainMenuNotice_ = "Options are not available yet.";
                break;
            case 4:
                inputState_.quitRequested = true;
                break;
            case 5:
                mainMenuNotice_ = "Language selection is not available yet.";
                break;
            case 6:
                mainMenuNotice_ = "Accessibility options are not available yet.";
                break;
            default:
                break;
            }
        }
        return;
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight = stats != nullptr ? stats->textHeight : 30;

        if (inputState_.leftMousePressed)
        {
            const int hit = render::Renderer::hitTestPauseMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                textWidth,
                textHeight);
            switch (hit)
            {
            case 0:
                gameScreen_ = GameScreen::Playing;
                mouseCaptured_ = true;
                window_.setRelativeMouseMode(true);
                pauseMenuNotice_.clear();
                inputState_.clearMouseMotion();
                break;
            case 1:
                pauseMenuNotice_ = "Options are not available yet.";
                break;
            case 2:
                gameScreen_ = GameScreen::MainMenu;
                mouseCaptured_ = false;
                window_.setRelativeMouseMode(false);
                pauseMenuNotice_.clear();
                break;
            case 3:
                inputState_.quitRequested = true;
                break;
            default:
                break;
            }
        }
        return;
    }

    if (gameScreen_ != GameScreen::Playing)
    {
        return;
    }

    if (mouseCaptured_)
    {
        camera_.addYawPitch(
            -inputState_.mouseDeltaX * kInputTuning.mouseSensitivity,
            -inputState_.mouseDeltaY * kInputTuning.mouseSensitivity);
    }

    if (inputState_.isKeyDown(SDL_SCANCODE_1))
    {
        selectedHotbarIndex_ = 0;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_2))
    {
        selectedHotbarIndex_ = 1;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_3))
    {
        selectedHotbarIndex_ = 2;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_4))
    {
        selectedHotbarIndex_ = 3;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_5))
    {
        selectedHotbarIndex_ = 4;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_6))
    {
        selectedHotbarIndex_ = 5;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_7))
    {
        selectedHotbarIndex_ = 6;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_8))
    {
        selectedHotbarIndex_ = 7;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_9))
    {
        selectedHotbarIndex_ = 8;
    }

    const bool sneaking = inputState_.isKeyDown(SDL_SCANCODE_LSHIFT);
    const bool sprinting = inputState_.isKeyDown(SDL_SCANCODE_LCTRL) && !sneaking;
    const float colliderHeight = sneaking ? kPlayerMovementSettings.sneakingColliderHeight
                                          : kPlayerMovementSettings.standingColliderHeight;
    const float eyeHeight = sneaking ? kPlayerMovementSettings.sneakingEyeHeight
                                     : kPlayerMovementSettings.standingEyeHeight;
    respawnNotice_.clear();

    float currentMoveSpeed = kInputTuning.moveSpeed;
    if (sneaking)
    {
        currentMoveSpeed *= kInputTuning.sneakSpeedMultiplier;
    }
    else if (sprinting)
    {
        currentMoveSpeed *= kInputTuning.sprintSpeedMultiplier;
    }

    glm::vec3 localMotion(0.0f);
    if (inputState_.isKeyDown(SDL_SCANCODE_W))
    {
        localMotion.z += currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_S))
    {
        localMotion.z -= currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_D))
    {
        localMotion.x -= currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_A))
    {
        localMotion.x += currentMoveSpeed * deltaTimeSeconds;
    }

    glm::vec3 horizontalForward = camera_.forward();
    horizontalForward.y = 0.0f;
    if (glm::dot(horizontalForward, horizontalForward) > 0.0f)
    {
        horizontalForward = glm::normalize(horizontalForward);
    }
    else
    {
        horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 horizontalRight = camera_.right();
    horizontalRight.y = 0.0f;
    if (glm::dot(horizontalRight, horizontalRight) > 0.0f)
    {
        horizontalRight = glm::normalize(horizontalRight);
    }
    else
    {
        horizontalRight = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::vec3 horizontalDisplacement =
        horizontalForward * localMotion.z + horizontalRight * localMotion.x;

    if (glm::dot(horizontalDisplacement, horizontalDisplacement) > 0.0f)
    {
        static_cast<void>(movePlayerAxisWithCollision(
            world_, playerFeetPosition_, 0, horizontalDisplacement.x, colliderHeight));
        static_cast<void>(movePlayerAxisWithCollision(
            world_, playerFeetPosition_, 2, horizontalDisplacement.z, colliderHeight));
    }

    const bool wasGrounded = isGrounded_;
    isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, colliderHeight);

    const bool jumpHeld = inputState_.isKeyDown(SDL_SCANCODE_SPACE);
    if (jumpHeld && !jumpWasHeld_ && isGrounded_)
    {
        verticalVelocity_ = kPlayerMovementSettings.jumpVelocity;
        isGrounded_ = false;
    }
    jumpWasHeld_ = jumpHeld;

    verticalVelocity_ -= kPlayerMovementSettings.gravity * deltaTimeSeconds;
    verticalVelocity_ = std::max(verticalVelocity_, -kPlayerMovementSettings.terminalFallVelocity);

    const glm::vec3 verticalStartPosition = playerFeetPosition_;
    const float verticalDisplacement = verticalVelocity_ * deltaTimeSeconds;
    const bool verticalBlocked =
        movePlayerAxisWithCollision(world_, playerFeetPosition_, 1, verticalDisplacement, colliderHeight);
    if (verticalBlocked)
    {
        if (verticalVelocity_ < 0.0f)
        {
            isGrounded_ = true;
        }
        verticalVelocity_ = 0.0f;
    }
    else
    {
        isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, colliderHeight);
        if (isGrounded_ && verticalVelocity_ < 0.0f)
        {
            verticalVelocity_ = 0.0f;
        }
    }

    playerHazards_ = samplePlayerHazards(world_, playerFeetPosition_, colliderHeight, eyeHeight);
    if (verticalDisplacement < 0.0f && !playerHazards_.bodyInWater)
    {
        accumulatedFallDistance_ += std::max(0.0f, verticalStartPosition.y - playerFeetPosition_.y);
    }

    const bool landedThisFrame = !wasGrounded && isGrounded_;
    if (landedThisFrame)
    {
        static_cast<void>(playerVitals_.applyLandingImpact(accumulatedFallDistance_, playerHazards_.bodyInWater));
        accumulatedFallDistance_ = 0.0f;
    }
    else if (playerHazards_.bodyInWater)
    {
        accumulatedFallDistance_ = 0.0f;
    }

    playerVitals_.tickEnvironment(deltaTimeSeconds, playerHazards_);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, eyeHeight, 0.0f));

    if (playerVitals_.isDead())
    {
        respawnNotice_ = fmt::format("Respawned after {} damage.", game::damageCauseName(playerVitals_.lastDamageCause()));
        respawnPlayer();
    }

    const auto raycastHit =
        world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    if (!raycastHit.has_value())
    {
        return;
    }

    if (inputState_.leftMousePressed)
    {
        if (world_.applyEditCommand(world::WorldEditCommand{
            .action = world::WorldEditAction::Remove,
            .position = raycastHit->solidBlock,
            .blockType = world::BlockType::Air,
        }))
        {
            static_cast<void>(
                addBlockToInventory(hotbarSlots_, bagSlots_, raycastHit->blockType, selectedHotbarIndex_));
        }
    }

    if (inputState_.rightMousePressed)
    {
        InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (selectedSlot.count == 0)
        {
            return;
        }

        if (world_.applyEditCommand(world::WorldEditCommand{
            .action = world::WorldEditAction::Place,
            .position = raycastHit->buildTarget,
            .blockType = selectedSlot.blockType,
        }))
        {
            consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
        }
    }
}

void Application::respawnPlayer()
{
    playerFeetPosition_ = spawnFeetPosition_;
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    isGrounded_ =
        isGroundedAtFeetPosition(world_, playerFeetPosition_, kPlayerMovementSettings.standingColliderHeight);
    playerVitals_.reset();
    playerHazards_ = samplePlayerHazards(
        world_,
        playerFeetPosition_,
        kPlayerMovementSettings.standingColliderHeight,
        kPlayerMovementSettings.standingEyeHeight);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
}

void Application::syncWorldData()
{
    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
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
        kStreamingSettings.meshBuildBudgetPerFrame);
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);

    if (!cpuData.residentDirtyCoords.empty())
    {
        world_.rebuildDirtyMeshes(chunkMesher_, cpuData.residentDirtyCoords);
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
