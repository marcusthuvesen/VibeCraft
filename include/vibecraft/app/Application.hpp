#pragma once

#include <glm/vec3.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/app/Crafting.hpp"
#include "vibecraft/audio/MusicDirector.hpp"
#include "vibecraft/audio/SharedAudioOutput.hpp"
#include "vibecraft/audio/SoundEffects.hpp"
#include "vibecraft/game/Camera.hpp"
#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/MobSpawnSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/game/WeatherSystem.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/multiplayer/Session.hpp"
#include "vibecraft/platform/InputState.hpp"
#include "vibecraft/platform/Window.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
using MainMenuMultiplayerPanel = render::FrameDebugData::MainMenuMultiplayerPanel;

enum class GameScreen
{
    MainMenu,
    Playing,
    Paused,
};

enum class MultiplayerRuntimeMode
{
    SinglePlayer,
    Host,
    Client,
};

class Application
{
  public:
    bool initialize();
    int run();

  private:
    struct FrameInputIntent
    {
        float moveX = 0.0f;
        float moveZ = 0.0f;
        float yawDelta = 0.0f;
        float pitchDelta = 0.0f;
        bool jumpHeld = false;
        bool breakBlock = false;
        bool placeBlock = false;
        bool sneaking = false;
        bool sprinting = false;
        std::optional<vibecraft::world::WorldEditCommand> pendingEdit;
    };

    struct RemotePlayerState
    {
        std::uint16_t clientId = 0;
        glm::vec3 position{0.0f};
        float yawDegrees = 0.0f;
        float pitchDegrees = 0.0f;
        float health = 20.0f;
        float air = 10.0f;
    };

    struct DroppedItem
    {
        vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
        EquippedItem equippedItem = EquippedItem::None;
        glm::vec3 worldPosition{0.0f};
        glm::vec3 velocity{0.0f};
        float ageSeconds = 0.0f;
        float pickupDelaySeconds = 0.25f;
        float spinRadians = 0.0f;
    };

    struct ActiveMiningState
    {
        bool active = false;
        glm::ivec3 targetBlockPosition{0};
        vibecraft::world::BlockType targetBlockType = vibecraft::world::BlockType::Air;
        vibecraft::world::BlockType equippedBlockType = vibecraft::world::BlockType::Air;
        float elapsedSeconds = 0.0f;
        float requiredSeconds = 0.0f;
        /// Time until the next dig tick while holding attack (immediate tick on target change).
        float digSoundCooldownSeconds = 0.0f;
    };

    struct SingleplayerLoadState
    {
        bool active = false;
        bool worldPrepared = false;
        float progress = 0.0f;
        std::string label;
    };

    struct CraftingMenuState
    {
        bool active = false;
        bool usesWorkbench = false;
        glm::ivec3 workbenchBlockPosition{0};
        CraftingGridSlots gridSlots{};
        InventorySlot carriedSlot{};
        std::string hint;
    };

    bool startHostSession();
    bool startClientSession(const std::string& address);
    void stopMultiplayerSessions();
    void updateMultiplayer(float deltaTimeSeconds);
    void loadMultiplayerPrefs();
    void saveMultiplayerPrefs() const;
    [[nodiscard]] std::filesystem::path multiplayerPrefsPath() const;
    void refreshDetectedLanAddress();
    void processJoinMenuTextInput();
    void tryStartHostFromMenu();
    void tryConnectFromJoinMenu();
    void sendInitialWorldToClient(std::uint16_t clientId);
    void applyChunkSnapshot(const vibecraft::multiplayer::protocol::ChunkSnapshotMessage& chunkMessage);
    void applyRemoteBlockEdit(const vibecraft::multiplayer::protocol::BlockEditEventMessage& editMessage);
    [[nodiscard]] vibecraft::multiplayer::protocol::ServerSnapshotMessage buildServerSnapshot() const;

    void update(float deltaTimeSeconds);
    void processInput(float deltaTimeSeconds);
    void beginSingleplayerLoad();
    void updateSingleplayerLoad();
    void syncWorldData();
    void respawnPlayer();
    void openCraftingMenu(bool useWorkbench, const glm::ivec3& workbenchBlockPosition = glm::ivec3(0));
    void closeCraftingMenu();
    void handleCraftingMenuClick();
    void returnCraftingSlotsToInventory();
    void spawnDroppedItem(vibecraft::world::BlockType blockType, const glm::ivec3& blockPosition);
    void spawnDroppedItemAtPosition(vibecraft::world::BlockType blockType, const glm::vec3& worldPosition);
    void spawnDroppedItemAtPosition(EquippedItem equippedItem, const glm::vec3& worldPosition);
    void updateDroppedItems(float deltaTimeSeconds, float eyeHeight);

    vibecraft::platform::Window window_;
    vibecraft::platform::InputState inputState_;
    vibecraft::render::Renderer renderer_;
    vibecraft::audio::SharedAudioOutput sharedAudioOutput_;
    vibecraft::audio::MusicDirector musicDirector_;
    vibecraft::audio::SoundEffects soundEffects_;
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
    vibecraft::game::MobSpawnSystem mobSpawnSystem_{};
    HotbarSlots hotbarSlots_{};
    BagSlots bagSlots_{};
    std::size_t selectedHotbarIndex_ = 0;
    float smoothedFrameTimeMs_ = 0.0f;
    bool frameTimeInitialized_ = false;
    GameScreen gameScreen_ = GameScreen::MainMenu;
    MultiplayerRuntimeMode multiplayerMode_ = MultiplayerRuntimeMode::SinglePlayer;
    std::unique_ptr<vibecraft::multiplayer::HostSession> hostSession_;
    std::unique_ptr<vibecraft::multiplayer::ClientSession> clientSession_;
    std::uint16_t localClientId_ = 0;
    std::string multiplayerAddress_ = "127.0.0.1";
    std::uint16_t multiplayerPort_ = 41234;
    std::string multiplayerStatusLine_;
    float networkTickAccumulatorSeconds_ = 0.0f;
    std::uint32_t networkServerTick_ = 0;
    MainMenuMultiplayerPanel mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
    std::string joinAddressInput_;
    std::string joinPortInput_;
    int joinFocusedField_ = 0;
    std::string detectedLanAddress_;
    std::vector<RemotePlayerState> remotePlayers_;
    std::unordered_set<std::uint16_t> worldSyncSentClients_;
    float mainMenuTimeSeconds_ = 0.0f;
    std::string mainMenuNotice_;
    std::string pauseMenuNotice_;
    bool pauseSoundSettingsOpen_ = false;
    bool pauseGameSettingsOpen_ = false;
    bool mobSpawningEnabled_ = true;
    bool mainMenuSoundSettingsOpen_ = false;
    float musicVolume_ = 0.85f;
    float sfxVolume_ = 1.0f;
    float heldItemSwing_ = 0.0f;
    float footstepDistanceAccumulator_ = 0.0f;
    bool craftingKeyWasDown_ = false;
    std::string respawnNotice_;
    std::vector<DroppedItem> droppedItems_;
    ActiveMiningState activeMiningState_{};
    SingleplayerLoadState singleplayerLoadState_{};
    CraftingMenuState craftingMenuState_{};
    /// Run-loop frame index (used to delay main-menu mouse activation until layout is stable).
    std::uint32_t runFrameIndex_ = 0;
    bool showWorldOriginGuides_ = false;
    bool debugF3KeyWasDown_ = false;
};
}  // namespace vibecraft::app
