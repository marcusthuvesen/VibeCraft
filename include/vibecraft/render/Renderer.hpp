#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/game/MobTypes.hpp"
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

struct TextureUvRect
{
    float minU = 0.0f;
    float maxU = 1.0f;
    float minV = 0.0f;
    float maxV = 1.0f;
};

enum class HudItemKind : std::uint8_t
{
    None = 0,
    DiamondSword,
    Stick,
    RottenFlesh,
    Leather,
    RawPorkchop,
    Mutton,
    Feather,
    WoodSword,
    StoneSword,
    IronSword,
    GoldSword,
    WoodPickaxe,
    StonePickaxe,
    IronPickaxe,
    GoldPickaxe,
    DiamondPickaxe,
    WoodAxe,
    StoneAxe,
    IronAxe,
    GoldAxe,
    DiamondAxe,
};

struct FrameDebugData
{
    struct HotbarSlotHud
    {
        vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
        std::uint32_t count = 0;
        HudItemKind itemKind = HudItemKind::None;
        /// True when the held item should use the first-person sword pose (all sword tiers).
        bool heldItemUsesSwordPose = false;
    };

    static constexpr std::size_t kBagHudSlotCount = 81;

    std::uint32_t chunkCount = 0;
    std::uint32_t dirtyChunkCount = 0;
    std::uint32_t totalFaces = 0;
    std::uint32_t residentChunkCount = 0;
    glm::vec3 cameraPosition{0.0f};
    float uiCursorX = 0.0f;
    float uiCursorY = 0.0f;
    float health = 20.0f;
    float maxHealth = 20.0f;
    /// 0–1 progress toward the next level (visual XP bar only until gameplay tracks XP).
    float experienceFill = 0.0f;
    bool hasTarget = false;
    glm::ivec3 targetBlock{0, 0, 0};
    bool miningTargetActive = false;
    float miningTargetProgress = 0.0f;
    std::string statusLine;
    std::array<HotbarSlotHud, 9> hotbarSlots{};
    std::array<HotbarSlotHud, kBagHudSlotCount> bagSlots{};
    std::array<HotbarSlotHud, 9> craftingGridSlots{};
    HotbarSlotHud craftingResultSlot{};
    HotbarSlotHud craftingCursorSlot{};
    std::uint8_t craftingBagStartRow = 0;
    std::size_t hotbarSelectedIndex = 0;
    /// 0..1 short equip swing impulse, set by gameplay on primary attack.
    float heldItemSwing = 0.0f;
    struct WorldPickupHud
    {
        vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
        HudItemKind itemKind = HudItemKind::None;
        glm::vec3 worldPosition{0.0f};
        float spinRadians = 0.0f;
    };
    std::vector<WorldPickupHud> worldPickups;

    struct WorldMobHud
    {
        glm::vec3 feetPosition{0.0f};
        float yawRadians = 0.0f;
        float halfWidth = 0.28f;
        float height = 1.75f;
        vibecraft::game::MobKind mobKind = vibecraft::game::MobKind::HostileStalker;
    };
    std::vector<WorldMobHud> worldMobs;

    /// When true, the 3D view and in-game HUD are hidden and the title menu is drawn instead.
    bool mainMenuActive = false;
    /// Index of the menu control under the cursor, or -1 (ids match hitTestMainMenu).
    int mainMenuHoveredButton = -1;
    float mainMenuTimeSeconds = 0.0f;
    std::string mainMenuNotice;
    bool mainMenuLoadingActive = false;
    float mainMenuLoadingProgress = 0.0f;
    std::string mainMenuLoadingLabel;
    bool mainMenuSoundSettingsActive = false;
    int mainMenuSoundSettingsHoveredControl = -1;
    float mainMenuSoundMusicVolume = 0.85f;
    float mainMenuSoundSfxVolume = 1.0f;

    enum class MainMenuMultiplayerPanel : std::uint8_t
    {
        None = 0,
        Hub = 1,
        Host = 2,
        Join = 3,
    };
    MainMenuMultiplayerPanel mainMenuMultiplayerPanel = MainMenuMultiplayerPanel::None;
    /// Primary LAN IPv4 for "tell your friend" (may be empty).
    std::string mainMenuMultiplayerLanAddress;
    std::string mainMenuJoinAddressField;
    std::string mainMenuJoinPortField;
    std::uint16_t mainMenuMultiplayerPortDisplay = 41234;
    int mainMenuMultiplayerHoveredControl = -1;
    /// 0 = host address field, 1 = port field (Join screen).
    int mainMenuJoinFocusedField = 0;

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
    bool craftingMenuActive = false;
    bool craftingUsesWorkbench = false;
    std::string craftingTitle;
    std::string craftingHint;

