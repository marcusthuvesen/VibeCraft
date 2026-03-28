#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <unordered_set>
#include <vector>

#include "vibecraft/core/Logger.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"
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
constexpr float kNetworkTickSeconds = 1.0f / 20.0f;

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
    std::vector<world::ChunkMeshUpdate> dirtyResidentMeshUpdates;
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
    cpuData.dirtyResidentMeshUpdates.reserve(dirtyCoordSet.size());
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
                cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                    .coord = coord,
                    .stats = world::ChunkMeshStats{},
                });
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
        const world::ChunkMeshStats chunkMeshStats{
            .faceCount = meshData.faceCount,
            .vertexCount = static_cast<std::uint32_t>(meshData.vertices.size()),
            .indexCount = static_cast<std::uint32_t>(meshData.indices.size()),
        };
        cpuData.sceneMeshesToUpload.push_back(std::move(sceneMesh));
        if (isDirty)
        {
            cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                .coord = coord,
                .stats = chunkMeshStats,
            });
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

[[nodiscard]] std::string resolveJoinAddressFromEnvironment()
{
    if (const char* const address = std::getenv("VIBECRAFT_JOIN_ADDRESS"); address != nullptr
        && address[0] != '\0')
    {
        return std::string(address);
    }
    return "127.0.0.1";
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

    multiplayerAddress_ = resolveJoinAddressFromEnvironment();

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
    camera_.addYawPitch(90.0f, 0.0f);
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

    gameScreen_ = GameScreen::MainMenu;
    mainMenuNotice_ = "Press Multiplayer, then H to host or J to join " + multiplayerAddress_ + ".";
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

    stopMultiplayerSessions();
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

    updateMultiplayer(deltaTimeSeconds);

    if (inputState_.windowSizeChanged && window_.width() != 0 && window_.height() != 0)
    {
        renderer_.resize(window_.width(), window_.height());
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        syncWorldData();
        const float currentEyeHeight = std::max(0.0f, camera_.position().y - playerFeetPosition_.y);
        updateDroppedItems(deltaTimeSeconds, currentEyeHeight);
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
        "HP: {:.0f}/{:.0f}  Air: {:.0f}/{:.0f}  Hazard: {}  Mouse: {}  Grounded: {}  Time: {} {:02d}:{:02d}  Weather: {}  Save: {}  Net: {}  Peers: {}  Frame: {:.2f} ms ({:.1f} fps){}",
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
        multiplayerStatusLine_.empty() ? "offline" : multiplayerStatusLine_,
        remotePlayers_.size(),
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
    frameDebugData.worldPickups.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        const float bobOffset = 0.14f + std::sin(droppedItem.ageSeconds * 6.0f) * 0.08f;
        frameDebugData.worldPickups.push_back(render::FrameDebugData::WorldPickupHud{
            .blockType = droppedItem.blockType,
            .worldPosition = droppedItem.worldPosition + glm::vec3(0.0f, bobOffset, 0.0f),
            .spinRadians = droppedItem.spinRadians,
        });
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
        handleMainMenuMultiplayerShortcuts();
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
                stopMultiplayerSessions();
                gameScreen_ = GameScreen::Playing;
                mouseCaptured_ = true;
                window_.setRelativeMouseMode(true);
                mainMenuNotice_.clear();
                inputState_.clearMouseMotion();
                break;
            case 1:
                mainMenuNotice_ = fmt::format(
                    "Press H to host on {} or J to join {}:{}.",
                    multiplayerPort_,
                    multiplayerAddress_,
                    multiplayerPort_);
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
                stopMultiplayerSessions();
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
        const world::WorldEditCommand command{
            .action = world::WorldEditAction::Remove,
            .position = raycastHit->solidBlock,
            .blockType = world::BlockType::Air,
        };
        if (world_.applyEditCommand(command))
        {
            spawnDroppedItem(raycastHit->blockType, raycastHit->solidBlock);
            if (hostSession_ != nullptr && hostSession_->running())
            {
                hostSession_->broadcastBlockEdit({
                    .authorClientId = localClientId_,
                    .action = command.action,
                    .x = command.position.x,
                    .y = command.position.y,
                    .z = command.position.z,
                    .blockType = command.blockType,
                });
            }
            if (clientSession_ != nullptr && clientSession_->connected())
            {
                clientSession_->sendInput(
                    {
                        .clientId = clientSession_->clientId(),
                        .breakBlock = true,
                        .targetX = command.position.x,
                        .targetY = command.position.y,
                        .targetZ = command.position.z,
                    },
                    networkServerTick_);
            }
        }
    }

    if (inputState_.rightMousePressed)
    {
        InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (selectedSlot.count == 0)
        {
            return;
        }

        const world::WorldEditCommand command{
            .action = world::WorldEditAction::Place,
            .position = raycastHit->buildTarget,
            .blockType = selectedSlot.blockType,
        };
        if (world_.applyEditCommand(command))
        {
            consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
            if (hostSession_ != nullptr && hostSession_->running())
            {
                hostSession_->broadcastBlockEdit({
                    .authorClientId = localClientId_,
                    .action = command.action,
                    .x = command.position.x,
                    .y = command.position.y,
                    .z = command.position.z,
                    .blockType = command.blockType,
                });
            }
            if (clientSession_ != nullptr && clientSession_->connected())
            {
                clientSession_->sendInput(
                    {
                        .clientId = clientSession_->clientId(),
                        .placeBlock = true,
                        .targetX = command.position.x,
                        .targetY = command.position.y,
                        .targetZ = command.position.z,
                        .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                        .placeBlockType = command.blockType,
                    },
                    networkServerTick_);
            }
        }
    }
}

