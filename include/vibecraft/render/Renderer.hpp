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
    /// 0–1 progress toward the next level (visual XP bar only until gameplay tracks XP).
    float experienceFill = 0.0f;
    bool hasTarget = false;
    glm::ivec3 targetBlock{0, 0, 0};
    std::string statusLine;
    std::array<HotbarSlotHud, 9> hotbarSlots{};
    std::array<HotbarSlotHud, kBagHudSlotCount> bagSlots{};
    std::size_t hotbarSelectedIndex = 0;
    struct WorldPickupHud
    {
        vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
        glm::vec3 worldPosition{0.0f};
        float spinRadians = 0.0f;
    };
    std::vector<WorldPickupHud> worldPickups;

    struct WorldMobHud
    {
        glm::vec3 feetPosition{0.0f};
        float halfWidth = 0.28f;
        float height = 1.75f;
    };
    std::vector<WorldMobHud> worldMobs;

    /// When true, the 3D view and in-game HUD are hidden and the title menu is drawn instead.
    bool mainMenuActive = false;
    /// Index of the menu control under the cursor, or -1 (ids match hitTestMainMenu).
    int mainMenuHoveredButton = -1;
    float mainMenuTimeSeconds = 0.0f;
    std::string mainMenuNotice;
    bool mainMenuSoundSettingsActive = false;
    int mainMenuSoundSettingsHoveredControl = -1;
    float mainMenuSoundMusicVolume = 0.85f;
    float mainMenuSoundSfxVolume = 1.0f;

    /// In-game pause overlay (world still rendered underneath).
    bool pauseMenuActive = false;
    int pauseMenuHoveredButton = -1;
    std::string pauseMenuNotice;
    bool pauseSoundSettingsActive = false;
    int pauseSoundSettingsHoveredControl = -1;
    float pauseSoundMusicVolume = 0.34f;
    float pauseSoundSfxVolume = 1.0f;
    bool pauseGameSettingsActive = false;
    int pauseGameSettingsHoveredControl = -1;
    bool mobSpawningEnabled = true;
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

    /// Pause menu: 0 Resume, 1 Sound settings, 2 Quit to title, 3 Quit game, 4 Game options.
    [[nodiscard]] static int hitTestPauseMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Pause game options: 0 Back, 1 Mob spawning toggle.
    [[nodiscard]] static int hitTestPauseGameSettingsMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Sound settings panel (pause or title menu): 0 Back, 1 Music-, 2 Music+, 3 SFX-, 4 SFX+.
    [[nodiscard]] static int hitTestPauseSoundMenu(
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
    void drawCrosshairOverlay();
    void drawUiSolidRect(float x0, float y0, float x1, float y1, std::uint32_t abgr);
    void drawInventoryItemIcons(
        const FrameDebugData& frameDebugData,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        std::uint16_t hotbarRow,
        std::uint16_t bagRow0,
        std::uint16_t bagRow1,
        std::uint16_t bagRow2);
    void drawAtlasIcon(float centerX, float centerY, float iconSizePx, std::uint8_t tileIndex);
    void drawWorldPickupSprites(const FrameDebugData& frameDebugData);

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
    std::uint16_t crosshairProgramHandle_ = UINT16_MAX;
    std::uint16_t crosshairTextureHandle_ = UINT16_MAX;
    std::uint16_t crosshairSamplerHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSolidProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSamplerHandle_ = UINT16_MAX;
    std::unordered_map<std::uint64_t, SceneGpuMesh> sceneMeshes_;
};
}  // namespace vibecraft::render
