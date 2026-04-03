#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"

namespace vibecraft::app
{
bool Application::startHostSession()
{
    stopMultiplayerSessions();

    auto hostSession = std::make_unique<multiplayer::HostSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!hostSession->start(multiplayerPort_))
    {
        multiplayerStatusLine_ = "host failed: " + hostSession->lastError();
        return false;
    }

    hostSession_ = std::move(hostSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Host;
    localClientId_ = 0;
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("hosting on :{}", multiplayerPort_);
    return true;
}

bool Application::startClientSession(const std::string& address)
{
    stopMultiplayerSessions();

    auto clientSession = std::make_unique<multiplayer::ClientSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!clientSession->connect(address, multiplayerPort_, "Player"))
    {
        multiplayerStatusLine_ = "join failed: " + clientSession->lastError();
        return false;
    }

    clientSession_ = std::move(clientSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Client;
    localClientId_ = 0;
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("connecting {}:{}", address, multiplayerPort_);
    return true;
}

void Application::stopMultiplayerSessions()
{
    if (clientSession_ != nullptr)
    {
        clientSession_->disconnect();
        clientSession_.reset();
    }
    if (hostSession_ != nullptr)
    {
        hostSession_->shutdown();
        hostSession_.reset();
    }
    multiplayerMode_ = MultiplayerRuntimeMode::SinglePlayer;
    localClientId_ = 0;
    remotePlayers_.clear();
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
}

void Application::beginClientJoinLoad()
{
    if (gameScreen_ != GameScreen::MainMenu || singleplayerLoadState_.active)
    {
        return;
    }

    clientJoinLoadDebugFrame_ = 0;
    clientJoinLoggedFirstChunkSummary_ = false;
    clientJoinAuthoritativeSnapLogsRemaining_ = 8;
    logMultiplayerJoinDiag(
        "beginClientJoinLoad -> {}:{} (local world cleared, awaiting snapshots + authoritative spawn)",
        multiplayerAddress_,
        multiplayerPort_);

    pendingHostStartAfterWorldLoad_ = false;
    pendingClientJoinAfterWorldLoad_ = true;
    singleplayerLoadState_.active = true;
    singleplayerLoadState_.worldPrepared = false;
    singleplayerLoadState_.progress = 0.02f;
    singleplayerLoadState_.label = "Connecting to host...";

    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
    mainMenuSoundSettingsOpen_ = false;
    mainMenuNotice_.clear();
    window_.setTextInputActive(false);
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();
}

void Application::sendInitialWorldToClient(const std::uint16_t clientId)
{
    if (hostSession_ == nullptr)
    {
        return;
    }

    for (const auto& [coord, chunk] : world_.chunks())
    {
        multiplayer::protocol::ChunkSnapshotMessage snapshot{
            .coord = coord,
        };
        const auto& blockStorage = chunk.blockStorage();
        for (std::size_t i = 0; i < blockStorage.size(); ++i)
        {
            snapshot.blocks[i] = static_cast<std::uint8_t>(blockStorage[i]);
        }
        hostSession_->sendChunkSnapshot(clientId, snapshot);
    }
}

void Application::applyChunkSnapshot(const multiplayer::protocol::ChunkSnapshotMessage& chunkMessage)
{
    world::Chunk chunk(chunkMessage.coord);
    auto& storage = chunk.mutableBlockStorage();
    for (std::size_t i = 0; i < storage.size(); ++i)
    {
        storage[i] = static_cast<world::BlockType>(chunkMessage.blocks[i]);
    }
    world_.replaceChunk(std::move(chunk));
}

void Application::applyRemoteBlockEdit(const multiplayer::protocol::BlockEditEventMessage& editMessage)
{
    static_cast<void>(world_.applyEditCommand({
        .action = editMessage.action,
        .position = {editMessage.x, editMessage.y, editMessage.z},
        .blockType = editMessage.blockType,
    }));
}

multiplayer::protocol::ServerSnapshotMessage Application::buildServerSnapshot() const
{
    multiplayer::protocol::ServerSnapshotMessage snapshot;
    snapshot.serverTick = networkServerTick_;
    snapshot.dayNightElapsedSeconds = dayNightCycle_.elapsedSeconds();
    snapshot.weatherElapsedSeconds = weatherSystem_.elapsedSeconds();
    snapshot.players.reserve(remotePlayers_.size() + 1);
    const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
    snapshot.players.push_back(multiplayer::protocol::PlayerSnapshotMessage{
        .clientId = localClientId_,
        .posX = playerFeetPosition_.x,
        .posY = playerFeetPosition_.y,
        .posZ = playerFeetPosition_.z,
        .yawDegrees = camera_.yawDegrees(),
        .pitchDegrees = camera_.pitchDegrees(),
        .health = playerVitals_.health(),
        .air = encodeLegacyNetworkAir(oxygenSystem_.state()),
        .selectedEquippedItem = selectedSlot.equippedItem,
        .selectedBlockType = selectedSlot.blockType,
    });
    for (const RemotePlayerState& remote : remotePlayers_)
    {
        snapshot.players.push_back(multiplayer::protocol::PlayerSnapshotMessage{
            .clientId = remote.clientId,
            .posX = remote.position.x,
            .posY = remote.position.y,
            .posZ = remote.position.z,
            .yawDegrees = remote.yawDegrees,
            .pitchDegrees = remote.pitchDegrees,
            .health = remote.health,
            .air = remote.air,
            .selectedEquippedItem = remote.selectedEquippedItem,
            .selectedBlockType = remote.selectedBlockType,
        });
    }
    snapshot.droppedItems.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        snapshot.droppedItems.push_back(multiplayer::protocol::DroppedItemSnapshotMessage{
            .blockType = droppedItem.equippedItem != EquippedItem::None
                ? world::BlockType::Air
                : droppedItem.blockType,
            .equippedItem = droppedItem.equippedItem,
            .posX = droppedItem.worldPosition.x,
            .posY = droppedItem.worldPosition.y,
            .posZ = droppedItem.worldPosition.z,
            .velocityX = droppedItem.velocity.x,
            .velocityY = droppedItem.velocity.y,
            .velocityZ = droppedItem.velocity.z,
            .ageSeconds = droppedItem.ageSeconds,
            .spinRadians = droppedItem.spinRadians,
        });
    }
    snapshot.mobs.reserve(
        std::min(mobSpawnSystem_.mobs().size(), multiplayer::protocol::kMaxMobsPerSnapshot));
    for (const game::MobInstance& mob : mobSpawnSystem_.mobs())
    {
        if (snapshot.mobs.size() >= multiplayer::protocol::kMaxMobsPerSnapshot)
        {
            break;
        }
        snapshot.mobs.push_back(multiplayer::protocol::MobSnapshotMessage{
            .id = mob.id,
            .kind = mob.kind,
            .feetX = mob.feetX,
            .feetY = mob.feetY,
            .feetZ = mob.feetZ,
            .yawRadians = mob.yawRadians,
            .halfWidth = mob.halfWidth,
            .height = mob.height,
            .health = mob.health,
        });
    }
    return snapshot;
}

