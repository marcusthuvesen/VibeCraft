#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"

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

void Application::updateMultiplayer(const float deltaTimeSeconds)
{
    networkTickAccumulatorSeconds_ += deltaTimeSeconds;
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        clientReplicatedMobs_.clear();
    }

    if (hostSession_ != nullptr && hostSession_->running())
    {
        hostSession_->poll();
        std::unordered_set<std::uint16_t> activeClientIds;
        activeClientIds.reserve(hostSession_->clients().size());
        const auto rebuildChunkSyncList = [this](const std::uint16_t clientId, const world::ChunkCoord& centerChunk)
        {
            std::vector<world::ChunkCoord> coords;
            const int radius = kStreamingSettings.generationChunkRadius;
            coords.reserve(static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1)));
            for (int chunkZ = centerChunk.z - radius; chunkZ <= centerChunk.z + radius; ++chunkZ)
            {
                for (int chunkX = centerChunk.x - radius; chunkX <= centerChunk.x + radius; ++chunkX)
                {
                    const world::ChunkCoord coord{chunkX, chunkZ};
                    if (world_.chunks().contains(coord))
                    {
                        coords.push_back(coord);
                    }
                }
            }
            std::sort(
                coords.begin(),
                coords.end(),
                [&centerChunk](const world::ChunkCoord& lhs, const world::ChunkCoord& rhs)
                {
                    const int lhsDx = lhs.x - centerChunk.x;
                    const int lhsDz = lhs.z - centerChunk.z;
                    const int rhsDx = rhs.x - centerChunk.x;
                    const int rhsDz = rhs.z - centerChunk.z;
                    const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
                    const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
                    if (lhsDistanceSq != rhsDistanceSq)
                    {
                        return lhsDistanceSq < rhsDistanceSq;
                    }
                    if (lhs.z != rhs.z)
                    {
                        return lhs.z < rhs.z;
                    }
                    return lhs.x < rhs.x;
                });
            clientChunkSyncCoordsById_[clientId] = std::move(coords);
            clientChunkSyncCursorById_[clientId] = 0;
            clientChunkSyncCenterById_[clientId] = centerChunk;
        };

        constexpr std::size_t kChunkSnapshotsPerClientPerFrame = 2;
        for (const multiplayer::ConnectedClient& client : hostSession_->clients())
        {
            activeClientIds.insert(client.clientId);
            auto remotePlayerIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&client](const RemotePlayerState& remote)
                {
                    return remote.clientId == client.clientId;
                });
            if (remotePlayerIt == remotePlayers_.end())
            {
                const glm::vec3 spawnFeetPosition = findSafeMultiplayerJoinFeetPosition(playerFeetPosition_);
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = client.clientId,
                    .position = spawnFeetPosition,
                    .yawDegrees = camera_.yawDegrees(),
                    .pitchDegrees = camera_.pitchDegrees(),
                    .health = 20.0f,
                    .air = 10.0f,
                    .selectedBlockType = world::BlockType::Air,
                    .selectedEquippedItem = EquippedItem::None,
                });
                remotePlayerIt = std::prev(remotePlayers_.end());
                const world::ChunkCoord spawnChunk = world::worldToChunkCoord(
                    static_cast<int>(std::floor(spawnFeetPosition.x)),
                    static_cast<int>(std::floor(spawnFeetPosition.z)));
                logMultiplayerJoinDiag(
                    "host: new client id {} spawn feet ({:.2f},{:.2f},{:.2f}) chunk ({},{})  host feet ({:.2f},{:.2f},{:.2f})  "
                    "chunks in host world {}",
                    client.clientId,
                    spawnFeetPosition.x,
                    spawnFeetPosition.y,
                    spawnFeetPosition.z,
                    spawnChunk.x,
                    spawnChunk.z,
                    playerFeetPosition_.x,
                    playerFeetPosition_.y,
                    playerFeetPosition_.z,
                    world_.chunks().size());
            }
            const world::ChunkCoord centerChunk = remotePlayerIt != remotePlayers_.end()
                ? world::worldToChunkCoord(
                    static_cast<int>(std::floor(remotePlayerIt->position.x)),
                    static_cast<int>(std::floor(remotePlayerIt->position.z)))
                : world::worldToChunkCoord(
                    static_cast<int>(std::floor(playerFeetPosition_.x)),
                    static_cast<int>(std::floor(playerFeetPosition_.z)));
            if (!clientChunkSyncCenterById_.contains(client.clientId)
                || !(clientChunkSyncCenterById_.at(client.clientId) == centerChunk))
            {
                rebuildChunkSyncList(client.clientId, centerChunk);
            }

            const auto coordsIt = clientChunkSyncCoordsById_.find(client.clientId);
            if (coordsIt == clientChunkSyncCoordsById_.end() || coordsIt->second.empty())
            {
                continue;
            }

            std::size_t& cursor = clientChunkSyncCursorById_[client.clientId];
            const std::vector<world::ChunkCoord>& coords = coordsIt->second;
            for (std::size_t i = 0; i < kChunkSnapshotsPerClientPerFrame; ++i)
            {
                if (cursor >= coords.size())
                {
                    cursor = 0;
                }
                const world::ChunkCoord coord = coords[cursor++];
                const auto chunkIt = world_.chunks().find(coord);
                if (chunkIt == world_.chunks().end())
                {
                    continue;
                }
                multiplayer::protocol::ChunkSnapshotMessage snapshot{
                    .coord = coord,
                };
                const auto& blockStorage = chunkIt->second.blockStorage();
                for (std::size_t blockIndex = 0; blockIndex < blockStorage.size(); ++blockIndex)
                {
                    snapshot.blocks[blockIndex] = static_cast<std::uint8_t>(blockStorage[blockIndex]);
                }
                hostSession_->sendChunkSnapshot(client.clientId, snapshot);
            }
        }

        const std::vector<multiplayer::protocol::ClientInputMessage> inputs = hostSession_->takePendingInputs();
        for (const multiplayer::protocol::ClientInputMessage& input : inputs)
        {
            auto remotePlayerIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&input](const RemotePlayerState& state)
                {
                    return state.clientId == input.clientId;
                });
            if (remotePlayerIt == remotePlayers_.end())
            {
                const glm::vec3 spawnFeetPosition = findSafeMultiplayerJoinFeetPosition(playerFeetPosition_);
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = input.clientId,
                    .position = spawnFeetPosition,
                    .yawDegrees = camera_.yawDegrees(),
                    .pitchDegrees = input.pitchDelta,
                    .health = input.health,
                    .air = input.air,
                    .selectedBlockType = input.selectedBlockType,
                    .selectedEquippedItem = input.selectedEquippedItem,
                });
                remotePlayerIt = std::prev(remotePlayers_.end());
            }
            remotePlayerIt->position = {input.positionX, input.positionY, input.positionZ};
            remotePlayerIt->yawDegrees = input.yawDelta;
            remotePlayerIt->pitchDegrees = input.pitchDelta;
            remotePlayerIt->health = input.health;
            remotePlayerIt->air = input.air;
            remotePlayerIt->selectedBlockType = input.selectedBlockType;
            remotePlayerIt->selectedEquippedItem = input.selectedEquippedItem;

            if (input.breakBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Remove,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = world::BlockType::Air,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
            else if (input.placeBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Place,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = input.placeBlockType,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
            else if (input.mobMeleeSwing && input.mobMeleeTargetId != 0)
            {
                const glm::vec3 feet{input.positionX, input.positionY, input.positionZ};
                const float eyeHeight = input.isSneaking ? kPlayerMovementSettings.sneakingEyeHeight
                                                         : kPlayerMovementSettings.standingEyeHeight;
                const bool useClientCameraY = input.cameraEyeY > input.positionY + 0.35f
                    && input.cameraEyeY < input.positionY + 2.7f;
                const glm::vec3 rayOrigin = useClientCameraY
                    ? glm::vec3(input.positionX, input.cameraEyeY, input.positionZ)
                    : feet + glm::vec3(0.0f, eyeHeight, 0.0f);
                const glm::vec3 rayDir =
                    game::Camera::forwardFromYawPitchDegrees(input.yawDelta, input.pitchDelta);
                InventorySlot selectedSlot{};
                selectedSlot.equippedItem = input.selectedEquippedItem;
                selectedSlot.blockType = input.selectedBlockType;
                selectedSlot.count = 1;
                const float reach = meleeReachForSlot(selectedSlot);
                const std::optional<std::size_t> closestOpt =
                    game::findClosestMobIndexAlongRay(mobSpawnSystem_.mobs(), rayOrigin, rayDir, reach);
                if (!closestOpt.has_value())
                {
                    continue;
                }
                if (mobSpawnSystem_.mobs()[*closestOpt].id != input.mobMeleeTargetId)
                {
                    continue;
                }
                if (const std::optional<game::MobDamageResult> mobDamage = mobSpawnSystem_.damageMobAtIndex(
                        world_,
                        *closestOpt,
                        meleeDamageForSlot(selectedSlot),
                        feet,
                        rayDir,
                        knockbackDistanceForSlot(selectedSlot));
                    mobDamage.has_value())
                {
                    soundEffects_.playPlayerAttack();
                    if (mobDamage->killed)
                    {
                        soundEffects_.playMobDefeat(mobDamage->mobKind);
                        spawnDroppedItemAtPosition(
                            mobDropItemForKind(mobDamage->mobKind),
                            mobDamage->feetPosition + glm::vec3(0.0f, 0.35f, 0.0f));
                    }
                    else
                    {
                        soundEffects_.playMobHit(mobDamage->mobKind);
                    }
                }
            }
        }

        while (networkTickAccumulatorSeconds_ >= kNetworkTickSeconds)
        {
            networkTickAccumulatorSeconds_ -= kNetworkTickSeconds;
            ++networkServerTick_;
            hostSession_->broadcastSnapshot(buildServerSnapshot());
        }

        multiplayerStatusLine_ =
            fmt::format("host {} client(s) @{}", hostSession_->clients().size(), multiplayerPort_);

        for (auto it = clientChunkSyncCoordsById_.begin(); it != clientChunkSyncCoordsById_.end();)
        {
            if (activeClientIds.contains(it->first))
            {
                ++it;
            }
            else
            {
                clientChunkSyncCursorById_.erase(it->first);
                clientChunkSyncCenterById_.erase(it->first);
                it = clientChunkSyncCoordsById_.erase(it);
            }
        }
        remotePlayers_.erase(
            std::remove_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&activeClientIds](const RemotePlayerState& state)
                {
                    return !activeClientIds.contains(state.clientId);
                }),
            remotePlayers_.end());
    }

    if (clientSession_ != nullptr)
    {
        clientSession_->poll();
        if (const std::optional<multiplayer::protocol::JoinAcceptMessage> accepted = clientSession_->takeJoinAccept();
            accepted.has_value())
        {
            localClientId_ = accepted->clientId;
            dayNightCycle_.setElapsedSeconds(accepted->dayNightElapsedSeconds);
            weatherSystem_.setElapsedSeconds(accepted->weatherElapsedSeconds);
        }

        for (const multiplayer::protocol::ChunkSnapshotMessage& chunk : clientSession_->takeChunkSnapshots())
        {
            applyChunkSnapshot(chunk);
        }

        for (const multiplayer::protocol::BlockEditEventMessage& edit : clientSession_->takeBlockEdits())
        {
            applyRemoteBlockEdit(edit);
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
                        // During join bootstrap, snap to the host-authoritative spawn so chunk loading
                        // targets the same area the host chose for this client.
                        playerFeetPosition_ = authoritativePosition;
                        spawnFeetPosition_ = authoritativePosition;
                        verticalVelocity_ = 0.0f;
                        accumulatedFallDistance_ = 0.0f;
                        isGrounded_ = isGroundedAtFeetPosition(
                            world_,
                            playerFeetPosition_,
                            kPlayerMovementSettings.standingColliderHeight);
                        playerHazards_ = samplePlayerHazards(
                            world_,
                            playerFeetPosition_,
                            kPlayerMovementSettings.standingColliderHeight,
                            kPlayerMovementSettings.standingEyeHeight);
                        camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                    }
                    else
                    {
                        // After join bootstrap the client is authoritative for its own movement,
                        // so the host snapshot is effectively an echo of our earlier input.
                        // Pulling toward it every frame adds visible lag and weakens jumping.
                        const glm::vec3 authoritativeDelta = authoritativePosition - playerFeetPosition_;
                        if (glm::dot(authoritativeDelta, authoritativeDelta) > 4.0f)
                        {
                            playerFeetPosition_ = authoritativePosition;
                            verticalVelocity_ = 0.0f;
                            accumulatedFallDistance_ = 0.0f;
                            isGrounded_ = isGroundedAtFeetPosition(
                                world_,
                                playerFeetPosition_,
                                kPlayerMovementSettings.standingColliderHeight);
                            playerHazards_ = samplePlayerHazards(
                                world_,
                                playerFeetPosition_,
                                kPlayerMovementSettings.standingColliderHeight,
                                kPlayerMovementSettings.standingEyeHeight);
                            camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                        }
                    }
                    applyLegacyNetworkAirToOxygenSystem(oxygenSystem_, player.air);
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
                        .air = encodeLegacyNetworkAir(oxygenSystem_.state()),
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
}
}  // namespace vibecraft::app
