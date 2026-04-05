#include "vibecraft/app/Application.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::uint32_t hashUint32(const std::int32_t x, const std::int32_t y, const std::int32_t z)
{
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9e3779b9U;
    value ^= static_cast<std::uint32_t>(y) * 0x85ebca6bU + 0x7f4a7c15U;
    value ^= static_cast<std::uint32_t>(z) * 0xc2b2ae35U + 0x165667b1U;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] float hash01(const std::int32_t x, const std::int32_t y, const std::int32_t z)
{
    return static_cast<float>(hashUint32(x, y, z) & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

[[nodiscard]] std::int64_t blockStorageKey(const glm::ivec3& blockPosition)
{
    constexpr std::int64_t offset = 1LL << 20;
    constexpr std::int64_t mask = (1LL << 21) - 1LL;
    const std::int64_t x = (static_cast<std::int64_t>(blockPosition.x) + offset) & mask;
    const std::int64_t y = (static_cast<std::int64_t>(blockPosition.y) + offset) & mask;
    const std::int64_t z = (static_cast<std::int64_t>(blockPosition.z) + offset) & mask;
    return (x << 42) | (y << 21) | z;
}

void broadcastBlockEditIfHosting(
    multiplayer::HostSession* const hostSession,
    const std::uint16_t localClientId,
    const world::WorldEditCommand& command)
{
    if (hostSession == nullptr || !hostSession->running())
    {
        return;
    }
    hostSession->broadcastBlockEdit({
        .authorClientId = localClientId,
        .action = command.action,
        .x = command.position.x,
        .y = command.position.y,
        .z = command.position.z,
        .blockType = command.blockType,
    });
}
}  // namespace

void Application::queuePrimedTnt(const glm::ivec3& blockPosition, const float fuseSeconds)
{
    const float clampedFuse = std::clamp(fuseSeconds, 0.05f, 8.0f);
    auto existingIt = std::find_if(
        primedTntStates_.begin(),
        primedTntStates_.end(),
        [&blockPosition](const PrimedTntState& state)
        {
            return state.blockPosition == blockPosition;
        });
    if (existingIt != primedTntStates_.end())
    {
        existingIt->fuseSeconds = std::min(existingIt->fuseSeconds, clampedFuse);
        return;
    }

    primedTntStates_.push_back(PrimedTntState{
        .blockPosition = blockPosition,
        .centerPosition = glm::vec3(blockPosition) + glm::vec3(0.5f, 0.5f, 0.5f),
        .fuseSeconds = clampedFuse,
    });
}

void Application::igniteTntAtBlock(
    const glm::ivec3& blockPosition,
    const float fuseSeconds,
    const bool broadcastRemove)
{
    if (world_.blockAt(blockPosition.x, blockPosition.y, blockPosition.z) != world::BlockType::TNT)
    {
        return;
    }

    const world::WorldEditCommand removeCommand{
        .action = world::WorldEditAction::Remove,
        .position = blockPosition,
        .blockType = world::BlockType::Air,
    };
    if (!world_.applyEditCommand(removeCommand))
    {
        return;
    }

    if (broadcastRemove)
    {
        broadcastBlockEditIfHosting(hostSession_.get(), localClientId_, removeCommand);
    }
    queuePrimedTnt(blockPosition, fuseSeconds);
    soundEffects_.playBlockPlace(world::BlockType::TNT);
}

void Application::explodeTntAt(const glm::vec3& centerPosition, const float blastRadiusBlocks)
{
    const float radius = std::clamp(blastRadiusBlocks, 1.0f, 8.0f);
    const int ceilRadius = static_cast<int>(std::ceil(radius));
    const glm::ivec3 centerCell{
        static_cast<int>(std::floor(centerPosition.x)),
        static_cast<int>(std::floor(centerPosition.y)),
        static_cast<int>(std::floor(centerPosition.z)),
    };
    std::vector<std::pair<glm::ivec3, float>> chainIgnitions;
    chainIgnitions.reserve(16);
    bool brokeAnyBlocks = false;

    for (int dz = -ceilRadius; dz <= ceilRadius; ++dz)
    {
        for (int dy = -ceilRadius; dy <= ceilRadius; ++dy)
        {
            for (int dx = -ceilRadius; dx <= ceilRadius; ++dx)
            {
                const glm::ivec3 cell = centerCell + glm::ivec3(dx, dy, dz);
                const glm::vec3 sample = glm::vec3(cell) + glm::vec3(0.5f, 0.5f, 0.5f);
                const float distance = glm::distance(sample, centerPosition);
                const float directionalJitter = 0.85f + hash01(cell.x, cell.y, cell.z) * 0.35f;
                if (distance > radius * directionalJitter)
                {
                    continue;
                }

                const world::BlockType blockType = world_.blockAt(cell.x, cell.y, cell.z);
                if (blockType == world::BlockType::Air || !world::blockMetadata(blockType).breakable)
                {
                    continue;
                }

                if (blockType == world::BlockType::TNT)
                {
                    chainIgnitions.push_back(
                        {cell, 0.45f + hash01(cell.x + 17, cell.y + 23, cell.z + 31) * 0.55f});
                    continue;
                }

                const world::WorldEditCommand removeCommand{
                    .action = world::WorldEditAction::Remove,
                    .position = cell,
                    .blockType = world::BlockType::Air,
                };
                if (world_.applyEditCommand(removeCommand))
                {
                    brokeAnyBlocks = true;
                    broadcastBlockEditIfHosting(hostSession_.get(), localClientId_, removeCommand);
                    if (blockType == world::BlockType::Chest)
                    {
                        chestSlotsByPosition_.erase(blockStorageKey(cell));
                    }
                    if (world::isFurnaceBlock(blockType))
                    {
                        furnaceStatesByPosition_.erase(blockStorageKey(cell));
                    }
                    if (world::isDoorVariantBlock(blockType))
                    {
                        const glm::ivec3 counterpartCell =
                            cell + (world::isDoorUpperHalf(blockType) ? glm::ivec3(0, -1, 0) : glm::ivec3(0, 1, 0));
                        const world::BlockType counterpartType =
                            world_.blockAt(counterpartCell.x, counterpartCell.y, counterpartCell.z);
                        if (world::isDoorVariantBlock(counterpartType)
                            && world::doorFamilyForBlockType(counterpartType)
                                == world::doorFamilyForBlockType(blockType))
                        {
                            const world::WorldEditCommand counterpartRemove{
                                .action = world::WorldEditAction::Remove,
                                .position = counterpartCell,
                                .blockType = world::BlockType::Air,
                            };
                            if (world_.applyEditCommand(counterpartRemove))
                            {
                                broadcastBlockEditIfHosting(hostSession_.get(), localClientId_, counterpartRemove);
                            }
                        }
                    }
                }
            }
        }
    }

    for (const auto& [chainCell, fuseSeconds] : chainIgnitions)
    {
        igniteTntAtBlock(chainCell, fuseSeconds, true);
    }

    if (brokeAnyBlocks || !chainIgnitions.empty())
    {
        soundEffects_.playBlockBreak(world::BlockType::TNT);
    }

    const glm::vec3 playerCenter = playerFeetPosition_ + glm::vec3(0.0f, 0.9f, 0.0f);
    const float playerDistance = glm::distance(playerCenter, centerPosition);
    const float damageRadius = radius * 1.9f;
    if (playerDistance < damageRadius && !creativeModeEnabled_)
    {
        const float normalized = std::clamp(1.0f - (playerDistance / damageRadius), 0.0f, 1.0f);
        const float damage = std::ceil(normalized * normalized * 18.0f);
        if (damage > 0.0f
            && playerVitals_.applyDamage({.cause = game::DamageCause::EnemyAttack, .amount = damage}) > 0.0f)
        {
            soundEffects_.playPlayerHurt();
        }
    }
}

void Application::tickPrimedTnt(const float deltaTimeSeconds)
{
    if (primedTntStates_.empty() || deltaTimeSeconds <= 0.0f)
    {
        return;
    }

    std::vector<glm::vec3> detonationCenters;
    detonationCenters.reserve(primedTntStates_.size());
    for (PrimedTntState& state : primedTntStates_)
    {
        state.fuseSeconds -= deltaTimeSeconds;
        if (state.fuseSeconds <= 0.0f)
        {
            detonationCenters.push_back(state.centerPosition);
        }
    }

    primedTntStates_.erase(
        std::remove_if(
            primedTntStates_.begin(),
            primedTntStates_.end(),
            [](const PrimedTntState& state)
            {
                return state.fuseSeconds <= 0.0f;
            }),
        primedTntStates_.end());

    for (const glm::vec3& center : detonationCenters)
    {
        explodeTntAt(center, 4.0f);
    }
}
}  // namespace vibecraft::app
