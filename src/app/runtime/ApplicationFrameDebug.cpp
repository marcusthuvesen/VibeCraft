#include "vibecraft/app/Application.hpp"

#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool isSwordItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::DiamondSword:
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] const char* cardinalFacingLabel(const glm::vec3& forward)
{
    if (std::abs(forward.x) >= std::abs(forward.z))
    {
        return forward.x >= 0.0f ? "+X (East)" : "-X (West)";
    }
    return forward.z >= 0.0f ? "+Z (South)" : "-Z (North)";
}

[[nodiscard]] std::int64_t storageKeyForBlockPosition(const glm::ivec3& blockPosition)
{
    constexpr std::int64_t offset = 1LL << 20;
    constexpr std::int64_t mask = (1LL << 21) - 1LL;
    const std::int64_t x = (static_cast<std::int64_t>(blockPosition.x) + offset) & mask;
    const std::int64_t y = (static_cast<std::int64_t>(blockPosition.y) + offset) & mask;
    const std::int64_t z = (static_cast<std::int64_t>(blockPosition.z) + offset) & mask;
    return (x << 42) | (y << 21) | z;
}

[[nodiscard]] render::HudItemKind hudItemKindForEquippedItem(const EquippedItem equippedItem)
{
    return static_cast<render::HudItemKind>(equippedItem);
}

[[nodiscard]] render::FrameDebugData::HotbarSlotHud makeHudSlot(const InventorySlot& slot)
{
    const std::uint16_t maxDurability = maxDurabilityForEquippedItem(slot.equippedItem);
    const std::uint16_t normalizedDurability =
        maxDurability == 0
        ? 0
        : static_cast<std::uint16_t>(
              std::clamp<std::uint32_t>(
                  slot.durabilityRemaining == 0 ? maxDurability : slot.durabilityRemaining,
                  0U,
                  maxDurability));
    return render::FrameDebugData::HotbarSlotHud{
        .blockType = slot.blockType,
        .count = slot.count,
        .itemKind = hudItemKindForEquippedItem(slot.equippedItem),
        .durabilityRemaining = normalizedDurability,
        .durabilityMax = maxDurability,
        .heldItemUsesSwordPose = isSwordItem(slot.equippedItem),
    };
}
}  // namespace

