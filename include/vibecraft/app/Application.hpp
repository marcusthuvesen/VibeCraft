#pragma once

#include <glm/vec3.hpp>

#include <filesystem>
#include <string>
#include <unordered_set>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/Camera.hpp"
#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/WeatherSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/platform/InputState.hpp"
#include "vibecraft/platform/Window.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
enum class GameScreen
{
    MainMenu,
    Playing,
    Paused,
};

class Application
{
  public:
    bool initialize();
    int run();

  private:
    void update(float deltaTimeSeconds);
    void processInput(float deltaTimeSeconds);
    void syncWorldData();
    void respawnPlayer();

    vibecraft::platform::Window window_;
    vibecraft::platform::InputState inputState_;
    vibecraft::render::Renderer renderer_;
    vibecraft::game::Camera camera_;
    vibecraft::game::DayNightCycle dayNightCycle_;
    vibecraft::game::WeatherSystem weatherSystem_;
    vibecraft::world::TerrainGenerator terrainGenerator_;
    vibecraft::world::World world_;
    vibecraft::meshing::ChunkMesher chunkMesher_;
    std::filesystem::path savePath_ = "assets/saves/dev_world.bin";
    std::unordered_set<std::uint64_t> residentChunkMeshIds_;
    bool mouseCaptured_ = true;
    glm::vec3 playerFeetPosition_{0.0f};
    glm::vec3 spawnFeetPosition_{0.0f};
    float verticalVelocity_ = 0.0f;
    float accumulatedFallDistance_ = 0.0f;
    bool isGrounded_ = false;
    bool jumpWasHeld_ = false;
    vibecraft::game::EnvironmentalHazards playerHazards_{};
    vibecraft::game::PlayerVitals playerVitals_{};
    HotbarSlots hotbarSlots_{};
    BagSlots bagSlots_{};
    std::size_t selectedHotbarIndex_ = 0;
    float smoothedFrameTimeMs_ = 0.0f;
    bool frameTimeInitialized_ = false;
    GameScreen gameScreen_ = GameScreen::Playing;
    float mainMenuTimeSeconds_ = 0.0f;
    std::string mainMenuNotice_;
    std::string pauseMenuNotice_;
    std::string respawnNotice_;
    /// Run-loop frame index (used to ignore spurious startup mouse-down on the main menu).
    std::uint32_t runFrameIndex_ = 0;
};
}  // namespace vibecraft::app
