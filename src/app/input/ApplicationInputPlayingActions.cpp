#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

#include "vibecraft/app/ApplicationBotanyRuntime.hpp"
#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/Mining.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::int64_t blockStorageKey(const glm::ivec3& blockPosition)
{
    constexpr std::int64_t offset = 1LL << 20;
    constexpr std::int64_t mask = (1LL << 21) - 1LL;
    const std::int64_t x = (static_cast<std::int64_t>(blockPosition.x) + offset) & mask;
    const std::int64_t y = (static_cast<std::int64_t>(blockPosition.y) + offset) & mask;
    const std::int64_t z = (static_cast<std::int64_t>(blockPosition.z) + offset) & mask;
    return (x << 42) | (y << 21) | z;
}

[[nodiscard]] world::BlockType furnaceBlockFacingPlayer(const glm::vec3& cameraForward)
{
    if (std::abs(cameraForward.x) >= std::abs(cameraForward.z))
    {
        return cameraForward.x >= 0.0f ? world::BlockType::FurnaceWest : world::BlockType::FurnaceEast;
    }
    return cameraForward.z >= 0.0f ? world::BlockType::FurnaceNorth : world::BlockType::Furnace;
}
}  // namespace

void Application::processPlayingActionInput(const float deltaTimeSeconds)
{
    bool attackedMobThisFrame = false;
    if (inputState_.leftMousePressed)
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (multiplayerMode_ == MultiplayerRuntimeMode::Client && clientSession_ != nullptr
            && clientSession_->connected())
        {
            pendingClientMobMeleeSwing_ = false;
            pendingClientMobMeleeTargetId_ = 0;
            if (const std::optional<std::size_t> mobIndex = game::findClosestMobIndexAlongRay(
                    clientReplicatedMobs_,
                    camera_.position(),
                    camera_.forward(),
                    meleeReachForSlot(selectedSlot));
                mobIndex.has_value())
            {
                pendingClientMobMeleeSwing_ = true;
                pendingClientMobMeleeTargetId_ = clientReplicatedMobs_[*mobIndex].id;
                lastClientMeleeSwingMobId_ = pendingClientMobMeleeTargetId_;
                lastClientMeleeSwingSessionTimeSeconds_ = sessionPlayTimeSeconds_;
                attackedMobThisFrame = true;
                activeMiningState_.active = false;
                activeMiningState_.elapsedSeconds = 0.0f;
                soundEffects_.playPlayerAttack();
            }
        }
        else if (const std::optional<game::MobDamageResult> mobDamage = mobSpawnSystem_.damageClosestAlongRay(
                     world_,
                     camera_.position(),
                     camera_.forward(),
                     meleeReachForSlot(selectedSlot),
                     meleeDamageForSlot(selectedSlot),
                     playerFeetPosition_,
                     knockbackDistanceForSlot(selectedSlot));
                 mobDamage.has_value())
        {
            attackedMobThisFrame = true;
            activeMiningState_.active = false;
            activeMiningState_.elapsedSeconds = 0.0f;
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

    const auto raycastHit = world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    if (attackedMobThisFrame)
    {
        if (!creativeModeEnabled_)
        {
            consumeEquippedItemDurability(hotbarSlots_[selectedHotbarIndex_]);
        }
        return;
    }
    if (!raycastHit.has_value())
    {
        activeMiningState_.active = false;
        activeMiningState_.elapsedSeconds = 0.0f;
        return;
    }

    const std::uint32_t mouseButtons = SDL_GetMouseState(nullptr, nullptr);
    const bool leftMouseHeld = (mouseButtons & SDL_BUTTON_LMASK) != 0U;
    if (!leftMouseHeld)
    {
        activeMiningState_.active = false;
        activeMiningState_.elapsedSeconds = 0.0f;
    }
    else
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        const world::BlockType equippedBlockType =
            selectedSlot.count == 0 ? world::BlockType::Air : selectedSlot.blockType;
        const EquippedItem equippedItem = selectedSlot.equippedItem;
        const bool targetChanged = !activeMiningState_.active
            || activeMiningState_.targetBlockPosition != raycastHit->solidBlock
            || activeMiningState_.targetBlockType != raycastHit->blockType
            || activeMiningState_.equippedBlockType != equippedBlockType
            || activeMiningState_.equippedItem != equippedItem;
        if (targetChanged)
        {
            activeMiningState_.active = true;
            activeMiningState_.targetBlockPosition = raycastHit->solidBlock;
            activeMiningState_.targetBlockType = raycastHit->blockType;
            activeMiningState_.equippedBlockType = equippedBlockType;
            activeMiningState_.equippedItem = equippedItem;
            activeMiningState_.elapsedSeconds = 0.0f;
            activeMiningState_.requiredSeconds = creativeModeEnabled_
                ? 0.0f
                : miningDurationSeconds(raycastHit->blockType, equippedBlockType, equippedItem);
            soundEffects_.playBlockDigTick(raycastHit->blockType);
            activeMiningState_.digSoundCooldownSeconds = 0.11f;
        }
        else
        {
            activeMiningState_.digSoundCooldownSeconds -= deltaTimeSeconds;
            if (activeMiningState_.digSoundCooldownSeconds <= 0.0f)
            {
                soundEffects_.playBlockDigTick(raycastHit->blockType);
                activeMiningState_.digSoundCooldownSeconds = 0.11f;
            }
        }
        activeMiningState_.elapsedSeconds += deltaTimeSeconds;

        if (activeMiningState_.elapsedSeconds >= activeMiningState_.requiredSeconds)
        {
            const world::WorldEditCommand command{
                .action = world::WorldEditAction::Remove,
                .position = raycastHit->solidBlock,
                .blockType = world::BlockType::Air,
            };
            if (world_.applyEditCommand(command))
            {
                if (raycastHit->blockType == world::BlockType::Chest)
                {
                    const auto chestIt = chestSlotsByPosition_.find(blockStorageKey(raycastHit->solidBlock));
                    if (chestIt != chestSlotsByPosition_.end())
                    {
                        for (const InventorySlot& slot : chestIt->second)
                        {
                            for (std::uint32_t i = 0; i < slot.count; ++i)
                            {
                                if (!creativeModeEnabled_ && slot.equippedItem != EquippedItem::None)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.equippedItem,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                                else if (!creativeModeEnabled_ && slot.blockType != world::BlockType::Air)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.blockType,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                            }
                        }
                        chestSlotsByPosition_.erase(chestIt);
                    }
                }
                if (world::isFurnaceBlock(raycastHit->blockType))
                {
                    const auto furnaceIt = furnaceStatesByPosition_.find(blockStorageKey(raycastHit->solidBlock));
                    if (furnaceIt != furnaceStatesByPosition_.end())
                    {
                        const std::array<InventorySlot, 3> furnaceSlots{
                            furnaceIt->second.inputSlot,
                            furnaceIt->second.fuelSlot,
                            furnaceIt->second.outputSlot,
                        };
                        for (const InventorySlot& slot : furnaceSlots)
                        {
                            for (std::uint32_t i = 0; i < slot.count; ++i)
                            {
                                if (!creativeModeEnabled_ && slot.equippedItem != EquippedItem::None)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.equippedItem,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                                else if (!creativeModeEnabled_ && slot.blockType != world::BlockType::Air)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.blockType,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                            }
                        }
                        furnaceStatesByPosition_.erase(furnaceIt);
                    }
                }
                if (!creativeModeEnabled_)
                {
                    consumeEquippedItemDurability(hotbarSlots_[selectedHotbarIndex_]);
                    spawnDroppedItem(
                        world::normalizeFurnaceBlockType(raycastHit->blockType),
                        raycastHit->solidBlock);
                }
                soundEffects_.playBlockBreak(raycastHit->blockType);
                if (hostSession_ != nullptr && hostSession_->running())
                {
                    hostSession_->broadcastBlockEdit({
                        .authorClientId = localClientId_,
                        .action = command.action,
                        .x = command.position.x,
                        .y = command.position.y,
                        .z = command.position.z,
                        .blockType = command.blockType,
                    });
                }
                if (clientSession_ != nullptr && clientSession_->connected())
                {
                    clientSession_->sendInput(
                        {
                            .clientId = clientSession_->clientId(),
                            .positionX = playerFeetPosition_.x,
                            .positionY = playerFeetPosition_.y,
                            .positionZ = playerFeetPosition_.z,
                            .yawDelta = camera_.yawDegrees(),
                            .pitchDelta = camera_.pitchDegrees(),
                            .health = playerVitals_.health(),
                            .air = playerVitals_.air(),
                            .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                            .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                            .breakBlock = true,
                            .targetX = command.position.x,
                            .targetY = command.position.y,
                            .targetZ = command.position.z,
                        },
                        networkServerTick_);
                }
            }
            activeMiningState_.active = false;
            activeMiningState_.elapsedSeconds = 0.0f;
        }
    }

    if (!inputState_.rightMousePressed)
    {
        return;
    }
    if (raycastHit->blockType == world::BlockType::CraftingTable)
    {
        openCraftingMenu(true, raycastHit->solidBlock);
        return;
    }
    if (raycastHit->blockType == world::BlockType::Chest)
    {
        openChestMenu(raycastHit->solidBlock);
        return;
    }
    if (world::isFurnaceBlock(raycastHit->blockType))
    {
        openFurnaceMenu(raycastHit->solidBlock);
        return;
    }

    InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
    if (selectedSlot.count == 0 || selectedSlot.blockType == world::BlockType::Air
        || selectedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    const world::BlockType placedBlockType =
        world::normalizeFurnaceBlockType(selectedSlot.blockType) == world::BlockType::Furnace
        ? furnaceBlockFacingPlayer(camera_.forward())
        : selectedSlot.blockType;
    const world::WorldEditCommand command{
        .action = world::WorldEditAction::Place,
        .position = raycastHit->buildTarget,
        .blockType = placedBlockType,
    };
    const BotanyPlacementResult botanyPlacement = validateBotanyBlockPlacement(
        world_,
        command.position,
        world::normalizeFurnaceBlockType(selectedSlot.blockType),
        playerFeetPosition_,
        creativeModeEnabled_);
    if (!botanyPlacement.allowed)
    {
        respawnNotice_ = botanyPlacement.failureReason;
        return;
    }
    if (world_.applyEditCommand(command))
    {
        if (!creativeModeEnabled_)
        {
            consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
        }
        soundEffects_.playBlockPlace(command.blockType);
        if (hostSession_ != nullptr && hostSession_->running())
        {
            hostSession_->broadcastBlockEdit({
                .authorClientId = localClientId_,
                .action = command.action,
                .x = command.position.x,
                .y = command.position.y,
                .z = command.position.z,
                .blockType = command.blockType,
            });
        }
        if (clientSession_ != nullptr && clientSession_->connected())
        {
            clientSession_->sendInput(
                {
                    .clientId = clientSession_->clientId(),
                    .positionX = playerFeetPosition_.x,
                    .positionY = playerFeetPosition_.y,
                    .positionZ = playerFeetPosition_.z,
                    .yawDelta = camera_.yawDegrees(),
                    .pitchDelta = camera_.pitchDegrees(),
                    .health = playerVitals_.health(),
                    .air = playerVitals_.air(),
                    .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                    .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                    .placeBlock = true,
                    .targetX = command.position.x,
                    .targetY = command.position.y,
                    .targetZ = command.position.z,
                    .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                    .placeBlockType = command.blockType,
                },
                networkServerTick_);
        }
    }
}
}  // namespace vibecraft::app
