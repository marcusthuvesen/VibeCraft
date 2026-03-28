#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>

#include <cmath>
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
};

struct InputTuning
{
    float moveSpeed = 12.0f;
    float mouseSensitivity = 0.09f;
    float reachDistance = 6.0f;
};

constexpr WindowSettings kWindowSettings{};
constexpr StreamingSettings kStreamingSettings{};
constexpr InputTuning kInputTuning{};

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
}

[[nodiscard]] std::uint32_t chunkColor(const world::ChunkCoord& coord)
{
    return ((coord.x + coord.z) & 1) == 0 ? 0xff81c784 : 0xff66bb6a;
}

[[nodiscard]] glm::vec3 chunkBoundsMin(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>(coord.x * world::Chunk::kSize),
        0.0f,
        static_cast<float>(coord.z * world::Chunk::kSize),
    };
}

[[nodiscard]] glm::vec3 chunkBoundsMax(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>((coord.x + 1) * world::Chunk::kSize),
        static_cast<float>(world::Chunk::kHeight),
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
}

bool Application::initialize()
{
    core::initializeLogger();

    if (!window_.create("VibeCraft", kWindowSettings.width, kWindowSettings.height))
    {
        return false;
    }

    if (!renderer_.initialize(window_.nativeWindowHandle(), window_.width(), window_.height()))
    {
        core::logError("Failed to initialize bgfx.");
        return false;
    }

    window_.setRelativeMouseMode(true);

    if (!world_.load(savePath_))
    {
        world_.generateRadius(terrainGenerator_, kStreamingSettings.bootstrapChunkRadius);
        world_.save(savePath_);
    }

    syncWorldData();
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
    }

    world_.save(savePath_);
    renderer_.shutdown();
    return 0;
}

void Application::update(const float deltaTimeSeconds)
{
    static_cast<void>(deltaTimeSeconds);

    if (inputState_.windowSizeChanged && window_.width() != 0 && window_.height() != 0)
    {
        renderer_.resize(window_.width(), window_.height());
    }

    syncWorldData();

    const auto raycastHit =
        world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    render::FrameDebugData frameDebugData;
    frameDebugData.chunkCount = static_cast<std::uint32_t>(world_.chunks().size());
    frameDebugData.dirtyChunkCount = static_cast<std::uint32_t>(world_.dirtyChunkCount());
    frameDebugData.totalFaces = world_.totalVisibleFaces();
    frameDebugData.residentChunkCount = static_cast<std::uint32_t>(residentChunkMeshIds_.size());
    frameDebugData.cameraPosition = camera_.position();
    frameDebugData.statusLine = fmt::format(
        "Mouse: {}  Save: {}",
        mouseCaptured_ ? "captured" : "released",
        savePath_.generic_string());

    if (raycastHit.has_value())
    {
        frameDebugData.hasTarget = true;
        frameDebugData.targetBlock = raycastHit->solidBlock;
    }

    renderer_.renderFrame(
        frameDebugData,
        render::CameraFrameData{
            .position = camera_.position(),
            .forward = camera_.forward(),
            .up = camera_.up(),
        });
}

void Application::processInput(const float deltaTimeSeconds)
{
    if (inputState_.releaseMouseRequested)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }

    if (inputState_.captureMouseRequested)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
    }

    if (!inputState_.windowFocused)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        return;
    }

    if (mouseCaptured_)
    {
        camera_.addYawPitch(
            inputState_.mouseDeltaX * kInputTuning.mouseSensitivity,
            -inputState_.mouseDeltaY * kInputTuning.mouseSensitivity);
    }

    glm::vec3 localMotion(0.0f);
    if (inputState_.isKeyDown(SDL_SCANCODE_W))
    {
        localMotion.z += kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_S))
    {
        localMotion.z -= kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_D))
    {
        localMotion.x += kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_A))
    {
        localMotion.x -= kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_SPACE))
    {
        localMotion.y += kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_LSHIFT))
    {
        localMotion.y -= kInputTuning.moveSpeed * deltaTimeSeconds;
    }
    camera_.moveLocal(localMotion);

    const auto raycastHit =
        world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    if (!raycastHit.has_value())
    {
        return;
    }

    if (inputState_.leftMousePressed)
    {
        world_.applyEditCommand(world::WorldEditCommand{
            .action = world::WorldEditAction::Remove,
            .position = raycastHit->solidBlock,
            .blockType = world::BlockType::Air,
        });
    }

    if (inputState_.rightMousePressed)
    {
        world_.applyEditCommand(world::WorldEditCommand{
            .action = world::WorldEditAction::Place,
            .position = raycastHit->buildTarget,
            .blockType = world::BlockType::Dirt,
        });
    }
}

void Application::syncWorldData()
{
    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());

    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));

    std::unordered_set<std::uint64_t> desiredResidentIds;
    std::vector<render::SceneMeshData> sceneMeshes;
    std::vector<std::uint64_t> removedMeshIds;

    sceneMeshes.reserve(dirtyCoords.size());
    removedMeshIds.reserve(dirtyCoords.size() + residentChunkMeshIds_.size());

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
            desiredResidentIds.insert(meshId);

            const auto chunkIt = world_.chunks().find(coord);
            if (chunkIt == world_.chunks().end())
            {
                continue;
            }

            const bool isResident = residentChunkMeshIds_.contains(meshId);
            const bool isDirty = dirtyCoordSet.contains(coord);
            if (isResident && !isDirty)
            {
                continue;
            }

            meshing::ChunkMeshData meshData = chunkMesher_.buildMesh(world_, coord);
            if (meshData.vertices.empty() || meshData.indices.empty())
            {
                removedMeshIds.push_back(meshId);
                continue;
            }

            render::SceneMeshData sceneMesh;
            sceneMesh.id = meshId;
            sceneMesh.boundsMin = chunkBoundsMin(coord);
            sceneMesh.boundsMax = chunkBoundsMax(coord);
            sceneMesh.indices = std::move(meshData.indices);
            sceneMesh.vertices.reserve(meshData.vertices.size());

            const std::uint32_t color = chunkColor(coord);

            for (const meshing::DebugVertex& vertex : meshData.vertices)
            {
                sceneMesh.vertices.push_back(render::SceneMeshData::Vertex{
                    .position = {vertex.x, vertex.y, vertex.z},
                    .abgr = color,
                });
            }

            buildVertexNormals(sceneMesh);
            sceneMeshes.push_back(std::move(sceneMesh));
        }
    }

    const std::vector<std::uint64_t> residentIdsSnapshot(
        residentChunkMeshIds_.begin(),
        residentChunkMeshIds_.end());
    for (const std::uint64_t residentId : residentIdsSnapshot)
    {
        if (!desiredResidentIds.contains(residentId))
        {
            removedMeshIds.push_back(residentId);
        }
    }

    if (!sceneMeshes.empty() || !removedMeshIds.empty())
    {
        renderer_.updateSceneMeshes(sceneMeshes, removedMeshIds);
    }

    for (const std::uint64_t removedId : removedMeshIds)
    {
        residentChunkMeshIds_.erase(removedId);
    }

    for (const render::SceneMeshData& sceneMesh : sceneMeshes)
    {
        residentChunkMeshIds_.insert(sceneMesh.id);
    }

    if (world_.dirtyChunkCount() > 0)
    {
        world_.rebuildDirtyMeshes(chunkMesher_);
    }
}
}  // namespace vibecraft::app