    /// World-origin grid + axis in the 3D view (heavy debug draw). Off by default; toggle with F3 in-game.
    bool showWorldOriginGuides = false;
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

    /// Multiplayer hub: 0 Host, 1 Join, 2 Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerHub(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Host screen: 0 Start hosting, 1 Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerHost(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Join screen: 0 address field, 1 port field, 2 Connect, 3 Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerJoin(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Crafting screen hit ids: 0..8 grid, 9 result, 10..18 hotbar, 19..99 bag.
    static constexpr int kCraftingGridHitBase = 0;
    static constexpr int kCraftingResultHit = 9;
    static constexpr int kCraftingHotbarHitBase = 10;
    static constexpr int kCraftingBagHitBase = 19;

    [[nodiscard]] static int hitTestCraftingMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        bool useWorkbench,
        std::size_t bagStartRow);

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
    void drawMainMenuBackground();
    void drawMainMenuLogo();
    void drawCrosshairOverlay();
    void drawHeldItemOverlay(const FrameDebugData& frameDebugData);
    void drawBlockBreakingOverlay(const FrameDebugData& frameDebugData);
    void drawCraftingOverlay(const FrameDebugData& frameDebugData);
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
    void drawTextureIcon(float centerX, float centerY, float iconSizePx, std::uint16_t textureHandle);
    void drawWorldPickupSprites(const FrameDebugData& frameDebugData);
    void drawWorldMobSprites(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);
    [[nodiscard]] std::uint16_t mobTextureHandleForKind(vibecraft::game::MobKind kind) const;
    [[nodiscard]] TextureUvRect mobTextureUvForKind(vibecraft::game::MobKind kind) const;
    [[nodiscard]] std::uint16_t hudItemKindTextureHandle(HudItemKind kind) const;

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
    std::uint16_t mainMenuBackgroundTextureHandle_ = UINT16_MAX;
    std::uint16_t mainMenuBackgroundWidthPx_ = 0;
    std::uint16_t mainMenuBackgroundHeightPx_ = 0;
    std::uint16_t crosshairProgramHandle_ = UINT16_MAX;
    std::uint16_t crosshairTextureHandle_ = UINT16_MAX;
    std::uint16_t crosshairSamplerHandle_ = UINT16_MAX;
    std::uint16_t diamondSwordTextureHandle_ = UINT16_MAX;
    std::uint16_t stickTextureHandle_ = UINT16_MAX;
    std::uint16_t rottenFleshTextureHandle_ = UINT16_MAX;
    std::uint16_t leatherTextureHandle_ = UINT16_MAX;
    std::uint16_t rawPorkchopTextureHandle_ = UINT16_MAX;
    std::uint16_t muttonTextureHandle_ = UINT16_MAX;
    std::uint16_t featherTextureHandle_ = UINT16_MAX;
    /// Optional textures for WoodSword..DiamondAxe (see HudItemKind); falls back in hudItemKindTextureHandle.
    std::array<std::uint16_t, 14> extendedToolTextureHandles_{};
    std::array<std::uint16_t, 10> blockBreakStageTextureHandles_{};
    std::uint16_t fullHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t halfHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t emptyHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSolidProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSamplerHandle_ = UINT16_MAX;
    std::uint16_t hostileMobTextureHandle_ = UINT16_MAX;
    std::uint16_t cowMobTextureHandle_ = UINT16_MAX;
    std::uint16_t pigMobTextureHandle_ = UINT16_MAX;
    std::uint16_t sheepMobTextureHandle_ = UINT16_MAX;
    std::uint16_t chickenMobTextureHandle_ = UINT16_MAX;
    TextureUvRect hostileMobTextureUv_{};
    TextureUvRect cowMobTextureUv_{};
    TextureUvRect pigMobTextureUv_{};
    TextureUvRect sheepMobTextureUv_{};
    TextureUvRect chickenMobTextureUv_{};
    std::unordered_map<std::uint64_t, SceneGpuMesh> sceneMeshes_;
};
}  // namespace vibecraft::render
