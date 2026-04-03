#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

#include "vibecraft/app/ApplicationChunkStreaming.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"
#include "vibecraft/app/ApplicationOxygenRuntime.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/core/Logger.hpp"

namespace vibecraft::app
{
void Application::beginSingleplayerLoad()
{
    if (gameScreen_ != GameScreen::MainMenu || singleplayerLoadState_.active)
    {
        return;
    }

    if (!ensureSelectedSingleplayerWorld() || selectedSingleplayerWorldIndex_ >= singleplayerWorlds_.size())
    {
        return;
    }

    pendingClientJoinAfterWorldLoad_ = false;
    singleplayerLoadState_.active = true;
    singleplayerLoadState_.worldPrepared = false;
    singleplayerLoadState_.playerStateLoaded = false;
    singleplayerLoadState_.progress = 0.0f;
    singleplayerLoadState_.label = "Loading world...";

    stopMultiplayerSessions();
    mainMenuSingleplayerPickerOpen_ = false;
    mainMenuSingleplayerAwaitingMouseRelease_ = false;
    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
    mainMenuSoundSettingsOpen_ = false;
    mainMenuNotice_.clear();
    window_.setTextInputActive(false);
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();

    std::vector<std::uint64_t> removedMeshIds(residentChunkMeshIds_.begin(), residentChunkMeshIds_.end());
    if (!removedMeshIds.empty())
    {
        renderer_.updateSceneMeshes({}, removedMeshIds);
    }
    residentChunkMeshIds_.clear();

    vibecraft::world::World::ChunkMap emptyChunks;
    world_.replaceChunks(std::move(emptyChunks));
    droppedItems_.clear();
    remotePlayers_.clear();
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
    mobSpawnSystem_.clearAllMobs();
    applyDefaultHotbarLoadout(hotbarSlots_, selectedHotbarIndex_);
    bagSlots_.fill({});
    static_cast<void>(ensureStarterRelayAvailable(hotbarSlots_, bagSlots_, selectedHotbarIndex_));
    equipmentSlots_.fill({});
    activeMiningState_ = {};
    playerVitals_.reset();
    oxygenSystem_.resetForNewGame();
    syncOxygenEquipmentSlotFromSystem(equipmentSlots_, oxygenSystem_);
    playerHazards_ = {};
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    isGrounded_ = false;
    jumpWasHeld_ = false;
    autoJumpCooldownSeconds_ = 0.0f;
    footstepDistanceAccumulator_ = 0.0f;
    heldItemSwing_ = 0.0f;
    respawnNotice_ = "Starter Oxygen Generators ready in slot 9.";
    dayNightCycle_.setElapsedSeconds(150.0f);
    weatherSystem_.setElapsedSeconds(0.0f);

    const SingleplayerWorldEntry& selectedWorld = singleplayerWorlds_[selectedSingleplayerWorldIndex_];
    activeSingleplayerWorldFolderName_ = selectedWorld.folderName;
    activeSingleplayerWorldDisplayName_ = selectedWorld.metadata.displayName;
    savePath_ = singleplayerWorldDataPath(selectedWorld.folderName);
    autosaveAccumulatorSeconds_ = 0.0f;

    const std::uint32_t worldSeed =
        selectedWorld.metadata.seed != 0 ? selectedWorld.metadata.seed : generateRandomWorldSeed();
    world_.setGenerationSeed(worldSeed);
    terrainGenerator_.setWorldSeed(worldSeed);

    const std::filesystem::path worldPath = singleplayerWorldDataPath(selectedWorld.folderName);
    if (selectedWorld.hasWorldData)
    {
        if (!world_.load(worldPath))
        {
            mainMenuNotice_ = "Could not load the selected world.";
            singleplayerLoadState_ = {};
            unloadActiveSingleplayerWorld();
            return;
        }
        if (world_.generationSeed() == 0 && selectedWorld.metadata.seed != 0)
        {
            world_.setGenerationSeed(selectedWorld.metadata.seed);
        }
        terrainGenerator_.setWorldSeed(world_.generationSeed());
        core::logInfo(fmt::format(
            "Loading singleplayer world {} with seed {}",
            activeSingleplayerWorldDisplayName_,
            world_.generationSeed()));
    }
    else
    {
        core::logInfo(fmt::format(
            "Starting new singleplayer world {} with seed {}",
            activeSingleplayerWorldDisplayName_,
            worldSeed));
    }

    if (const std::optional<SingleplayerPlayerState> playerState =
            SingleplayerSaveSerializer::loadPlayerState(singleplayerPlayerDataPath(selectedWorld.folderName));
        playerState.has_value())
    {
        playerFeetPosition_ = playerState->playerFeetPosition;
        spawnFeetPosition_ = playerState->spawnFeetPosition;
        camera_.setYawPitch(playerState->cameraYawDegrees, playerState->cameraPitchDegrees);
        camera_.setPosition(
            playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
        playerVitals_.setHealthAndAir(playerState->health, playerState->air);
        oxygenSystem_.setState(playerState->oxygenState);
        creativeModeEnabled_ = playerState->creativeModeEnabled;
        selectedHotbarIndex_ = std::min<std::size_t>(playerState->selectedHotbarIndex, hotbarSlots_.size() - 1);
        dayNightCycle_.setElapsedSeconds(playerState->dayNightElapsedSeconds);
        weatherSystem_.setElapsedSeconds(playerState->weatherElapsedSeconds);
        hotbarSlots_ = playerState->hotbarSlots;
        bagSlots_ = playerState->bagSlots;
        if (ensureStarterRelayAvailable(hotbarSlots_, bagSlots_, selectedHotbarIndex_))
        {
            respawnNotice_ = "Starter Oxygen Generator added to your inventory.";
        }
        equipmentSlots_ = playerState->equipmentSlots;
        if (equipmentSlots_[equipmentSlotIndex(EquipmentSlotKind::OxygenTank)].count == 0)
        {
            syncOxygenEquipmentSlotFromSystem(equipmentSlots_, oxygenSystem_);
        }
        else
        {
            syncOxygenSystemFromEquipmentSlot(equipmentSlots_, oxygenSystem_, false);
        }
        chestSlotsByPosition_ = playerState->chestSlotsByPosition;
        droppedItems_.clear();
        droppedItems_.reserve(playerState->droppedItems.size());
        for (const SavedDroppedItem& droppedItem : playerState->droppedItems)
        {
            droppedItems_.push_back(DroppedItem{
                .blockType = droppedItem.blockType,
                .equippedItem = droppedItem.equippedItem,
                .worldPosition = droppedItem.worldPosition,
                .velocity = droppedItem.velocity,
                .ageSeconds = droppedItem.ageSeconds,
                .pickupDelaySeconds = droppedItem.pickupDelaySeconds,
                .spinRadians = droppedItem.spinRadians,
            });
        }
        playerOxygenEnvironment_ = refreshPlayerOxygenEnvironment(
            world_,
            terrainGenerator_,
            playerFeetPosition_,
            playerHazards_,
            creativeModeEnabled_);
        singleplayerLoadState_.playerStateLoaded = true;
        mainMenuNotice_ = fmt::format("Continuing {}.", activeSingleplayerWorldDisplayName_);
    }
    else
    {
        mainMenuNotice_ = fmt::format("Entering {}.", activeSingleplayerWorldDisplayName_);
    }
}

void Application::updateSingleplayerLoad()
{
    if (pendingClientJoinAfterWorldLoad_)
    {
        ++clientJoinLoadDebugFrame_;
        if (clientSession_ == nullptr)
        {
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;
            mainMenuNotice_ = "Could not connect. Check address, firewall, and that the host is running.";
            return;
        }

        world::ChunkCoord cameraChunk = world::worldToChunkCoord(
            static_cast<int>(std::floor(camera_.position().x)),
            static_cast<int>(std::floor(camera_.position().z)));
        const std::size_t residentTarget = static_cast<std::size_t>(
            (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1));
        const bool connected = clientSession_->connected();

        if (connected && world_.chunks().empty())
        {
            singleplayerLoadState_.progress = std::max(singleplayerLoadState_.progress, 0.18f);
            singleplayerLoadState_.label = "Waiting for world data from host...";
            if ((clientJoinLoadDebugFrame_ % 45U) == 1U)
            {
                logMultiplayerJoinDiag(
                    "client join load: waiting for first chunk (frame {}) connected={} localId={}",
                    clientJoinLoadDebugFrame_,
                    connected,
                    localClientId_);
            }
            return;
        }

        if (!clientJoinLoggedFirstChunkSummary_ && !world_.chunks().empty())
        {
            clientJoinLoggedFirstChunkSummary_ = true;
            world::ChunkCoord minC{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
            world::ChunkCoord maxC{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
            for (const auto& [coord, chunk] : world_.chunks())
            {
                static_cast<void>(chunk);
                minC.x = std::min(minC.x, coord.x);
                minC.z = std::min(minC.z, coord.z);
                maxC.x = std::max(maxC.x, coord.x);
                maxC.z = std::max(maxC.z, coord.z);
            }
            logMultiplayerJoinDiag(
                "client join load: first chunk data — count {}  AABB chunks x[{},{}] z[{},{}]  cameraChunk ({},{})  "
                "feet ({:.2f},{:.2f},{:.2f})",
                world_.chunks().size(),
                minC.x,
                maxC.x,
                minC.z,
                maxC.z,
                cameraChunk.x,
                cameraChunk.z,
                playerFeetPosition_.x,
                playerFeetPosition_.y,
                playerFeetPosition_.z);
        }

        if (!world_.chunks().empty())
        {
            bool hasChunkNearCamera = false;
            for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
                 chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius && !hasChunkNearCamera;
                 ++chunkZ)
            {
                for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
                     chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
                     ++chunkX)
                {
                    if (world_.chunks().contains(world::ChunkCoord{chunkX, chunkZ}))
                    {
                        hasChunkNearCamera = true;
                        break;
                    }
                }
            }

            if (!hasChunkNearCamera)
            {
                const world::ChunkCoord firstReceivedCoord = world_.chunks().begin()->first;
                logMultiplayerJoinDiag(
                    "client join load: re-anchor camera to first received chunk — was ({},{}) -> ({},{}) "
                    "(no resident chunks near prior camera)",
                    cameraChunk.x,
                    cameraChunk.z,
                    firstReceivedCoord.x,
                    firstReceivedCoord.z);
                playerFeetPosition_.x = static_cast<float>(firstReceivedCoord.x * world::Chunk::kSize)
                    + static_cast<float>(world::Chunk::kSize) * 0.5f;
                playerFeetPosition_.z = static_cast<float>(firstReceivedCoord.z * world::Chunk::kSize)
                    + static_cast<float>(world::Chunk::kSize) * 0.5f;
                camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
                cameraChunk = firstReceivedCoord;
            }
        }

        const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
        std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
            dirtyCoords.begin(),
            dirtyCoords.end());
        const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
            world_,
            chunkMesher_,
            residentChunkMeshIds_,
            dirtyCoordSet,
            cameraChunk,
            kStreamingSettings.residentChunkRadius,
            std::max<std::size_t>(12, kStreamingSettings.meshBuildBudgetPerFrame * 3));
        applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);
        if (!cpuData.dirtyResidentMeshUpdates.empty())
        {
            world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
        }

