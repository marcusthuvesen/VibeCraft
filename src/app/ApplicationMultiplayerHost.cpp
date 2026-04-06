#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"
#include "vibecraft/multiplayer/WorldSync.hpp"

namespace vibecraft::app
{
void Application::acceptPendingHostClient(const multiplayer::PendingClientJoin& join)
{
    if (hostSession_ == nullptr)
    {
        return;
    }

    auto remotePlayerIt = std::find_if(
        remotePlayers_.begin(),
        remotePlayers_.end(),
        [&join](const RemotePlayerState& remote)
        {
            return remote.clientId == join.clientId;
        });
    if (remotePlayerIt == remotePlayers_.end())
    {
        const glm::vec3 spawnFeetPosition = findSafeMultiplayerJoinFeetPosition(playerFeetPosition_);
        remotePlayers_.push_back(RemotePlayerState{
            .clientId = join.clientId,
            .position = spawnFeetPosition,
            .yawDegrees = camera_.yawDegrees(),
            .pitchDegrees = camera_.pitchDegrees(),
            .health = 20.0f,
            .air = 10.0f,
            .selectedBlockType = world::BlockType::Air,
            .selectedEquippedItem = EquippedItem::None,
        });
        remotePlayerIt = std::prev(remotePlayers_.end());
    }

    const glm::vec3 spawnFeetPosition = remotePlayerIt->position;
    hostSession_->acceptPendingJoin(
        join.clientId,
        multiplayer::protocol::JoinAcceptMessage{
            .clientId = join.clientId,
            .worldSeed = world_.generationSeed(),
            .spawnX = spawnFeetPosition.x,
            .spawnY = spawnFeetPosition.y,
            .spawnZ = spawnFeetPosition.z,
            .dayNightElapsedSeconds = dayNightCycle_.elapsedSeconds(),
            .weatherElapsedSeconds = weatherSystem_.elapsedSeconds(),
        });

    const world::ChunkCoord spawnChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(spawnFeetPosition.x)),
        static_cast<int>(std::floor(spawnFeetPosition.z)));
    rebuildClientChunkSyncList(join.clientId, spawnChunk);
    sendInitialWorldToClient(join.clientId);

    logMultiplayerJoinDiag(
        "host: accepted client id {} ({}) spawn feet ({:.2f},{:.2f},{:.2f}) chunk ({},{}) host seed {} chunks {}",
        join.clientId,
        join.playerName,
        spawnFeetPosition.x,
        spawnFeetPosition.y,
        spawnFeetPosition.z,
        spawnChunk.x,
        spawnChunk.z,
        world_.generationSeed(),
        world_.chunks().size());
}

void Application::rebuildClientChunkSyncList(const std::uint16_t clientId, const world::ChunkCoord& centerChunk)
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
}

void Application::sendChunkSnapshotsToClient(const std::uint16_t clientId, const std::size_t maxChunkSnapshots)
{
    if (hostSession_ == nullptr)
    {
        return;
    }

    const auto coordsIt = clientChunkSyncCoordsById_.find(clientId);
    if (coordsIt == clientChunkSyncCoordsById_.end() || coordsIt->second.empty())
    {
        return;
    }

    std::size_t& cursor = clientChunkSyncCursorById_[clientId];
    const std::vector<world::ChunkCoord>& coords = coordsIt->second;
    for (std::size_t snapshotIndex = 0; snapshotIndex < maxChunkSnapshots; ++snapshotIndex)
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
        hostSession_->sendChunkSnapshot(clientId, multiplayer::buildChunkSnapshot(chunkIt->second));
    }
}

void Application::updateHostMultiplayer(const float deltaTimeSeconds)
{
    static_cast<void>(deltaTimeSeconds);
    if (hostSession_ == nullptr || !hostSession_->running())
    {
        return;
    }

    hostSession_->poll();
    for (const multiplayer::PendingClientJoin& pendingJoin : hostSession_->takePendingJoins())
    {
        acceptPendingHostClient(pendingJoin);
    }

    std::unordered_set<std::uint16_t> activeClientIds;
    activeClientIds.reserve(hostSession_->clients().size());
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
        }

        const world::ChunkCoord centerChunk = world::worldToChunkCoord(
            static_cast<int>(std::floor(remotePlayerIt->position.x)),
            static_cast<int>(std::floor(remotePlayerIt->position.z)));
        if (!clientChunkSyncCenterById_.contains(client.clientId)
            || !(clientChunkSyncCenterById_.at(client.clientId) == centerChunk))
        {
            rebuildClientChunkSyncList(client.clientId, centerChunk);
        }
        sendChunkSnapshotsToClient(client.clientId, kChunkSnapshotsPerClientPerFrame);
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
            const bool useClientCameraY =
                input.cameraEyeY > input.positionY + 0.35f && input.cameraEyeY < input.positionY + 2.7f;
            const glm::vec3 rayOrigin = useClientCameraY
                ? glm::vec3(input.positionX, input.cameraEyeY, input.positionZ)
                : feet + glm::vec3(0.0f, eyeHeight, 0.0f);
            const glm::vec3 rayDir = game::Camera::forwardFromYawPitchDegrees(input.yawDelta, input.pitchDelta);
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
                    spawnMobKillDrops(mobDamage->mobKind, mobDamage->feetPosition);
                }
                else
                {
                    soundEffects_.playMobHit(mobDamage->mobKind);
                }
            }
        }
    }

    const std::vector<multiplayer::protocol::CommandRequestMessage> commandRequests =
        hostSession_->takePendingCommandRequests();
    for (const multiplayer::protocol::CommandRequestMessage& request : commandRequests)
    {
        handleHostRequestedCommand(request.clientId, request.commandText);
    }

    while (networkTickAccumulatorSeconds_ >= kNetworkTickSeconds)
    {
        networkTickAccumulatorSeconds_ -= kNetworkTickSeconds;
        ++networkServerTick_;
        hostSession_->broadcastSnapshot(buildServerSnapshot());
    }

    multiplayerStatusLine_ = fmt::format("host {} client(s) @{}", hostSession_->clients().size(), multiplayerPort_);

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
}  // namespace vibecraft::app
