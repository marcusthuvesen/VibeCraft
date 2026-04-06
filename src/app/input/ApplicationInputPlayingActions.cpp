#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "vibecraft/app/ApplicationBotanyRuntime.hpp"
#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/Mining.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/app/TorchPlacement.hpp"
#include "vibecraft/app/crafting/Crafting.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool consumeOneArrowFromInventory(HotbarSlots& hotbarSlots, BagSlots& bagSlots)
{
    for (InventorySlot& slot : hotbarSlots)
    {
        if (slot.equippedItem == EquippedItem::Arrow && slot.count > 0)
        {
            --slot.count;
            if (slot.count == 0)
            {
                clearInventorySlot(slot);
            }
            return true;
        }
    }
    for (InventorySlot& slot : bagSlots)
    {
        if (slot.equippedItem == EquippedItem::Arrow && slot.count > 0)
        {
            --slot.count;
            if (slot.count == 0)
            {
                clearInventorySlot(slot);
            }
            return true;
        }
    }
    return false;
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

[[nodiscard]] bool shouldDropSaplingFromLeafBreak(
    const glm::ivec3& blockPosition,
    const float sessionPlayTimeSeconds)
{
    // Target behavior: roughly 1 in 10 leaf/crown breaks drop a sapling.
    const std::uint64_t positionSeed = static_cast<std::uint64_t>(blockStorageKey(blockPosition));
    const std::uint32_t timeSeed = static_cast<std::uint32_t>(
        std::llround(std::max(0.0f, sessionPlayTimeSeconds) * 1000.0f));
    std::uint32_t seed = static_cast<std::uint32_t>(positionSeed ^ (positionSeed >> 32U));
    seed ^= timeSeed * 747796405u + 2891336453u;
    seed ^= seed >> 16U;
    seed *= 2246822519u;
    seed ^= seed >> 13U;
    seed *= 3266489917u;
    seed ^= seed >> 16U;
    return (seed % 10u) == 0u;
}

[[nodiscard]] world::BlockType furnaceBlockFacingPlayer(const glm::vec3& cameraForward)
{
    if (std::abs(cameraForward.x) >= std::abs(cameraForward.z))
    {
        return cameraForward.x >= 0.0f ? world::BlockType::FurnaceWest : world::BlockType::FurnaceEast;
    }
    return cameraForward.z >= 0.0f ? world::BlockType::FurnaceNorth : world::BlockType::Furnace;
}

[[nodiscard]] world::BlockType stairsBlockFacingPlayer(
    const world::BlockType stairsBaseBlock,
    const glm::vec3& cameraForward)
{
    const bool xDominant = std::abs(cameraForward.x) >= std::abs(cameraForward.z);
    switch (world::normalizeStairBlockType(stairsBaseBlock))
    {
    case world::BlockType::OakStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::OakStairsEast : world::BlockType::OakStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::OakStairsSouth : world::BlockType::OakStairsNorth;
    case world::BlockType::CobblestoneStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::CobblestoneStairsEast
                                           : world::BlockType::CobblestoneStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::CobblestoneStairsSouth
                                       : world::BlockType::CobblestoneStairsNorth;
    case world::BlockType::StoneStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::StoneStairsEast : world::BlockType::StoneStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::StoneStairsSouth : world::BlockType::StoneStairsNorth;
    case world::BlockType::BrickStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::BrickStairsEast : world::BlockType::BrickStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::BrickStairsSouth : world::BlockType::BrickStairsNorth;
    case world::BlockType::SandstoneStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::SandstoneStairsEast
                                           : world::BlockType::SandstoneStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::SandstoneStairsSouth
                                       : world::BlockType::SandstoneStairsNorth;
    case world::BlockType::JungleStairs:
        if (xDominant)
        {
            return cameraForward.x >= 0.0f ? world::BlockType::JungleStairsEast : world::BlockType::JungleStairsWest;
        }
        return cameraForward.z >= 0.0f ? world::BlockType::JungleStairsSouth : world::BlockType::JungleStairsNorth;
    default:
        return stairsBaseBlock;
    }
}

[[nodiscard]] world::DoorFacing doorFacingPlayer(const glm::vec3& cameraForward)
{
    if (std::abs(cameraForward.x) >= std::abs(cameraForward.z))
    {
        return cameraForward.x >= 0.0f ? world::DoorFacing::East : world::DoorFacing::West;
    }
    return cameraForward.z >= 0.0f ? world::DoorFacing::South : world::DoorFacing::North;
}

[[nodiscard]] world::DoorFacing pairedDoorFacingForPlacement(
    const world::World& worldState,
    const glm::ivec3& lowerDoorPosition,
    const world::BlockType doorItemBlock,
    const world::DoorFacing fallbackFacing)
{
    const world::DoorFamily family = world::doorFamilyForBlockType(doorItemBlock);
    const bool xAxisPlane = world::doorUsesXAxisPlane(fallbackFacing);
    const std::array<glm::ivec3, 2> neighborOffsets = xAxisPlane
        ? std::array<glm::ivec3, 2>{glm::ivec3{0, 0, -1}, glm::ivec3{0, 0, 1}}
        : std::array<glm::ivec3, 2>{glm::ivec3{-1, 0, 0}, glm::ivec3{1, 0, 0}};

    for (const glm::ivec3& offset : neighborOffsets)
    {
        const glm::ivec3 neighborLower = lowerDoorPosition + offset;
        const world::BlockType neighborBlock =
            worldState.blockAt(neighborLower.x, neighborLower.y, neighborLower.z);
        if (!world::isDoorVariantBlock(neighborBlock) || world::isDoorUpperHalf(neighborBlock))
        {
            continue;
        }
        if (world::doorFamilyForBlockType(neighborBlock) != family)
        {
            continue;
        }

        const world::DoorFacing neighborFacing = world::doorFacingForBlockType(neighborBlock);
        if (world::doorUsesXAxisPlane(neighborFacing) != xAxisPlane)
        {
            continue;
        }

        return world::oppositeDoorFacingWithinAxis(neighborFacing);
    }

    return fallbackFacing;
}

[[nodiscard]] bool canReplaceDoorCell(const world::BlockType blockType)
{
    return blockType == world::BlockType::Air
        || (!world::isSolid(blockType) && world::blockMetadata(blockType).breakable);
}
}  // namespace

