#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::render
{
struct CameraFrameData
{
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 skyTint{0.54f, 0.76f, 0.98f};
    glm::vec3 horizonTint{0.76f, 0.88f, 1.0f};
    glm::vec3 sunDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 moonDirection{-1.0f, 0.0f, 0.0f};
    glm::vec3 sunLightTint{1.0f, 0.97f, 0.92f};
    glm::vec3 moonLightTint{0.62f, 0.72f, 1.0f};
    glm::vec3 cloudTint{0.96f, 0.97f, 1.0f};
    glm::vec2 weatherWindDirectionXZ{1.0f, 0.0f};
    float sunVisibility = 1.0f;
    float moonVisibility = 0.0f;
    float cloudCoverage = 0.15f;
    float rainIntensity = 0.0f;
    float weatherTimeSeconds = 0.0f;
    float weatherWindSpeed = 2.0f;
    float verticalFovDegrees = 60.0f;
    float nearClip = 0.1f;
    float farClip = 1024.0f;
};

struct SceneMeshData
{
    std::uint64_t id = 0;
    struct Vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 uv{0.0f};
        std::uint32_t abgr = 0xffffffff;
    };

    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct FrameDebugData
{
    struct HotbarSlotHud
    {
        vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
        std::uint32_t count = 0;
    };

    static constexpr std::size_t kBagHudSlotCount = 27;

    std::uint32_t chunkCount = 0;
    std::uint32_t dirtyChunkCount = 0;
    std::uint32_t totalFaces = 0;
    std::uint32_t residentChunkCount = 0;
    glm::vec3 cameraPosition{0.0f};
    float health = 20.0f;
    float maxHealth = 20.0f;
    bool hasTarget = false;
    glm::ivec3 targetBlock{0, 0, 0};
    std::string statusLine;
    std::array<HotbarSlotHud, 9> hotbarSlots{};
    std::array<HotbarSlotHud, kBagHudSlotCount> bagSlots{};
    std::size_t hotbarSelectedIndex = 0;

    /// When true, the 3D view and in-game HUD are hidden and the title menu is drawn instead.
    bool mainMenuActive = false;
    /// Index of the menu control under the cursor, or -1 (ids match hitTestMainMenu).
    int mainMenuHoveredButton = -1;
    float mainMenuTimeSeconds = 0.0f;
    std::string mainMenuNotice;

    /// In-game pause overlay (world still rendered underneath).
    bool pauseMenuActive = false;
    int pauseMenuHoveredButton = -1;
    std::string pauseMenuNotice;
};

class Renderer
{
  public:
    bool initialize(void* nativeWindowHandle, std::uint32_t width, std::uint32_t height);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);
    void updateSceneMeshes(
        const std::vector<SceneMeshData>& sceneMeshes,
        const std::vector<std::uint64_t>& removedMeshIds);
    void renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);

    /// Returns a button id for the main menu hit test, or -1. Layout must match drawMainMenuOverlay().
    [[nodiscard]] static int hitTestMainMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Pause menu buttons: 0 Resume, 1 Options, 2 Quit to title, 3 Quit game. Layout matches drawPauseMenuOverlay().
    [[nodiscard]] static int hitTestPauseMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

  private:
    struct SceneGpuMesh
    {
        std::uint16_t vertexBufferHandle = UINT16_MAX;
        std::uint16_t indexBufferHandle = UINT16_MAX;
        std::uint32_t indexCount = 0;
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
    };

    void destroySceneMesh(std::uint64_t sceneMeshId);
    void destroySceneMeshes();
    void drawMainMenuLogo();

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool initialized_ = false;
    std::uint16_t chunkProgramHandle_ = UINT16_MAX;
    std::uint16_t chunkAtlasTextureHandle_ = UINT16_MAX;
    std::uint16_t chunkAtlasSamplerHandle_ = UINT16_MAX;
    std::uint16_t chunkSunDirectionUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkSunLightColorUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkMoonDirectionUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkMoonLightColorUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkAmbientLightUniformHandle_ = UINT16_MAX;
    std::uint16_t logoProgramHandle_ = UINT16_MAX;
    std::uint16_t logoTextureHandle_ = UINT16_MAX;
    std::uint16_t logoSamplerHandle_ = UINT16_MAX;
    std::uint16_t logoWidthPx_ = 0;
    std::uint16_t logoHeightPx_ = 0;
    std::unordered_map<std::uint64_t, SceneGpuMesh> sceneMeshes_;
};
}  // namespace vibecraft::render
