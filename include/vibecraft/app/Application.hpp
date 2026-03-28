#pragma once

#include <filesystem>
#include <unordered_set>

#include "vibecraft/game/Camera.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/platform/InputState.hpp"
#include "vibecraft/platform/Window.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
class Application
{
  public:
    bool initialize();
    int run();

  private:
    void update(float deltaTimeSeconds);
    void processInput(float deltaTimeSeconds);
    void syncWorldData();

    vibecraft::platform::Window window_;
    vibecraft::platform::InputState inputState_;
    vibecraft::render::Renderer renderer_;
    vibecraft::game::Camera camera_;
    vibecraft::world::TerrainGenerator terrainGenerator_;
    vibecraft::world::World world_;
    vibecraft::meshing::ChunkMesher chunkMesher_;
    std::filesystem::path savePath_ = "assets/saves/dev_world.bin";
    std::unordered_set<std::uint64_t> residentChunkMeshIds_;
    bool mouseCaptured_ = true;
    float smoothedFrameTimeMs_ = 0.0f;
    bool frameTimeInitialized_ = false;
};
}  // namespace vibecraft::app