void Application::processPlayingActionInput(const float deltaTimeSeconds)
{
    playerBowCooldownSeconds_ = std::max(0.0f, playerBowCooldownSeconds_ - deltaTimeSeconds);
    bool attackedMobThisFrame = false;
    if (inputState_.leftMousePressed)
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (multiplayerMode_ != MultiplayerRuntimeMode::Client && selectedSlot.equippedItem == EquippedItem::Bow)
        {
            if (tryFirePlayerBow())
            {
                attackedMobThisFrame = true;
            }
        }
        if (!attackedMobThisFrame && multiplayerMode_ == MultiplayerRuntimeMode::Client && clientSession_ != nullptr
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
                spawnMobKillDrops(mobDamage->mobKind, mobDamage->feetPosition);
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
            InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
            if (selectedSlot.equippedItem != EquippedItem::Bow)
            {
                [[maybe_unused]] const bool durabilityConsumed = consumeEquippedItemDurability(
                    selectedSlot,
                    durabilityUseAmountForEquippedItem(selectedSlot.equippedItem));
            }
        }
        return;
    }
    if (!raycastHit.has_value())
    {
        activeMiningState_.active = false;
        activeMiningState_.elapsedSeconds = 0.0f;
        return;
    }

    const auto relayWorldEdit = [this](const world::WorldEditCommand& command)
    {
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
            multiplayer::protocol::ClientInputMessage input{
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
                .targetX = command.position.x,
                .targetY = command.position.y,
                .targetZ = command.position.z,
                .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                .placeBlockType = command.blockType,
            };
            if (command.action == world::WorldEditAction::Remove)
            {
                input.breakBlock = true;
            }
            else
            {
                input.placeBlock = true;
            }
            clientSession_->sendInput(input, networkServerTick_);
        }
    };

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
            std::vector<world::WorldEditCommand> commands;
            commands.push_back(world::WorldEditCommand{
                .action = world::WorldEditAction::Remove,
                .position = raycastHit->solidBlock,
                .blockType = world::BlockType::Air,
            });
            if (world::isDoorVariantBlock(raycastHit->blockType))
            {
                const glm::ivec3 counterpartPosition = raycastHit->solidBlock
                    + (world::isDoorUpperHalf(raycastHit->blockType) ? glm::ivec3(0, -1, 0)
                                                                     : glm::ivec3(0, 1, 0));
                const world::BlockType counterpartType = world_.blockAt(
                    counterpartPosition.x,
                    counterpartPosition.y,
                    counterpartPosition.z);
                if (world::isDoorVariantBlock(counterpartType)
                    && world::doorFamilyForBlockType(counterpartType)
                        == world::doorFamilyForBlockType(raycastHit->blockType))
                {
                    commands.push_back(world::WorldEditCommand{
                        .action = world::WorldEditAction::Remove,
                        .position = counterpartPosition,
                        .blockType = world::BlockType::Air,
                    });
                }
            }

            std::vector<world::WorldEditCommand> appliedCommands;
            appliedCommands.reserve(commands.size());
            for (const world::WorldEditCommand& command : commands)
            {
                if (world_.applyEditCommand(command))
                {
                    appliedCommands.push_back(command);
                }
            }
            if (!appliedCommands.empty())
            {
                // Fire break SFX immediately when the block edit is confirmed locally.
                soundEffects_.playBlockBreak(raycastHit->blockType);
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
                    InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
                    [[maybe_unused]] const bool durabilityConsumed = consumeEquippedItemDurability(
                        selectedSlot,
                        durabilityUseAmountForEquippedItem(selectedSlot.equippedItem));
                    const glm::ivec3 dropPosition = world::isDoorUpperHalf(raycastHit->blockType)
                        ? raycastHit->solidBlock + glm::ivec3(0, -1, 0)
                        : raycastHit->solidBlock;
                    const world::BlockType brokenBlockType =
                        world::normalizePlaceVariantBlockType(raycastHit->blockType);
                    if (!world::isLeafBlock(brokenBlockType))
                    {
                        spawnDroppedItem(brokenBlockType, dropPosition);
                    }
                    else if (shouldDropSaplingFromLeafBreak(raycastHit->solidBlock, sessionPlayTimeSeconds_))
                    {
                        spawnDroppedItem(world::BlockType::FiberSapling, dropPosition);
                    }
                }
                for (const world::WorldEditCommand& command : appliedCommands)
                {
                    relayWorldEdit(command);
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
    if (raycastHit->blockType == world::BlockType::TNT)
    {
        if (multiplayerMode_ == MultiplayerRuntimeMode::Client)
        {
            respawnNotice_ = "TNT ignition is host-side in multiplayer.";
            return;
        }
        igniteTntAtBlock(raycastHit->solidBlock, 4.0f, true);
        return;
    }
    if (world::isDoorVariantBlock(raycastHit->blockType))
    {
        const glm::ivec3 lowerPosition = world::isDoorUpperHalf(raycastHit->blockType)
            ? raycastHit->solidBlock + glm::ivec3(0, -1, 0)
            : raycastHit->solidBlock;
        const glm::ivec3 upperPosition = lowerPosition + glm::ivec3(0, 1, 0);
        std::vector<world::WorldEditCommand> toggleCommands;
        const world::BlockType lowerType = world_.blockAt(lowerPosition.x, lowerPosition.y, lowerPosition.z);
        const world::BlockType upperType = world_.blockAt(upperPosition.x, upperPosition.y, upperPosition.z);
        if (world::isDoorVariantBlock(lowerType))
        {
            toggleCommands.push_back(world::WorldEditCommand{
                .action = world::WorldEditAction::Place,
                .position = lowerPosition,
                .blockType = world::toggleDoorBlockType(lowerType),
            });
        }
        if (world::isDoorVariantBlock(upperType)
            && world::doorFamilyForBlockType(upperType) == world::doorFamilyForBlockType(lowerType))
        {
            toggleCommands.push_back(world::WorldEditCommand{
                .action = world::WorldEditAction::Place,
                .position = upperPosition,
                .blockType = world::toggleDoorBlockType(upperType),
            });
        }

        bool toggledDoor = false;
        for (const world::WorldEditCommand& command : toggleCommands)
        {
            if (!world_.applyEditCommand(command))
            {
                continue;
            }
            toggledDoor = true;
            relayWorldEdit(command);
        }
        if (toggledDoor)
        {
            soundEffects_.playBlockPlace(raycastHit->blockType);
        }
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

    const world::BlockType selectedBaseBlock = world::normalizePlaceVariantBlockType(selectedSlot.blockType);
    if (world::isTorchBlock(selectedBaseBlock)
        && !isValidTorchPlacementFace(raycastHit->solidBlock, raycastHit->buildTarget))
    {
        respawnNotice_ = "Torches must be placed on top or wall faces.";
        return;
    }
    const world::BlockType placedBlockType = world::isFurnaceBlock(selectedBaseBlock)
        ? furnaceBlockFacingPlayer(camera_.forward())
        : world::isStairBlock(selectedBaseBlock)
            ? stairsBlockFacingPlayer(selectedBaseBlock, camera_.forward())
            : world::isTorchBlock(selectedBaseBlock)
                ? torchBlockForPlacement(selectedBaseBlock, raycastHit->solidBlock, raycastHit->buildTarget)
                : selectedBaseBlock;
    const world::WorldEditCommand command{
        .action = world::WorldEditAction::Place,
        .position = raycastHit->buildTarget,
        .blockType = placedBlockType,
    };
    const BotanyPlacementResult botanyPlacement = validateBotanyBlockPlacement(
        world_,
        command.position,
        selectedBaseBlock,
        playerFeetPosition_,
        creativeModeEnabled_);
    if (!botanyPlacement.allowed)
    {
        respawnNotice_ = botanyPlacement.failureReason;
        return;
    }

    if (world::isDoorItemBlock(selectedBaseBlock))
    {
        const glm::ivec3 upperPosition = command.position + glm::ivec3(0, 1, 0);
        if (upperPosition.y > world::kWorldMaxY)
        {
            respawnNotice_ = "Doors need two blocks of height.";
            return;
        }
        if (!world::isSolid(world_.blockAt(command.position.x, command.position.y - 1, command.position.z)))
        {
            respawnNotice_ = "Doors need a solid block underneath.";
            return;
        }
        const world::BlockType lowerExisting =
            world_.blockAt(command.position.x, command.position.y, command.position.z);
        const world::BlockType upperExisting =
            world_.blockAt(upperPosition.x, upperPosition.y, upperPosition.z);
        if (!canReplaceDoorCell(lowerExisting) || !canReplaceDoorCell(upperExisting))
        {
            respawnNotice_ = "Doors need two empty blocks.";
            return;
        }

        const world::DoorFacing facing = pairedDoorFacingForPlacement(
            world_,
            command.position,
            selectedBaseBlock,
            doorFacingPlayer(camera_.forward()));
        const std::array<world::WorldEditCommand, 2> doorCommands{{
            {
                .action = world::WorldEditAction::Place,
                .position = command.position,
                .blockType = world::placedDoorLowerBlockType(selectedBaseBlock, facing),
            },
            {
                .action = world::WorldEditAction::Place,
                .position = upperPosition,
                .blockType = world::placedDoorUpperBlockType(selectedBaseBlock, facing),
            },
        }};

        if (world_.applyEditCommand(doorCommands[0]) && world_.applyEditCommand(doorCommands[1]))
        {
            if (!creativeModeEnabled_)
            {
                consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
            }
            soundEffects_.playBlockPlace(selectedBaseBlock);
            for (const world::WorldEditCommand& doorCommand : doorCommands)
            {
                relayWorldEdit(doorCommand);
            }
        }
        return;
    }

    if (world_.applyEditCommand(command))
    {
        if (!creativeModeEnabled_)
        {
            consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
        }
        soundEffects_.playBlockPlace(command.blockType);
        relayWorldEdit(command);
    }
}

bool Application::tryFirePlayerBow()
{
    if (playerBowCooldownSeconds_ > 0.0f)
    {
        return false;
    }
    InventorySlot& bowSlot = hotbarSlots_[selectedHotbarIndex_];
    if (bowSlot.equippedItem != EquippedItem::Bow || bowSlot.count == 0)
    {
        return false;
    }
    if (!creativeModeEnabled_)
    {
        if (!consumeOneArrowFromInventory(hotbarSlots_, bagSlots_))
        {
            return false;
        }
        static_cast<void>(consumeEquippedItemDurability(
            bowSlot,
            durabilityUseAmountForEquippedItem(EquippedItem::Bow)));
    }
    const glm::vec3 origin = camera_.position();
    const glm::vec3 forward = camera_.forward();
    mobSpawnSystem_.spawnPlayerArrow(origin, forward, 18.0f, 8.5f, 4.0f, 0.14f, 2.6f);
    soundEffects_.playBowShoot();
    playerBowCooldownSeconds_ = 0.36f;
    return true;
}
}  // namespace vibecraft::app