glm::vec3 Application::findSafeMultiplayerJoinFeetPosition(const glm::vec3& anchorFeetPosition) const
{
    const float colliderHeight = kPlayerMovementSettings.standingColliderHeight;
    const float minDistance = kPlayerMovementSettings.colliderHalfWidth * 2.0f + 0.35f;
    const float minDistanceSq = minDistance * minDistance;
    const std::array<glm::vec2, 8> ringDirections{
        glm::vec2{1.0f, 0.0f},
        glm::vec2{0.7071f, 0.7071f},
        glm::vec2{0.0f, 1.0f},
        glm::vec2{-0.7071f, 0.7071f},
        glm::vec2{-1.0f, 0.0f},
        glm::vec2{-0.7071f, -0.7071f},
        glm::vec2{0.0f, -1.0f},
        glm::vec2{0.7071f, -0.7071f},
    };

    for (int radiusStep = 1; radiusStep <= 6; ++radiusStep)
    {
        const float radius = 1.8f + static_cast<float>(radiusStep - 1) * 0.9f;
        for (const glm::vec2& direction : ringDirections)
        {
            const glm::vec3 candidateProbe{
                anchorFeetPosition.x + direction.x * radius,
                anchorFeetPosition.y,
                anchorFeetPosition.z + direction.y * radius,
            };
            const glm::vec3 candidateFeet =
                findInitialSpawnFeetPosition(world_, terrainGenerator_, candidateProbe, colliderHeight);
            const glm::vec2 horizontalDelta{
                candidateFeet.x - anchorFeetPosition.x,
                candidateFeet.z - anchorFeetPosition.z,
            };
            if (glm::dot(horizontalDelta, horizontalDelta) < minDistanceSq)
            {
                continue;
            }
            if (game::collidesWithSolidBlock(world_, playerAabbAt(candidateFeet, colliderHeight)))
            {
                continue;
            }
            if (!isSpawnFeetPositionSafe(world_, candidateFeet, colliderHeight))
            {
                continue;
            }
            return candidateFeet;
        }
    }

    const glm::vec3 fallbackProbe{
        anchorFeetPosition.x + 2.2f,
        anchorFeetPosition.y,
        anchorFeetPosition.z,
    };
    glm::vec3 fallbackFeet =
        findInitialSpawnFeetPosition(world_, terrainGenerator_, fallbackProbe, colliderHeight);
    if (isSpawnFeetPositionSafe(world_, fallbackFeet, colliderHeight))
    {
        return fallbackFeet;
    }
    if (const std::optional<glm::vec3> dryFeet = findNearbyDrySpawnFeetPosition(
            world_,
            terrainGenerator_,
            fallbackProbe,
            colliderHeight);
        dryFeet.has_value())
    {
        return *dryFeet;
    }
    return fallbackFeet;
}

void Application::clearClientWorldAwaitingHostChunks()
{
    logMultiplayerJoinDiag("clearClientWorldAwaitingHostChunks (drop resident meshes + empty chunk map)");
    std::vector<std::uint64_t> removedMeshIds(residentChunkMeshIds_.begin(), residentChunkMeshIds_.end());
    if (!removedMeshIds.empty())
    {
        renderer_.updateSceneMeshes({}, removedMeshIds);
    }
    residentChunkMeshIds_.clear();

    vibecraft::world::World::ChunkMap emptyChunks;
    world_.replaceChunks(std::move(emptyChunks));
    droppedItems_.clear();
    mobSpawnSystem_.clearAllMobs();
    activeMiningState_ = {};
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    autoJumpCooldownSeconds_ = 0.0f;
    footstepDistanceAccumulator_ = 0.0f;
    heldItemSwing_ = 0.0f;
}
}  // namespace vibecraft::app
