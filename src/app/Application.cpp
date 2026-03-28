#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <vector>

#include "vibecraft/core/Logger.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
constexpr std::uint32_t kWindowWidth = 1600;
constexpr std::uint32_t kWindowHeight = 900;
constexpr int kInitialChunkRadius = 2;
constexpr float kMoveSpeed = 12.0f;
constexpr float kMouseSensitivity = 0.09f;
constexpr float kReachDistance = 6.0f;

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
}

[[nodiscard]] std::uint32_t chunkColor(const world::ChunkCoord& coord)
{
    return ((coord.x + coord.z) & 1) == 0 ? 0xff81c784 : 0xff66bb6a;
}
}

bool Application::initialize()
{
    core::initializeLogger();

    if (!window_.create("VibeCraft", kWindowWidth, kWindowHeight))
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
        world_.generateRadius(terrainGenerator_, kInitialChunkRadius);
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

    const auto raycastHit = world_.raycast(camera_.position(), camera_.forward(), kReachDistance);
    render::FrameDebugData frameDebugData;
    frameDebugData.chunkCount = static_cast<std::uint32_t>(world_.chunks().size());
    frameDebugData.dirtyChunkCount = static_cast<std::uint32_t>(world_.dirtyChunkCount());
    frameDebugData.totalFaces = world_.totalVisibleFaces();
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
            inputState_.mouseDeltaX * kMouseSensitivity,
            -inputState_.mouseDeltaY * kMouseSensitivity);
    }

    glm::vec3 localMotion(0.0f);
    if (inputState_.isKeyDown(SDL_SCANCODE_W))
    {
        localMotion.z += kMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_S))
    {
        localMotion.z -= kMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_D))
    {
        localMotion.x += kMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_A))
    {
        localMotion.x -= kMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_SPACE))
    {
        localMotion.y += kMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_LSHIFT))
    {
        localMotion.y -= kMoveSpeed * deltaTimeSeconds;
    }
    camera_.moveLocal(localMotion);

    const auto raycastHit = world_.raycast(camera_.position(), camera_.forward(), kReachDistance);
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
    if (world_.dirtyChunkCount() > 0)
    {
        std::vector<render::SceneMeshData> sceneMeshes;
        sceneMeshes.reserve(world_.chunks().size());

        for (const auto& [coord, chunk] : world_.chunks())
        {
            static_cast<void>(chunk);

            meshing::ChunkMeshData meshData = chunkMesher_.buildMesh(world_, coord);
            if (meshData.vertices.empty() || meshData.indices.empty())
            {
                continue;
            }

            render::SceneMeshData sceneMesh;
            sceneMesh.id = chunkMeshId(coord);
            sceneMesh.indices = std::move(meshData.indices);
            sceneMesh.abgr = chunkColor(coord);
            sceneMesh.positions.reserve(meshData.vertices.size());

            for (const meshing::DebugVertex& vertex : meshData.vertices)
            {
                sceneMesh.positions.emplace_back(vertex.x, vertex.y, vertex.z);
            }

            sceneMeshes.push_back(std::move(sceneMesh));
        }

        renderer_.replaceSceneMeshes(sceneMeshes);
        world_.rebuildDirtyMeshes(chunkMesher_);
    }
}
}  // namespace vibecraft::app
