#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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
    glm::vec3 terrainHazeColor{0.67f, 0.80f, 0.95f};
    glm::vec3 terrainBounceTint{0.92f, 1.0f, 0.94f};
    glm::vec2 weatherWindDirectionXZ{1.0f, 0.0f};
    float sunVisibility = 1.0f;
    float moonVisibility = 0.0f;
    float cloudCoverage = 0.15f;
    float rainIntensity = 0.0f;
    float weatherTimeSeconds = 0.0f;
    float weatherWindSpeed = 2.0f;
    float terrainHazeStrength = 0.28f;
    float terrainSaturation = 1.12f;
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
    DiamondSword = 1,
    Stick = 2,
    RottenFlesh = 3,
    Leather = 4,
    RawPorkchop = 5,
    Mutton = 6,
    Feather = 7,
    WoodSword = 8,
    StoneSword = 9,
    IronSword = 10,
    GoldSword = 11,
    WoodPickaxe = 12,
    StonePickaxe = 13,
    IronPickaxe = 14,
    GoldPickaxe = 15,
    DiamondPickaxe = 16,
    WoodAxe = 17,
    StoneAxe = 18,
    IronAxe = 19,
    GoldAxe = 20,
    DiamondAxe = 21,
    Coal = 25,
    Charcoal = 26,
    ScoutHelmet = 27,
    ScoutChestRig = 28,
    ScoutGreaves = 29,
    ScoutBoots = 30,
    IronIngot = 31,
    GoldIngot = 32,
};