void Application::spawnDroppedItem(
    const world::BlockType blockType,
    const glm::ivec3& blockPosition)
{
    if (blockType == world::BlockType::Air || blockType == world::BlockType::Water
        || blockType == world::BlockType::Lava)
    {
        return;
    }

    droppedItems_.push_back(DroppedItem{
        .blockType = blockType,
        .worldPosition = glm::vec3(
            static_cast<float>(blockPosition.x) + 0.5f,
            static_cast<float>(blockPosition.y) + 0.2f,
            static_cast<float>(blockPosition.z) + 0.5f),
        .velocity = glm::vec3(
            std::sin(static_cast<float>(blockPosition.x + blockPosition.z) * 0.73f) * 1.05f,
            2.0f,
            std::cos(static_cast<float>(blockPosition.x - blockPosition.z) * 0.61f) * 1.05f),
        .ageSeconds = 0.0f,
        .pickupDelaySeconds = 0.25f,
        .spinRadians = 0.0f,
    });
}

void Application::updateDroppedItems(const float deltaTimeSeconds, const float eyeHeight)
{
    const glm::vec3 pickupCenter = playerFeetPosition_ + glm::vec3(0.0f, eyeHeight * 0.6f, 0.0f);
    constexpr float kPickupRadius = 1.25f;
    constexpr float kPickupRadiusSq = kPickupRadius * kPickupRadius;
    constexpr float kMagnetRadius = 3.8f;
    constexpr float kMagnetRadiusSq = kMagnetRadius * kMagnetRadius;
    constexpr float kGravity = 16.0f;
    constexpr float kTau = 6.28318530718f;

    std::size_t itemIndex = 0;
    while (itemIndex < droppedItems_.size())
    {
        DroppedItem& droppedItem = droppedItems_[itemIndex];
        droppedItem.ageSeconds += deltaTimeSeconds;
        droppedItem.pickupDelaySeconds =
            std::max(0.0f, droppedItem.pickupDelaySeconds - deltaTimeSeconds);
        droppedItem.spinRadians = std::fmod(
            droppedItem.spinRadians + deltaTimeSeconds * 4.2f,
            kTau);

        droppedItem.velocity.y -= kGravity * deltaTimeSeconds;
        droppedItem.velocity *= std::pow(0.96f, deltaTimeSeconds * 60.0f);

        const glm::vec3 toPlayer = pickupCenter - droppedItem.worldPosition;
        const float distanceToPlayerSq = glm::dot(toPlayer, toPlayer);
        if (droppedItem.pickupDelaySeconds <= 0.0f && distanceToPlayerSq <= kMagnetRadiusSq)
        {
            const float distanceToPlayer = std::sqrt(std::max(distanceToPlayerSq, 0.0001f));
            const glm::vec3 pullDirection = toPlayer / distanceToPlayer;
            const float pullStrength = 8.0f + (1.0f - distanceToPlayer / kMagnetRadius) * 18.0f;
            droppedItem.velocity += pullDirection * pullStrength * deltaTimeSeconds;
        }

        droppedItem.worldPosition += droppedItem.velocity * deltaTimeSeconds;

        const int belowX = static_cast<int>(std::floor(droppedItem.worldPosition.x));
        const int belowY = static_cast<int>(std::floor(droppedItem.worldPosition.y - 0.36f));
        const int belowZ = static_cast<int>(std::floor(droppedItem.worldPosition.z));
        if (world::isSolid(world_.blockAt(belowX, belowY, belowZ)) && droppedItem.velocity.y < 0.0f)
        {
            droppedItem.worldPosition.y = static_cast<float>(belowY + 1) + 0.36f;
            droppedItem.velocity.y = 0.0f;
        }

        const glm::vec3 delta = droppedItem.worldPosition - pickupCenter;
        if (droppedItem.pickupDelaySeconds <= 0.0f && glm::dot(delta, delta) <= kPickupRadiusSq
            && addBlockToInventory(hotbarSlots_, bagSlots_, droppedItem.blockType, selectedHotbarIndex_))
        {
            droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(itemIndex));
            continue;
        }

        ++itemIndex;
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

bool Application::startHostSession()
{
    stopMultiplayerSessions();

    auto hostSession = std::make_unique<multiplayer::HostSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!hostSession->start(multiplayerPort_))
    {
        multiplayerStatusLine_ = "host failed: " + hostSession->lastError();
        return false;
    }

    hostSession_ = std::move(hostSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Host;
    localClientId_ = 0;
    worldSyncSentClients_.clear();
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("hosting on :{}", multiplayerPort_);
    return true;
}

bool Application::startClientSession(const std::string& address)
{
    stopMultiplayerSessions();

    auto clientSession = std::make_unique<multiplayer::ClientSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!clientSession->connect(address, multiplayerPort_, "Player"))
    {
        multiplayerStatusLine_ = "join failed: " + clientSession->lastError();
        return false;
    }

    clientSession_ = std::move(clientSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Client;
    localClientId_ = 0;
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("connecting {}:{}", address, multiplayerPort_);
    return true;
}

void Application::stopMultiplayerSessions()
{
    if (clientSession_ != nullptr)
    {
        clientSession_->disconnect();
        clientSession_.reset();
    }
    if (hostSession_ != nullptr)
    {
        hostSession_->shutdown();
        hostSession_.reset();
    }
    multiplayerMode_ = MultiplayerRuntimeMode::SinglePlayer;
    localClientId_ = 0;
    remotePlayers_.clear();
    worldSyncSentClients_.clear();
}

void Application::handleMainMenuMultiplayerShortcuts()
{
    const bool hostHeld = inputState_.isKeyDown(SDL_SCANCODE_H);
    if (hostHeld && !hostShortcutLatch_)
    {
        if (startHostSession())
        {
            gameScreen_ = GameScreen::Playing;
            mouseCaptured_ = true;
            window_.setRelativeMouseMode(true);
            mainMenuNotice_ = "Hosting session started.";
        }
        else
        {
            mainMenuNotice_ = "Failed to start host session.";
        }
    }
    hostShortcutLatch_ = hostHeld;

    const bool joinHeld = inputState_.isKeyDown(SDL_SCANCODE_J);
    if (joinHeld && !joinShortcutLatch_)
    {
        multiplayerAddress_ = resolveJoinAddressFromEnvironment();
        if (startClientSession(multiplayerAddress_))
        {
            gameScreen_ = GameScreen::Playing;
            mouseCaptured_ = true;
            window_.setRelativeMouseMode(true);
            mainMenuNotice_ = "Joining remote host.";
        }
        else
        {
            mainMenuNotice_ = "Failed to start client session.";
        }
    }
    joinShortcutLatch_ = joinHeld;
}

void Application::sendInitialWorldToClient(const std::uint16_t clientId)
{
    if (hostSession_ == nullptr)
    {
        return;
    }

    for (const auto& [coord, chunk] : world_.chunks())
    {
        multiplayer::protocol::ChunkSnapshotMessage snapshot{
            .coord = coord,
        };
        const auto& blockStorage = chunk.blockStorage();
        for (std::size_t i = 0; i < blockStorage.size(); ++i)
        {
            snapshot.blocks[i] = static_cast<std::uint8_t>(blockStorage[i]);
        }
        hostSession_->sendChunkSnapshot(clientId, snapshot);
    }
}

void Application::applyChunkSnapshot(const multiplayer::protocol::ChunkSnapshotMessage& chunkMessage)
{
    world::World::ChunkMap chunks = world_.chunks();
    world::Chunk chunk(chunkMessage.coord);
    auto& storage = chunk.mutableBlockStorage();
    for (std::size_t i = 0; i < storage.size(); ++i)
    {
        storage[i] = static_cast<world::BlockType>(chunkMessage.blocks[i]);
    }
    chunks[chunkMessage.coord] = std::move(chunk);
    world_.replaceChunks(std::move(chunks));
}

void Application::applyRemoteBlockEdit(const multiplayer::protocol::BlockEditEventMessage& editMessage)
{
    static_cast<void>(world_.applyEditCommand({
        .action = editMessage.action,
        .position = {editMessage.x, editMessage.y, editMessage.z},
        .blockType = editMessage.blockType,
    }));
}

std::vector<multiplayer::protocol::PlayerSnapshotMessage> Application::buildServerSnapshots() const
{
    std::vector<multiplayer::protocol::PlayerSnapshotMessage> snapshots;
    snapshots.reserve(remotePlayers_.size() + 1);
    snapshots.push_back(multiplayer::protocol::PlayerSnapshotMessage{
        .clientId = localClientId_,
        .posX = playerFeetPosition_.x,
        .posY = playerFeetPosition_.y,
        .posZ = playerFeetPosition_.z,
        .yawDegrees = camera_.yawDegrees(),
        .pitchDegrees = camera_.pitchDegrees(),
        .health = playerVitals_.health(),
        .air = playerVitals_.air(),
    });
    for (const RemotePlayerState& remote : remotePlayers_)
    {
        snapshots.push_back(multiplayer::protocol::PlayerSnapshotMessage{
            .clientId = remote.clientId,
            .posX = remote.position.x,
            .posY = remote.position.y,
            .posZ = remote.position.z,
            .yawDegrees = remote.yawDegrees,
            .pitchDegrees = remote.pitchDegrees,
            .health = remote.health,
            .air = remote.air,
        });
    }
    return snapshots;
}

void Application::updateMultiplayer(const float deltaTimeSeconds)
{
    networkTickAccumulatorSeconds_ += deltaTimeSeconds;

    if (hostSession_ != nullptr && hostSession_->running())
    {
        hostSession_->poll();
        for (const multiplayer::ConnectedClient& client : hostSession_->clients())
        {
            if (!worldSyncSentClients_.contains(client.clientId))
            {
                sendInitialWorldToClient(client.clientId);
                worldSyncSentClients_.insert(client.clientId);
            }
        }

        const std::vector<multiplayer::protocol::ClientInputMessage> inputs = hostSession_->takePendingInputs();
        for (const multiplayer::protocol::ClientInputMessage& input : inputs)
        {
            auto remotePlayerIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&input](const RemotePlayerState& state)
                {
                    return state.clientId == input.clientId;
                });
            if (remotePlayerIt == remotePlayers_.end())
            {
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = input.clientId,
                    .position = {input.positionX, input.positionY, input.positionZ},
                    .yawDegrees = input.yawDelta,
                    .pitchDegrees = input.pitchDelta,
                    .health = input.health,
                    .air = input.air,
                });
            }
            else
            {
                remotePlayerIt->position = {input.positionX, input.positionY, input.positionZ};
                remotePlayerIt->yawDegrees = input.yawDelta;
                remotePlayerIt->pitchDegrees = input.pitchDelta;
                remotePlayerIt->health = input.health;
                remotePlayerIt->air = input.air;
            }

            if (input.breakBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Remove,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = world::BlockType::Air,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
            else if (input.placeBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Place,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = input.placeBlockType,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
        }

        while (networkTickAccumulatorSeconds_ >= kNetworkTickSeconds)
        {
            networkTickAccumulatorSeconds_ -= kNetworkTickSeconds;
            ++networkServerTick_;
            hostSession_->broadcastSnapshot(
                networkServerTick_,
                dayNightCycle_.elapsedSeconds(),
                weatherSystem_.elapsedSeconds(),
                buildServerSnapshots());
        }

        multiplayerStatusLine_ =
            fmt::format("host {} client(s) @{}", hostSession_->clients().size(), multiplayerPort_);
    }

    if (clientSession_ != nullptr)
    {
        clientSession_->poll();
        if (const std::optional<multiplayer::protocol::JoinAcceptMessage> accepted = clientSession_->takeJoinAccept();
            accepted.has_value())
        {
            localClientId_ = accepted->clientId;
            dayNightCycle_.setElapsedSeconds(accepted->dayNightElapsedSeconds);
            weatherSystem_.setElapsedSeconds(accepted->weatherElapsedSeconds);
        }

        for (const multiplayer::protocol::ChunkSnapshotMessage& chunk : clientSession_->takeChunkSnapshots())
        {
            applyChunkSnapshot(chunk);
        }

        for (const multiplayer::protocol::BlockEditEventMessage& edit : clientSession_->takeBlockEdits())
        {
            applyRemoteBlockEdit(edit);
        }

        const std::vector<multiplayer::protocol::ServerSnapshotMessage> snapshots = clientSession_->takeSnapshots();
        if (!snapshots.empty())
        {
            const multiplayer::protocol::ServerSnapshotMessage& latest = snapshots.back();
            dayNightCycle_.setElapsedSeconds(latest.dayNightElapsedSeconds);
            weatherSystem_.setElapsedSeconds(latest.weatherElapsedSeconds);
            remotePlayers_.clear();
            for (const multiplayer::protocol::PlayerSnapshotMessage& player : latest.players)
            {
                if (player.clientId == localClientId_)
                {
                    continue;
                }
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = player.clientId,
                    .position = {player.posX, player.posY, player.posZ},
                    .yawDegrees = player.yawDegrees,
                    .pitchDegrees = player.pitchDegrees,
                    .health = player.health,
                    .air = player.air,
                });
            }
        }

        if (clientSession_->connected())
        {
            clientSession_->sendInput(
                {
                    .clientId = localClientId_,
                    .dtSeconds = deltaTimeSeconds,
                    .positionX = playerFeetPosition_.x,
                    .positionY = playerFeetPosition_.y,
                    .positionZ = playerFeetPosition_.z,
                    .yawDelta = camera_.yawDegrees(),
                    .pitchDelta = camera_.pitchDegrees(),
                    .health = playerVitals_.health(),
                    .air = playerVitals_.air(),
                },
                networkServerTick_);
            multiplayerStatusLine_ = fmt::format("client id {} -> {}:{}", localClientId_, multiplayerAddress_, multiplayerPort_);
        }
        else if (clientSession_->connecting())
        {
            multiplayerStatusLine_ = fmt::format("connecting {}:{}...", multiplayerAddress_, multiplayerPort_);
        }
        else if (!clientSession_->lastError().empty())
        {
            multiplayerStatusLine_ = "client error: " + clientSession_->lastError();
        }
    }
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
