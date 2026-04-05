#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] float normalizeDegrees(float degrees)
{
    while (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }
    while (degrees < -180.0f)
    {
        degrees += 360.0f;
    }
    return degrees;
}

[[nodiscard]] float lerpDegrees(const float fromDegrees, const float toDegrees, const float t)
{
    return normalizeDegrees(fromDegrees + normalizeDegrees(toDegrees - fromDegrees) * t);
}
}  // namespace

void Application::updateClientMultiplayer(const float deltaTimeSeconds)
{
    if (clientSession_ == nullptr)
    {
        return;
    }

    clientSession_->poll();
    if (const std::optional<multiplayer::protocol::JoinAcceptMessage> accepted = clientSession_->takeJoinAccept();
        accepted.has_value())
    {
        localClientId_ = accepted->clientId;
        world_.setGenerationSeed(accepted->worldSeed);
        terrainGenerator_.setWorldSeed(accepted->worldSeed);
        dayNightCycle_.setElapsedSeconds(accepted->dayNightElapsedSeconds);
        weatherSystem_.setElapsedSeconds(accepted->weatherElapsedSeconds);
        const glm::vec3 authoritativeSpawn{accepted->spawnX, accepted->spawnY, accepted->spawnZ};
        spawnFeetPosition_ = authoritativeSpawn;
        teleportPlayerToFeetPosition(authoritativeSpawn);
        logMultiplayerJoinDiag(
            "client: join accepted id {} spawn ({:.2f},{:.2f},{:.2f}) worldSeed {}",
            accepted->clientId,
            authoritativeSpawn.x,
            authoritativeSpawn.y,
            authoritativeSpawn.z,
            accepted->worldSeed);
    }

    for (const multiplayer::protocol::ChunkSnapshotMessage& chunk : clientSession_->takeChunkSnapshots())
    {
        applyChunkSnapshot(chunk);
    }

    for (const multiplayer::protocol::BlockEditEventMessage& edit : clientSession_->takeBlockEdits())
    {
        applyRemoteBlockEdit(edit);
    }

    for (const multiplayer::protocol::CommandFeedbackMessage& feedback : clientSession_->takeCommandFeedback())
    {
        appendChatLine(feedback.feedback, feedback.isError);
    }

    const std::vector<multiplayer::protocol::ServerSnapshotMessage> snapshots = clientSession_->takeSnapshots();
    if (!snapshots.empty())
    {
        const multiplayer::protocol::ServerSnapshotMessage& latest = snapshots.back();
        dayNightCycle_.setElapsedSeconds(latest.dayNightElapsedSeconds);
        weatherSystem_.setElapsedSeconds(latest.weatherElapsedSeconds);
        droppedItems_.clear();
        droppedItems_.reserve(latest.droppedItems.size());
        for (const multiplayer::protocol::DroppedItemSnapshotMessage& droppedItem : latest.droppedItems)
        {
            droppedItems_.push_back(DroppedItem{
                .blockType = droppedItem.blockType,
                .equippedItem = droppedItem.equippedItem,
                .worldPosition = {droppedItem.posX, droppedItem.posY, droppedItem.posZ},
                .velocity = {droppedItem.velocityX, droppedItem.velocityY, droppedItem.velocityZ},
                .ageSeconds = droppedItem.ageSeconds,
                .pickupDelaySeconds = 0.1f,
                .spinRadians = droppedItem.spinRadians,
            });
        }
        const std::vector<game::MobInstance> previousClientMobs = clientReplicatedMobs_;
        clientReplicatedMobs_.clear();
        clientReplicatedMobs_.reserve(latest.mobs.size());
        for (const multiplayer::protocol::MobSnapshotMessage& mobSnap : latest.mobs)
        {
            game::MobInstance mob{};
            mob.id = mobSnap.id;
            mob.kind = mobSnap.kind;
            mob.feetX = mobSnap.feetX;
            mob.feetY = mobSnap.feetY;
            mob.feetZ = mobSnap.feetZ;
            mob.yawRadians = mobSnap.yawRadians;
            mob.halfWidth = mobSnap.halfWidth;
            mob.height = mobSnap.height;
            mob.health = mobSnap.health;
            clientReplicatedMobs_.push_back(mob);
        }
        if (clientSession_->lastServerSnapshotProtocolVersion() >= 6 && !previousClientMobs.empty())
        {
            for (const game::MobInstance& prev : previousClientMobs)
            {
                const auto it = std::find_if(
                    clientReplicatedMobs_.begin(),
                    clientReplicatedMobs_.end(),
                    [id = prev.id](const game::MobInstance& mob)
                    {
                        return mob.id == id;
                    });
                if (it != clientReplicatedMobs_.end())
                {
                    if (it->health < prev.health - 0.05f)
                    {
                        soundEffects_.playMobHit(it->kind);
                    }
                }
                else if (prev.health > 0.05f)
                {
                    const float maxH = game::mobKindDefaultMaxHealth(prev.kind);
                    const bool recentSwingOnMob =
                        prev.id == lastClientMeleeSwingMobId_
                        && (sessionPlayTimeSeconds_ - lastClientMeleeSwingSessionTimeSeconds_) < 0.75f;
                    const bool likelyKillFromDamage = prev.health < maxH * 0.38f;
                    if (recentSwingOnMob || likelyKillFromDamage)
                    {
                        soundEffects_.playMobDefeat(prev.kind);
                    }
                }
            }
        }
        std::vector<RemotePlayerState> updatedRemotePlayers;
        updatedRemotePlayers.reserve(latest.players.size());
        for (const multiplayer::protocol::PlayerSnapshotMessage& player : latest.players)
        {
            if (player.clientId == localClientId_)
            {
                const glm::vec3 authoritativePosition{player.posX, player.posY, player.posZ};
                if (pendingClientJoinAfterWorldLoad_)
                {
                    if (clientJoinAuthoritativeSnapLogsRemaining_ > 0)
                    {
                        --clientJoinAuthoritativeSnapLogsRemaining_;
                        const world::ChunkCoord authChunk = world::worldToChunkCoord(
                            static_cast<int>(std::floor(authoritativePosition.x)),
                            static_cast<int>(std::floor(authoritativePosition.z)));
                        logMultiplayerJoinDiag(
                            "client: authoritative snap during join ({} left) feet ({:.2f},{:.2f},{:.2f}) chunk "
                            "({},{}) yaw {:.1f} pitch {:.1f} worldChunks {} tick {}",
                            static_cast<int>(clientJoinAuthoritativeSnapLogsRemaining_),
                            authoritativePosition.x,
                            authoritativePosition.y,
                            authoritativePosition.z,
                            authChunk.x,
                            authChunk.z,
                            player.yawDegrees,
                            player.pitchDegrees,
                            world_.chunks().size(),
                            latest.serverTick);
                    }
                    teleportPlayerToFeetPosition(authoritativePosition);
                    spawnFeetPosition_ = authoritativePosition;
                    camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                }
                else
                {
                    const glm::vec3 authoritativeDelta = authoritativePosition - playerFeetPosition_;
                    if (glm::dot(authoritativeDelta, authoritativeDelta) > 4.0f)
                    {
                        teleportPlayerToFeetPosition(authoritativePosition);
                        camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                    }
                }
                playerVitals_.setHealthAndAir(player.health, player.air);
                camera_.setPosition(
                    playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
                continue;
            }
            const auto previousIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&player](const RemotePlayerState& state)
                {
                    return state.clientId == player.clientId;
                });
            const glm::vec3 targetPosition{player.posX, player.posY, player.posZ};
            const glm::vec3 smoothedPosition = previousIt == remotePlayers_.end()
                ? targetPosition
                : glm::mix(previousIt->position, targetPosition, 0.35f);
            const float smoothedYawDegrees = previousIt == remotePlayers_.end()
                ? normalizeDegrees(player.yawDegrees)
                : lerpDegrees(previousIt->yawDegrees, player.yawDegrees, 0.35f);
            const float smoothedPitchDegrees = previousIt == remotePlayers_.end()
                ? std::clamp(player.pitchDegrees, -89.0f, 89.0f)
                : std::clamp(
                    previousIt->pitchDegrees + (player.pitchDegrees - previousIt->pitchDegrees) * 0.35f,
                    -89.0f,
                    89.0f);
            updatedRemotePlayers.push_back(RemotePlayerState{
                .clientId = player.clientId,
                .position = smoothedPosition,
                .yawDegrees = smoothedYawDegrees,
                .pitchDegrees = smoothedPitchDegrees,
                .health = player.health,
                .air = player.air,
                .selectedBlockType = player.selectedBlockType,
                .selectedEquippedItem = player.selectedEquippedItem,
            });
        }
        remotePlayers_ = std::move(updatedRemotePlayers);
    }

    if (clientSession_->connected())
    {
        if (!pendingClientJoinAfterWorldLoad_)
        {
            clientSession_->sendInput(
                {
                    .clientId = localClientId_,
                    .dtSeconds = deltaTimeSeconds,
                    .positionX = playerFeetPosition_.x,
                    .positionY = playerFeetPosition_.y,
                    .positionZ = playerFeetPosition_.z,
                    .yawDelta = camera_.yawDegrees(),
                    .pitchDelta = camera_.pitchDegrees(),
                    .health = playerVitals_.health(),
                    .air = playerVitals_.air(),
                    .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                    .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                    .mobMeleeSwing = pendingClientMobMeleeSwing_,
                    .isSneaking = inputState_.isKeyDown(SDL_SCANCODE_LSHIFT),
                    .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                    .mobMeleeTargetId = pendingClientMobMeleeTargetId_,
                    .cameraEyeY = camera_.position().y,
                },
                networkServerTick_);
            pendingClientMobMeleeSwing_ = false;
            pendingClientMobMeleeTargetId_ = 0;
        }
        multiplayerStatusLine_ = fmt::format("client id {} -> {}:{}", localClientId_, multiplayerAddress_, multiplayerPort_);
    }
    else if (clientSession_->connecting())
    {
        multiplayerStatusLine_ = fmt::format("connecting {}:{}...", multiplayerAddress_, multiplayerPort_);
    }
    else if (!clientSession_->lastError().empty())
    {
        multiplayerStatusLine_ = "client error: " + clientSession_->lastError();
    }
}
}  // namespace vibecraft::app