enum class CraftingUiMode : std::uint8_t
{
    Inventory = 0,
    Workbench,
    Chest,
    Furnace,
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
    std::string debugCoordinatesLine;
    std::string debugBlockLine;
    std::string debugChunkLine;
    std::string debugFacingLine;
    std::string debugBiomeLine;
    float uiCursorX = 0.0f;
    float uiCursorY = 0.0f;
    /// Menu/pause overlays use logical-window-normalized debug-text dimensions for stable size across displays.
    std::uint32_t uiMenuWindowWidth = 0;
    std::uint32_t uiMenuWindowHeight = 0;
    std::uint16_t uiMenuTextWidth = 0;
    std::uint16_t uiMenuTextHeight = 0;
    float health = 20.0f;
    float maxHealth = 20.0f;
    /// Underwater breath (PlayerVitals::air).
    float air = 10.0f;
    float maxAir = 10.0f;
    std::string selectedHotbarLabel;
    std::string selectedHotbarActionHint;
    /// Rotating early-session survival tips (empty when disabled or expired).
    std::string survivalTipLine;
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
    std::array<HotbarSlotHud, 4> equipmentSlots{};
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
        float pitchRadians = 0.0f;
        float halfWidth = 0.28f;
        float height = 1.75f;
        vibecraft::game::MobKind mobKind = vibecraft::game::MobKind::Zombie;
        vibecraft::world::BlockType heldBlockType = vibecraft::world::BlockType::Air;
        HudItemKind heldItemKind = HudItemKind::None;
        bool heldItemUsesSwordPose = false;
        /// Mob HP (0 / 0 = hide bar). `MobKind::Player` entries ignore this.
        float mobHealthCurrent = 0.0f;
        float mobHealthMax = 0.0f;
    };
    std::vector<WorldMobHud> worldMobs;

    struct WorldBirdHud
    {
        glm::vec3 worldPosition{0.0f};
        float halfWidth = 1.0f;
        float halfHeight = 0.45f;
        glm::vec3 tint{1.0f};
        float alpha = 1.0f;
        float flapPhase = 0.0f;
        /// Horizontal flight direction in the XZ plane (x, z), normalized when set by ambient life.
        glm::vec2 flightForwardXZ{1.0f, 0.0f};
        /// Extra body roll (radians) into path turns — combined with wing flap roll in the renderer.
        float bankAngle = 0.0f;
    };
    std::vector<WorldBirdHud> worldBirds;

    /// When true, the 3D view and in-game HUD are hidden and the title menu is drawn instead.
    bool mainMenuActive = false;
    /// Index of the menu control under the cursor, or -1 (ids match hitTestMainMenu).
    int mainMenuHoveredButton = -1;
    float mainMenuTimeSeconds = 0.0f;
    std::string mainMenuNotice;
    bool mainMenuCreativeModeEnabled = false;
    std::string mainMenuSpawnPresetLabel;
    std::string mainMenuSpawnBiomeLabel;
    std::string mainMenuSelectedWorldLabel;
    bool mainMenuLoadingActive = false;
    float mainMenuLoadingProgress = 0.0f;
    std::string mainMenuLoadingTitle;
    std::string mainMenuLoadingLabel;
    bool mainMenuSingleplayerPanelActive = false;
    int mainMenuSingleplayerHoveredControl = -1;
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
    /// Quick-join preset button labels (max 3); same order as `join_presets.txt` / host config.
    std::vector<std::string> mainMenuJoinPresetButtonLabels;

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
    std::string pauseSpawnBiomeLabel;
    std::string pauseWeatherLabel;
    bool craftingMenuActive = false;
    bool craftingUsesWorkbench = false;
    CraftingUiMode craftingUiMode = CraftingUiMode::Inventory;
    std::string craftingTitle;
    std::string craftingHint;
    float craftingFuelFraction = 0.0f;
    float craftingProgressFraction = 0.0f;

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

    [[nodiscard]] std::uint16_t menuLogoWidthPx() const { return logoWidthPx_; }
    [[nodiscard]] std::uint16_t menuLogoHeightPx() const { return logoHeightPx_; }

    /// Returns a button id for the main menu hit test, or -1. Layout must match drawMainMenuOverlay().
    /// 0..4 main buttons, 5 creative shortcut, 6 spawn shortcut.
    [[nodiscard]] static int hitTestMainMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        std::uint16_t menuLogoWidthPx = 0,
        std::uint16_t menuLogoHeightPx = 0);

    /// Singleplayer panel: 0 Start saved, 1 Start new, 2 Cycle biome target, 3 Back.
    [[nodiscard]] static int hitTestMainMenuSingleplayerPanel(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Pause menu: 0 Resume, 1 Sound settings, 2 Game options, 3 Quit to title, 4 Quit game.
    [[nodiscard]] static int hitTestPauseMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight);

    /// Pause game options: 0 Back, 1 Mob spawning toggle, 2 Spawn biome cycle, 3 Travel now, 4 Weather cycle.
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
    /// Pause sound menu slider hit test: returns normalized volume [0..1] when mouse is over slider fill area.
    [[nodiscard]] static std::optional<float> pauseSoundSliderValueFromMouse(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        bool musicSlider);

    /// Same vertical centering as `drawMainMenuMultiplayerOverlay` (dbg-text row shift).
    [[nodiscard]] static int multiplayerMenuRowShift(
        std::uint16_t textHeight,
        FrameDebugData::MainMenuMultiplayerPanel panel,
        int joinPresetSlotCount,
        int mainMenuContentTopBias);

    /// Multiplayer hub: 0 Host, 1 Join, 2 Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerHub(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        int multiplayerRowShift,
        int mainMenuContentTopBias);

    /// Host screen: 0 Start hosting, 1 Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerHost(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        int multiplayerRowShift,
        int mainMenuContentTopBias);

    /// Join screen: 0..presetCount-1 quick join, then address, port, Connect, Back.
    [[nodiscard]] static int hitTestMainMenuMultiplayerJoin(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        int joinPresetSlotCount,
        int multiplayerRowShift,
        int mainMenuContentTopBias);

    /// Crafting screen hit ids: 0..8 grid, 9 result, 10..14 equipment, 15..23 hotbar, 24..104 bag.
    static constexpr int kCraftingGridHitBase = 0;
    static constexpr int kCraftingResultHit = 9;
    static constexpr int kCraftingEquipmentHitBase = 10;
    static constexpr int kCraftingHotbarHitBase = 15;
    static constexpr int kCraftingBagHitBase = 24;

    [[nodiscard]] static int hitTestCraftingMenu(
        float mouseX,
        float mouseY,
        std::uint32_t windowWidth,
        std::uint32_t windowHeight,
        CraftingUiMode mode,
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
    void drawMainMenuChrome(
        const FrameDebugData& frameDebugData,
        std::uint16_t textWidth,
        std::uint16_t textHeight,
        int mainMenuTitleContentRowOffset);
    void drawMainMenuLogo();
    void drawPauseMenuChrome(
        const FrameDebugData& frameDebugData,
        std::uint16_t textWidth,
        std::uint16_t textHeight);
    void drawCrosshairOverlay();
    void drawHeldItemOverlay(const FrameDebugData& frameDebugData);
    void drawBlockBreakingOverlay(const FrameDebugData& frameDebugData);
    void drawCraftingOverlay(const FrameDebugData& frameDebugData);
    void drawSurvivalStatusHud(const FrameDebugData& frameDebugData);
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
    void drawTextureIcon(
        float centerX,
        float centerY,
        float iconSizePx,
        std::uint16_t textureHandle,
        const TextureUvRect& uvRect = {});
    void drawWorldPickupSprites(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);
    void drawWorldBirdSprites(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);
    void drawWorldMobSprites(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);
    [[nodiscard]] std::uint16_t mobTextureHandleForKind(vibecraft::game::MobKind kind) const;
    [[nodiscard]] TextureUvRect mobTextureUvForKind(vibecraft::game::MobKind kind) const;
    [[nodiscard]] std::uint16_t hudItemKindTextureHandle(HudItemKind kind) const;
    [[nodiscard]] TextureUvRect hudItemKindTextureUv(HudItemKind kind) const;

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
    std::uint16_t chunkAnimUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkBiomeHazeUniformHandle_ = UINT16_MAX;
    std::uint16_t chunkBiomeGradeUniformHandle_ = UINT16_MAX;
    std::uint16_t logoProgramHandle_ = UINT16_MAX;
    std::uint16_t logoTextureHandle_ = UINT16_MAX;
    std::uint16_t logoSamplerHandle_ = UINT16_MAX;
    std::uint16_t logoWidthPx_ = 0;
    std::uint16_t logoHeightPx_ = 0;
    std::uint16_t mainMenuBackgroundTextureHandle_ = UINT16_MAX;
    std::uint16_t mainMenuBackgroundWidthPx_ = 0;
    std::uint16_t mainMenuBackgroundHeightPx_ = 0;
    std::uint16_t skySunTextureHandle_ = UINT16_MAX;
    std::uint16_t skyMoonPhasesTextureHandle_ = UINT16_MAX;
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
    std::uint16_t coalTextureHandle_ = UINT16_MAX;
    std::uint16_t charcoalTextureHandle_ = UINT16_MAX;
    std::uint16_t ironIngotTextureHandle_ = UINT16_MAX;
    std::uint16_t goldIngotTextureHandle_ = UINT16_MAX;
    std::uint16_t scoutHelmetTextureHandle_ = UINT16_MAX;
    std::uint16_t scoutChestRigTextureHandle_ = UINT16_MAX;
    std::uint16_t scoutGreavesTextureHandle_ = UINT16_MAX;
    std::uint16_t scoutBootsTextureHandle_ = UINT16_MAX;
    /// Optional textures for WoodSword..DiamondAxe (see HudItemKind); falls back in hudItemKindTextureHandle.
    std::array<std::uint16_t, 14> extendedToolTextureHandles_{};
    std::array<std::uint16_t, 10> blockBreakStageTextureHandles_{};
    std::uint16_t fullHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t halfHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t emptyHeartTextureHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSolidProgramHandle_ = UINT16_MAX;
    std::uint16_t inventoryUiSamplerHandle_ = UINT16_MAX;
    std::uint16_t zombieTextureHandle_ = UINT16_MAX;
    std::uint16_t playerMobTextureHandle_ = UINT16_MAX;
    std::uint16_t sporegrazerTextureHandle_ = UINT16_MAX;
    std::uint16_t burrowerTextureHandle_ = UINT16_MAX;
    std::uint16_t shardbackTextureHandle_ = UINT16_MAX;
    std::uint16_t skitterwingTextureHandle_ = UINT16_MAX;
    std::uint16_t ambientBirdTextureHandle_ = UINT16_MAX;
    TextureUvRect zombieTextureUv_{};
    TextureUvRect playerMobTextureUv_{};
    TextureUvRect sporegrazerTextureUv_{};
    TextureUvRect burrowerTextureUv_{};
    TextureUvRect shardbackTextureUv_{};
    TextureUvRect skitterwingTextureUv_{};
    TextureUvRect ambientBirdTextureUv_{};
    std::unordered_map<std::uint64_t, SceneGpuMesh> sceneMeshes_;
};
}  // namespace vibecraft::render