        std::size_t residentGeneratedCount = 0;
        std::size_t residentMeshCount = 0;
        std::size_t residentDirtyCount = 0;
        for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
             chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
             ++chunkZ)
        {
            for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
                 chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
                 ++chunkX)
            {
                const world::ChunkCoord coord{chunkX, chunkZ};
                if (world_.chunks().contains(coord))
                {
                    ++residentGeneratedCount;
                }
                if (residentChunkMeshIds_.contains(chunkMeshId(coord)))
                {
                    ++residentMeshCount;
                }
                if (dirtyCoordSet.contains(coord))
                {
                    ++residentDirtyCount;
                }
            }
        }

        if (connected)
        {
            if ((clientJoinLoadDebugFrame_ % 30U) == 0U)
            {
                logMultiplayerJoinDiag(
                    "client join load: frame {}  camChunk ({},{})  feet ({:.2f},{:.2f},{:.2f})  yaw {:.1f}  "
                    "terrain {}/{}  mesh {}/{}  dirtyInWindow {}  worldChunks {}",
                    clientJoinLoadDebugFrame_,
                    cameraChunk.x,
                    cameraChunk.z,
                    playerFeetPosition_.x,
                    playerFeetPosition_.y,
                    playerFeetPosition_.z,
                    camera_.yawDegrees(),
                    residentGeneratedCount,
                    residentTarget,
                    residentMeshCount,
                    residentTarget,
                    residentDirtyCount,
                    world_.chunks().size());
            }
            singleplayerLoadState_.label = fmt::format(
                "Receiving world... terrain {}/{}  meshes {}/{}",
                residentGeneratedCount,
                residentTarget,
                residentMeshCount,
                residentTarget);
            const float residentGeneratedProgress = residentTarget > 0
                ? static_cast<float>(residentGeneratedCount) / static_cast<float>(residentTarget)
                : 1.0f;
            const float residentMeshProgress = residentTarget > 0
                ? static_cast<float>(residentMeshCount) / static_cast<float>(residentTarget)
                : 1.0f;
            singleplayerLoadState_.progress =
                std::clamp(0.25f + residentGeneratedProgress * 0.35f + residentMeshProgress * 0.40f, 0.0f, 1.0f);
        }
        else if (clientSession_->connecting())
        {
            singleplayerLoadState_.progress = std::max(singleplayerLoadState_.progress, 0.08f);
            singleplayerLoadState_.label = fmt::format("Connecting to {}:{}...", multiplayerAddress_, multiplayerPort_);
            return;
        }
        else
        {
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;
            mainMenuNotice_ = clientSession_->lastError().empty()
                ? "Could not connect. Check address, firewall, and that the host is running."
                : "Could not connect: " + clientSession_->lastError();
            stopMultiplayerSessions();
            return;
        }

        if (connected && residentGeneratedCount >= residentTarget && residentMeshCount >= residentTarget
            && residentDirtyCount == 0)
        {
            singleplayerLoadState_.progress = 1.0f;
            singleplayerLoadState_.label = "World ready";
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;

            gameScreen_ = GameScreen::Playing;
            mouseCaptured_ = true;
            window_.setRelativeMouseMode(true);
            inputState_.clearMouseMotion();
        }
        return;
    }

    const std::size_t bootstrapTarget = static_cast<std::size_t>(
        (kStreamingSettings.bootstrapChunkRadius * 2 + 1) * (kStreamingSettings.bootstrapChunkRadius * 2 + 1));
    const world::ChunkCoord originChunk = singleplayerLoadState_.playerStateLoaded
        ? world::worldToChunkCoord(
            static_cast<int>(std::floor(playerFeetPosition_.x)),
            static_cast<int>(std::floor(playerFeetPosition_.z)))
        : world::ChunkCoord{0, 0};

    if (!singleplayerLoadState_.worldPrepared)
    {
        world_.generateMissingChunksAround(
            terrainGenerator_,
            originChunk,
            kStreamingSettings.bootstrapChunkRadius,
            std::max<std::size_t>(6, kStreamingSettings.generationChunkBudgetPerFrame));

        const std::size_t generatedCount = std::min(world_.chunks().size(), bootstrapTarget);
        singleplayerLoadState_.progress =
            0.45f * (bootstrapTarget > 0 ? static_cast<float>(generatedCount) / static_cast<float>(bootstrapTarget) : 1.0f);
        singleplayerLoadState_.label =
            fmt::format("Generating world... {}/{} chunks", generatedCount, bootstrapTarget);

        if (generatedCount < bootstrapTarget)
        {
            return;
        }

        const float spawnHeight = kPlayerMovementSettings.standingColliderHeight;
        if (!singleplayerLoadState_.playerStateLoaded)
        {
            playerFeetPosition_ = resolveSpawnFeetPosition(
                world_,
                terrainGenerator_,
                spawnPreset_,
                spawnBiomeTarget_,
                camera_.position(),
                spawnHeight);
            spawnFeetPosition_ = playerFeetPosition_;
            camera_.setPosition(
                playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
        }
        isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, spawnHeight);
        accumulatedFallDistance_ = 0.0f;
        playerHazards_ = samplePlayerHazards(
            world_,
            playerFeetPosition_,
            kPlayerMovementSettings.standingColliderHeight,
            kPlayerMovementSettings.standingEyeHeight);
        singleplayerLoadState_.worldPrepared = true;
    }

    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    const std::size_t residentTarget = static_cast<std::size_t>(
        (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1));

    world_.generateMissingChunksAround(
        terrainGenerator_,
        cameraChunk,
        kStreamingSettings.residentChunkRadius,
        std::max<std::size_t>(12, kStreamingSettings.generationChunkBudgetPerFrame * 2));

    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
        world_,
        chunkMesher_,
        residentChunkMeshIds_,
        dirtyCoordSet,
        cameraChunk,
        kStreamingSettings.residentChunkRadius,
        std::max<std::size_t>(12, kStreamingSettings.meshBuildBudgetPerFrame * 3));
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);
    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }

    std::size_t residentGeneratedCount = 0;
    std::size_t residentMeshCount = 0;
    std::size_t residentDirtyCount = 0;
    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            if (world_.chunks().contains(coord))
            {
                ++residentGeneratedCount;
            }
            if (residentChunkMeshIds_.contains(chunkMeshId(coord)))
            {
                ++residentMeshCount;
            }
            if (dirtyCoordSet.contains(coord))
            {
                ++residentDirtyCount;
            }
        }
    }

    const float residentGeneratedProgress = residentTarget > 0
        ? static_cast<float>(residentGeneratedCount) / static_cast<float>(residentTarget)
        : 1.0f;
    const float residentMeshProgress = residentTarget > 0
        ? static_cast<float>(residentMeshCount) / static_cast<float>(residentTarget)
        : 1.0f;
    singleplayerLoadState_.progress =
        std::clamp(0.45f + residentGeneratedProgress * 0.2f + residentMeshProgress * 0.35f, 0.0f, 1.0f);
    singleplayerLoadState_.label = fmt::format(
        "Preparing spawn area... terrain {}/{}  meshes {}/{}",
        residentGeneratedCount,
        residentTarget,
        residentMeshCount,
        residentTarget);

    if (residentGeneratedCount >= residentTarget && residentMeshCount >= residentTarget && residentDirtyCount == 0)
    {
        singleplayerLoadState_.progress = 1.0f;
        singleplayerLoadState_.label = "World ready";
        singleplayerLoadState_.active = false;

        if (pendingHostStartAfterWorldLoad_)
        {
            if (!startHostSession())
            {
                pendingHostStartAfterWorldLoad_ = false;
                mainMenuNotice_ = "Could not start hosting. Is the port already in use?";
                return;
            }
            pendingHostStartAfterWorldLoad_ = false;
        }

        if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
        {
            static_cast<void>(saveActiveSingleplayerWorld(false));
        }

        gameScreen_ = GameScreen::Playing;
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }
}
}  // namespace vibecraft::app