void Application::buildFrameDebugData(
    const float deltaTimeSeconds,
    const game::DayNightSample& dayNightSample,
    const game::WeatherSample& weatherSample,
    const world::SurfaceBiome playerSurfaceBiome,
    const std::optional<world::RaycastHit>& raycastHit,
    render::FrameDebugData& frameDebugData)
{
    frameDebugData.chunkCount = static_cast<std::uint32_t>(world_.chunks().size());
    frameDebugData.dirtyChunkCount = static_cast<std::uint32_t>(world_.dirtyChunkCount());
    frameDebugData.totalFaces = world_.totalVisibleFaces();
    frameDebugData.residentChunkCount = static_cast<std::uint32_t>(residentChunkMeshIds_.size());
    frameDebugData.cameraPosition = camera_.position();
    const MenuUiMetrics menuUiMetrics = computeMenuUiMetrics(window_, inputState_, bgfx::getStats());
    frameDebugData.uiMenuWindowWidth = menuUiMetrics.windowWidth;
    frameDebugData.uiMenuWindowHeight = menuUiMetrics.windowHeight;
    frameDebugData.uiMenuTextWidth = menuUiMetrics.textWidth;
    frameDebugData.uiMenuTextHeight = menuUiMetrics.textHeight;
    const glm::ivec3 playerBlockPosition(
        static_cast<int>(std::floor(playerFeetPosition_.x)),
        static_cast<int>(std::floor(playerFeetPosition_.y)),
        static_cast<int>(std::floor(playerFeetPosition_.z)));
    const world::ChunkCoord playerChunkCoord =
        world::worldToChunkCoord(playerBlockPosition.x, playerBlockPosition.z);
    const glm::ivec3 playerChunkLocal(
        world::worldToLocalCoord(playerBlockPosition.x),
        playerBlockPosition.y,
        world::worldToLocalCoord(playerBlockPosition.z));
    const glm::vec3 cameraForward = camera_.forward();
    frameDebugData.debugCoordinatesLine = fmt::format(
        "XYZ: {:.2f} / {:.2f} / {:.2f}",
        playerFeetPosition_.x,
        playerFeetPosition_.y,
        playerFeetPosition_.z);
    frameDebugData.debugBlockLine = fmt::format(
        "Block: {} {} {}  Target: {}",
        playerBlockPosition.x,
        playerBlockPosition.y,
        playerBlockPosition.z,
        raycastHit.has_value()
            ? fmt::format("{}, {}, {}", raycastHit->solidBlock.x, raycastHit->solidBlock.y, raycastHit->solidBlock.z)
            : "none");
    frameDebugData.debugChunkLine = fmt::format(
        "Chunk: {} {}  In-chunk: {} {} {}",
        playerChunkCoord.x,
        playerChunkCoord.z,
        playerChunkLocal.x,
        playerChunkLocal.y,
        playerChunkLocal.z);
    frameDebugData.debugFacingLine = fmt::format(
        "Facing: {}  Look: {:.2f} / {:.2f} / {:.2f}",
        cardinalFacingLabel(cameraForward),
        cameraForward.x,
        cameraForward.y,
        cameraForward.z);
    frameDebugData.debugBiomeLine = fmt::format(
        "Biome: {}  Surface Y: {}",
        world::surfaceBiomeLabel(playerSurfaceBiome),
        terrainGenerator_.surfaceHeightAt(playerBlockPosition.x, playerBlockPosition.z));
    frameDebugData.uiCursorX = inputState_.mouseWindowX;
    frameDebugData.uiCursorY = inputState_.mouseWindowY;
    frameDebugData.showWorldOriginGuides = showWorldOriginGuides_;
    frameDebugData.health = playerVitals_.health();
    frameDebugData.maxHealth = playerVitals_.maxHealth();
    frameDebugData.air = playerVitals_.air();
    frameDebugData.maxAir = playerVitals_.maxAir();
    const float safeFrameTimeMs = std::max(smoothedFrameTimeMs_, 0.001f);
    const float smoothedFps = 1000.0f / safeFrameTimeMs;
    const int cycleSeconds = static_cast<int>(std::floor(dayNightSample.elapsedSeconds));
    const int cycleMinutesComponent = cycleSeconds / 60;
    const int cycleSecondsComponent = cycleSeconds % 60;
    frameDebugData.statusLine = fmt::format(
        "Pos: {} {} {}  HP: {:.0f}/{:.0f}  Air: {:.0f}/{:.0f}  Hazard: {}  Mouse: {}  Grounded: {}  Time: {} {:02d}:{:02d}  Weather: {}  Save: {}  Net: {}  Peers: {}  Frame: {:.2f} ms ({:.1f} fps){}",
        playerBlockPosition.x,
        playerBlockPosition.y,
        playerBlockPosition.z,
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
    frameDebugData.chatLines.clear();
    frameDebugData.chatLines.reserve(chatState_.history.size());
    for (const ChatLine& line : chatState_.history)
    {
        frameDebugData.chatLines.push_back(render::FrameDebugData::ChatLineHud{
            .text = line.text,
            .isError = line.isError,
            .timestampLabel = line.timestampLabel,
            .ageSeconds = std::max(0.0f, sessionPlayTimeSeconds_ - line.createdAtSeconds),
        });
    }
    frameDebugData.chatOpen = chatState_.open;
    frameDebugData.chatInputLine = chatState_.inputBuffer;
    frameDebugData.chatCursorIndex = chatState_.cursorIndex;
    frameDebugData.chatHintLine = chatState_.hintLine;
    for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
    {
        frameDebugData.hotbarSlots[slotIndex] = makeHudSlot(hotbarSlots_[slotIndex]);
    }
    frameDebugData.hotbarSelectedIndex = selectedHotbarIndex_;
    frameDebugData.selectedHotbarLabel = inventorySlotLabel(hotbarSlots_[selectedHotbarIndex_]);
    frameDebugData.selectedHotbarActionHint.clear();
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (selectedSlot.count > 0
            && selectedSlot.equippedItem == EquippedItem::None
            && selectedSlot.blockType == world::BlockType::PlanterTray)
        {
            frameDebugData.selectedHotbarActionHint = "Right-click: place a planter for greenhouse growth";
        }
        else if (selectedSlot.count > 0
            && selectedSlot.equippedItem == EquippedItem::None
            && selectedSlot.blockType == world::BlockType::GreenhouseGlass)
        {
            frameDebugData.selectedHotbarActionHint = "Right-click: build around planters to boost growth";
        }
        else if (selectedSlot.count > 0
            && selectedSlot.equippedItem == EquippedItem::None
            && selectedSlot.blockType == world::BlockType::PowerConduit)
        {
            frameDebugData.selectedHotbarActionHint = "Right-click: power nearby greenhouse growth";
        }
        else if (selectedSlot.count > 0
            && selectedSlot.equippedItem == EquippedItem::None
            && selectedSlot.blockType == world::BlockType::FiberSapling)
        {
            frameDebugData.selectedHotbarActionHint = "Right-click: plant in a soil-filled planter tray";
        }
        else if (selectedSlot.count > 0 && selectedSlot.equippedItem != EquippedItem::None)
        {
            switch (selectedSlot.equippedItem)
            {
            case EquippedItem::ScoutHelmet:
                frameDebugData.selectedHotbarActionHint = "Open inventory: equip in Helmet slot";
                break;
            case EquippedItem::ScoutChestRig:
                frameDebugData.selectedHotbarActionHint = "Open inventory: equip in Chest slot";
                break;
            case EquippedItem::ScoutGreaves:
                frameDebugData.selectedHotbarActionHint = "Open inventory: equip in Legs slot";
                break;
            case EquippedItem::ScoutBoots:
                frameDebugData.selectedHotbarActionHint = "Open inventory: equip in Boots slot";
                break;
            default:
                frameDebugData.selectedHotbarActionHint = surfaceBiomeGuidance(terrainGenerator_, playerFeetPosition_);
                break;
            }
        }
        else
        {
            frameDebugData.selectedHotbarActionHint = surfaceBiomeGuidance(terrainGenerator_, playerFeetPosition_);
        }
    }
    frameDebugData.survivalTipLine = survivalTipLine(sessionPlayTimeSeconds_);
    compactBagSlots(bagSlots_);
    frameDebugData.heldItemSwing = heldItemSwing_;
    for (std::size_t i = 0; i < frameDebugData.bagSlots.size(); ++i)
    {
        frameDebugData.bagSlots[i] = makeHudSlot(bagSlots_[i]);
    }
    for (std::size_t i = 0; i < frameDebugData.equipmentSlots.size(); ++i)
    {
        frameDebugData.equipmentSlots[i] = makeHudSlot(equipmentSlots_[i]);
    }
    if (craftingMenuState_.active)
    {
        frameDebugData.craftingMenuActive = true;
        frameDebugData.craftingUsesWorkbench = craftingMenuState_.usesWorkbench;
        frameDebugData.craftingCreativeModeEnabled = creativeModeEnabled_;
        switch (craftingMenuState_.mode)
        {
        case CraftingMenuState::Mode::InventoryCrafting:
            frameDebugData.craftingUiMode = render::CraftingUiMode::Inventory;
            break;
        case CraftingMenuState::Mode::WorkbenchCrafting:
            frameDebugData.craftingUiMode = render::CraftingUiMode::Workbench;
            break;
        case CraftingMenuState::Mode::ChestStorage:
            frameDebugData.craftingUiMode = render::CraftingUiMode::Chest;
            break;
        case CraftingMenuState::Mode::Furnace:
            frameDebugData.craftingUiMode = render::CraftingUiMode::Furnace;
            break;
        }
        if (craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage)
        {
            frameDebugData.craftingTitle = "Chest";
        }
        else if (craftingMenuState_.mode == CraftingMenuState::Mode::Furnace)
        {
            frameDebugData.craftingTitle = "Furnace";
        }
        else
        {
            frameDebugData.craftingTitle =
                craftingMenuState_.usesWorkbench ? "Crafting Table" : "Inventory Crafting";
        }
        frameDebugData.craftingBagStartRow =
            static_cast<std::uint8_t>(std::min<std::size_t>(craftingMenuState_.bagStartRow, 255));
        const std::size_t visibleStart = craftingMenuState_.bagStartRow * 9;
        const std::size_t visibleEndExclusive =
            std::min<std::size_t>(visibleStart + 27, bagSlots_.size());
        std::string craftingHint = craftingMenuState_.hint;
        if (craftingMenuState_.mode == CraftingMenuState::Mode::Furnace)
        {
            const auto furnaceIt = furnaceStatesByPosition_.find(
                storageKeyForBlockPosition(craftingMenuState_.furnaceBlockPosition));
            if (furnaceIt != furnaceStatesByPosition_.end())
            {
                frameDebugData.craftingProgressFraction = furnaceSmeltFraction(furnaceIt->second);
                frameDebugData.craftingFuelFraction = furnaceFuelFraction(furnaceIt->second);
                craftingHint = fmt::format(
                    "{}  Smelt {:>3.0f}%  Fuel {:>3.0f}%",
                    craftingHint,
                    frameDebugData.craftingProgressFraction * 100.0f,
                    frameDebugData.craftingFuelFraction * 100.0f);
            }
        }
        frameDebugData.craftingHint = fmt::format(
            "{}  |  Bag slots: {}-{} / {} (mouse wheel to scroll)",
            craftingHint,
            visibleStart + 1,
            visibleEndExclusive,
            bagSlots_.size());
        frameDebugData.craftingCursorSlot = makeHudSlot(craftingMenuState_.carriedSlot);
        if (craftingMenuState_.mode == CraftingMenuState::Mode::Furnace)
        {
            const auto furnaceIt = furnaceStatesByPosition_.find(
                storageKeyForBlockPosition(craftingMenuState_.furnaceBlockPosition));
            if (furnaceIt != furnaceStatesByPosition_.end())
            {
                frameDebugData.craftingResultSlot = makeHudSlot(furnaceIt->second.outputSlot);
            }
        }
        else if (craftingMenuState_.mode != CraftingMenuState::Mode::ChestStorage)
        {
            if (const std::optional<CraftingMatch> craftingMatch = evaluateCraftingGrid(
                    craftingMenuState_.gridSlots,
                    craftingMenuState_.usesWorkbench ? CraftingMode::Workbench3x3 : CraftingMode::Inventory2x2);
                craftingMatch.has_value())
            {
                frameDebugData.craftingResultSlot = makeHudSlot(craftingMatch->output);
            }
        }
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.craftingGridSlots.size(); ++slotIndex)
        {
            frameDebugData.craftingGridSlots[slotIndex] = makeHudSlot(craftingMenuState_.gridSlots[slotIndex]);
        }
    }
    frameDebugData.worldPickups.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        const float bobOffset = 0.14f + std::sin(droppedItem.ageSeconds * 6.0f) * 0.08f;
        frameDebugData.worldPickups.push_back(render::FrameDebugData::WorldPickupHud{
            .blockType = droppedItem.blockType,
            .itemKind = hudItemKindForEquippedItem(droppedItem.equippedItem),
            .worldPosition = droppedItem.worldPosition + glm::vec3(0.0f, bobOffset, 0.0f),
            .spinRadians = droppedItem.spinRadians,
        });
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        const std::vector<game::MobInstance>& mobsForHud = multiplayerMode_ == MultiplayerRuntimeMode::Client
            ? clientReplicatedMobs_
            : mobSpawnSystem_.mobs();
        updateMobSoundEffects(deltaTimeSeconds, mobsForHud);
        frameDebugData.worldMobs.reserve(mobsForHud.size() + remotePlayers_.size());
        for (const game::MobInstance& mob : mobsForHud)
        {
            frameDebugData.worldMobs.push_back(render::FrameDebugData::WorldMobHud{
                .feetPosition = {mob.feetX, mob.feetY, mob.feetZ},
                .yawRadians = mob.yawRadians,
                .pitchRadians = mob.pitchRadians,
                .halfWidth = mob.halfWidth,
                .height = mob.height,
                .mobKind = mob.kind,
                .mobHealthCurrent = mob.health,
                .mobHealthMax = game::mobKindDefaultMaxHealth(mob.kind),
            });
        }
        frameDebugData.worldProjectiles.reserve(mobSpawnSystem_.projectiles().size());
        for (const game::HostileProjectile& projectile : mobSpawnSystem_.projectiles())
        {
            if (projectile.kind != game::HostileProjectileKind::Arrow)
            {
                continue;
            }
            frameDebugData.worldProjectiles.push_back(render::FrameDebugData::WorldProjectileHud{
                .worldPosition = projectile.position,
                .velocity = projectile.velocity,
                .itemKind = render::HudItemKind::Arrow,
            });
        }
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        for (const RemotePlayerState& remotePlayer : remotePlayers_)
        {
            frameDebugData.worldMobs.push_back(render::FrameDebugData::WorldMobHud{
                .feetPosition = remotePlayer.position,
                .yawRadians = remotePlayer.yawDegrees * kDegreesToRadians,
                .pitchRadians = remotePlayer.pitchDegrees * kDegreesToRadians,
                .halfWidth = 0.30f,
                .height = 2.0f,
                .mobKind = game::MobKind::Player,
                .heldBlockType = remotePlayer.selectedBlockType,
                .heldItemKind = hudItemKindForEquippedItem(remotePlayer.selectedEquippedItem),
                .heldItemUsesSwordPose = isSwordItem(remotePlayer.selectedEquippedItem),
            });
        }
    }

    if (raycastHit.has_value())
    {
        frameDebugData.hasTarget = true;
        frameDebugData.targetBlock = raycastHit->solidBlock;
        if (activeMiningState_.active
            && activeMiningState_.targetBlockPosition == raycastHit->solidBlock
            && activeMiningState_.requiredSeconds > 0.0f)
        {
            frameDebugData.miningTargetActive = true;
            frameDebugData.miningTargetProgress = std::clamp(
                activeMiningState_.elapsedSeconds / activeMiningState_.requiredSeconds,
                0.0f,
                1.0f);
        }
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        frameDebugData.mainMenuActive = true;
        frameDebugData.mainMenuTimeSeconds = mainMenuTimeSeconds_;
        frameDebugData.mainMenuNotice = mainMenuNotice_;
        frameDebugData.mainMenuCreativeModeEnabled = creativeModeEnabled_;
        frameDebugData.mainMenuSpawnPresetLabel = spawnPresetLabel(spawnPreset_);
        frameDebugData.mainMenuSpawnBiomeLabel = spawnBiomeTargetLabel(spawnBiomeTarget_);
        if (!singleplayerWorlds_.empty() && selectedSingleplayerWorldIndex_ < singleplayerWorlds_.size())
        {
            frameDebugData.mainMenuSelectedWorldLabel =
                singleplayerWorlds_[selectedSingleplayerWorldIndex_].metadata.displayName;
        }
        else
        {
            frameDebugData.mainMenuSelectedWorldLabel = "No world selected";
        }
        frameDebugData.mainMenuSingleplayerPanelActive = mainMenuSingleplayerPickerOpen_;
        if (mainMenuSoundSettingsOpen_)
        {
            frameDebugData.mainMenuSoundSettingsActive = true;
            frameDebugData.mainMenuSoundMusicVolume = musicVolume_;
            frameDebugData.mainMenuSoundSfxVolume = sfxVolume_;
            frameDebugData.mainMenuSoundSettingsHoveredControl = render::Renderer::hitTestPauseSoundMenu(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight);
        }
        else if (singleplayerLoadState_.active || mainMenuBootLoading_)
        {
            frameDebugData.mainMenuHoveredButton = -1;
            frameDebugData.mainMenuLoadingActive = true;
            if (singleplayerLoadState_.active)
            {
                frameDebugData.mainMenuLoadingProgress = singleplayerLoadState_.progress;
                frameDebugData.mainMenuLoadingLabel = singleplayerLoadState_.label;
            }
            else
            {
                frameDebugData.mainMenuLoadingProgress = std::min(1.0f, 0.25f + mainMenuTimeSeconds_ * 0.6f);
                frameDebugData.mainMenuLoadingTitle = "LOADING MENU";
                frameDebugData.mainMenuLoadingLabel = "Preparing main menu...";
            }
        }
        else if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None)
        {
            frameDebugData.mainMenuMultiplayerPanel = mainMenuMultiplayerPanel_;
            frameDebugData.mainMenuMultiplayerLanAddress = detectedLanAddress_;
            frameDebugData.mainMenuJoinAddressField = joinAddressInput_;
            frameDebugData.mainMenuJoinPortField = joinPortInput_;
            frameDebugData.mainMenuMultiplayerPortDisplay = multiplayerPort_;
            frameDebugData.mainMenuJoinFocusedField = joinFocusedField_;
            frameDebugData.mainMenuJoinPresetButtonLabels.clear();
            for (const JoinPresetEntry& preset : joinPresets_)
            {
                frameDebugData.mainMenuJoinPresetButtonLabels.push_back(
                    fmt::format("{} — {}:{}", preset.label, preset.host, preset.port));
            }
            const int joinSlotsForMpLayout = static_cast<int>(std::min(joinPresets_.size(), std::size_t(3)));
            const int mainMenuContentTopBias = vibecraft::render::detail::mainMenuLogoReservedDbgRows(
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
            const int multiplayerRowShift = render::Renderer::multiplayerMenuRowShift(
                menuUiMetrics.textHeight,
                mainMenuMultiplayerPanel_,
                mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join ? joinSlotsForMpLayout : 0,
                mainMenuContentTopBias);
            switch (mainMenuMultiplayerPanel_)
            {
            case MainMenuMultiplayerPanel::Hub:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerHub(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::Host:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerHost(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::Join:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerJoin(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    joinSlotsForMpLayout,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::None:
            default:
                frameDebugData.mainMenuMultiplayerHoveredControl = -1;
                break;
            }
        }
        else
        {
            if (mainMenuSingleplayerPickerOpen_)
            {
                frameDebugData.mainMenuSingleplayerHoveredControl = render::Renderer::hitTestMainMenuSingleplayerPanel(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight);
            }
            else
            {
                frameDebugData.mainMenuHoveredButton = render::Renderer::hitTestMainMenu(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    renderer_.menuLogoWidthPx(),
                    renderer_.menuLogoHeightPx());
            }
        }
        if (singleplayerLoadState_.active)
        {
            frameDebugData.mainMenuLoadingActive = true;
            frameDebugData.mainMenuLoadingProgress = singleplayerLoadState_.progress;
            if (pendingHostStartAfterWorldLoad_)
            {
                frameDebugData.mainMenuLoadingTitle = "STARTING MULTIPLAYER HOST";
            }
            else if (pendingClientJoinAfterWorldLoad_)
            {
                frameDebugData.mainMenuLoadingTitle = "JOINING MULTIPLAYER";
            }
            else
            {
                frameDebugData.mainMenuLoadingTitle = "LOADING SINGLEPLAYER";
            }
            frameDebugData.mainMenuLoadingLabel = singleplayerLoadState_.label;
        }
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        frameDebugData.pauseMenuActive = true;
        frameDebugData.pauseMenuNotice = pauseMenuNotice_;
        if (pauseGameSettingsOpen_)
        {
            frameDebugData.pauseGameSettingsActive = true;
            frameDebugData.mobSpawningEnabled = mobSpawningEnabled_;
            frameDebugData.pauseCreativeModeEnabled = creativeModeEnabled_;
            frameDebugData.pauseDifficultyLabel = difficultyGradeLabel(difficultyGrade_);
            frameDebugData.pauseSpawnBiomeLabel = spawnBiomeTargetLabel(spawnBiomeTarget_);
            frameDebugData.pauseWeatherLabel = weatherLabel(weatherSample.type);
            frameDebugData.pauseGameSettingsHoveredControl = render::Renderer::hitTestPauseGameSettingsMenu(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight);
        }
        else if (pauseSoundSettingsOpen_)
        {
            frameDebugData.pauseSoundSettingsActive = true;
            frameDebugData.pauseSoundMusicVolume = musicVolume_;
            frameDebugData.pauseSoundSfxVolume = sfxVolume_;
            int hoveredControl = render::Renderer::hitTestPauseSoundMenu(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight);
            if (render::Renderer::pauseSoundSliderValueFromMouse(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    true)
                .has_value())
            {
                hoveredControl = 1;
            }
            else if (render::Renderer::pauseSoundSliderValueFromMouse(
                         menuUiMetrics.mouseX,
                         menuUiMetrics.mouseY,
                         menuUiMetrics.windowWidth,
                         menuUiMetrics.windowHeight,
                         menuUiMetrics.textWidth,
                         menuUiMetrics.textHeight,
                         false)
                         .has_value())
            {
                hoveredControl = 3;
            }
            frameDebugData.pauseSoundSettingsHoveredControl = hoveredControl;
        }
        else
        {
            frameDebugData.pauseMenuHoveredButton = render::Renderer::hitTestPauseMenu(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight);
        }
    }
}
}  // namespace vibecraft::app
