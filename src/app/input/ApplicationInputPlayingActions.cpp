#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "vibecraft/app/ApplicationBotanyRuntime.hpp"
#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/app/OxygenItems.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::int64_t chestStorageKey(const glm::ivec3& blockPosition)
{
    const std::int64_t x = static_cast<std::int64_t>(blockPosition.x) & 0x1fffffLL;
    const std::int64_t y = static_cast<std::int64_t>(blockPosition.y) & 0x1fffffLL;
    const std::int64_t z = static_cast<std::int64_t>(blockPosition.z) & 0x1fffffLL;
    return (x << 42) | (y << 21) | z;
}

[[nodiscard]] bool isEarthyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::Grass || blockType == world::BlockType::Dirt
        || blockType == world::BlockType::Sand || blockType == world::BlockType::SnowGrass
        || blockType == world::BlockType::JungleGrass || blockType == world::BlockType::Gravel
        || blockType == world::BlockType::MossBlock
        || blockType == world::BlockType::Cactus || blockType == world::BlockType::Dandelion
        || blockType == world::BlockType::Poppy || blockType == world::BlockType::BlueOrchid
        || blockType == world::BlockType::Allium || blockType == world::BlockType::OxeyeDaisy
        || blockType == world::BlockType::BrownMushroom || blockType == world::BlockType::RedMushroom
        || blockType == world::BlockType::DeadBush || blockType == world::BlockType::Vines
        || blockType == world::BlockType::CocoaPod || blockType == world::BlockType::Melon
        || blockType == world::BlockType::Bamboo || blockType == world::BlockType::GrassTuft
        || blockType == world::BlockType::FlowerTuft || blockType == world::BlockType::DryTuft
        || blockType == world::BlockType::LushTuft || blockType == world::BlockType::FrostTuft
        || blockType == world::BlockType::SparseTuft || blockType == world::BlockType::CloverTuft
        || blockType == world::BlockType::SproutTuft;
}

[[nodiscard]] bool isStoneFamilyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::Stone || blockType == world::BlockType::Deepslate
        || blockType == world::BlockType::CoalOre || blockType == world::BlockType::IronOre
        || blockType == world::BlockType::GoldOre || blockType == world::BlockType::DiamondOre
        || blockType == world::BlockType::EmeraldOre || blockType == world::BlockType::Bricks
        || blockType == world::BlockType::Glowstone || blockType == world::BlockType::Obsidian
        || blockType == world::BlockType::MossyCobblestone
        || blockType == world::BlockType::Glass;
}

[[nodiscard]] bool isWoodFamilyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::OakLog || blockType == world::BlockType::OakLeaves
        || blockType == world::BlockType::JungleLog || blockType == world::BlockType::JungleLeaves
        || blockType == world::BlockType::SpruceLog || blockType == world::BlockType::SpruceLeaves
        || blockType == world::BlockType::Bookshelf || blockType == world::BlockType::JunglePlanks;
}

[[nodiscard]] bool isPickaxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isStoneFamilyBlockType(targetBlockType) || targetBlockType == world::BlockType::Cobblestone
        || targetBlockType == world::BlockType::Sandstone || targetBlockType == world::BlockType::Furnace
        || targetBlockType == world::BlockType::Glass;
}

[[nodiscard]] bool isAxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isWoodFamilyBlockType(targetBlockType) || targetBlockType == world::BlockType::OakPlanks
        || targetBlockType == world::BlockType::JunglePlanks
        || targetBlockType == world::BlockType::CraftingTable || targetBlockType == world::BlockType::Chest
        || targetBlockType == world::BlockType::Bookshelf;
}

[[nodiscard]] bool isPickaxeItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isAxeItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
    case EquippedItem::DiamondAxe:
        return true;
    default:
        return false;
    }
}

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

[[nodiscard]] int toolMaterialTier(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::WoodPickaxe:
    case EquippedItem::WoodAxe:
        return 1;
    case EquippedItem::StoneSword:
    case EquippedItem::StonePickaxe:
    case EquippedItem::StoneAxe:
        return 2;
    case EquippedItem::IronSword:
    case EquippedItem::IronPickaxe:
    case EquippedItem::IronAxe:
        return 3;
    case EquippedItem::GoldSword:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::GoldAxe:
        return 4;
    case EquippedItem::DiamondSword:
    case EquippedItem::DiamondPickaxe:
    case EquippedItem::DiamondAxe:
        return 5;
    default:
        return 0;
    }
}

[[nodiscard]] float toolMiningSpeedMultiplier(
    const EquippedItem equippedItem,
    const world::BlockType targetBlockType)
{
    if (equippedItem == EquippedItem::None)
    {
        return 1.0f;
    }

    const int tier = toolMaterialTier(equippedItem);
    if (tier <= 0)
    {
        return 1.0f;
    }

    const float t = static_cast<float>(tier);
    if (isPickaxeItem(equippedItem))
    {
        if (!isPickaxeEffectiveTarget(targetBlockType))
        {
            return 1.06f;
        }
        return 1.12f + t * 0.64f;
    }
    if (isAxeItem(equippedItem))
    {
        if (!isAxeEffectiveTarget(targetBlockType))
        {
            return 1.05f;
        }
        return 1.22f + t * 0.8f;
    }
    if (isSwordItem(equippedItem))
    {
        if (isPickaxeEffectiveTarget(targetBlockType) || isAxeEffectiveTarget(targetBlockType))
        {
            return 0.58f + t * 0.06f;
        }
        return 1.04f + t * 0.05f;
    }
    return 1.0f;
}

[[nodiscard]] float miningSpeedMultiplier(
    const world::BlockType equippedBlockType,
    const world::BlockType targetBlockType)
{
    if (equippedBlockType == world::BlockType::Air)
    {
        return 1.0f;
    }
    if (equippedBlockType == targetBlockType)
    {
        return 2.0f;
    }
    if (isStoneFamilyBlockType(targetBlockType) && isStoneFamilyBlockType(equippedBlockType))
    {
        return 2.5f;
    }
    if (isEarthyBlockType(targetBlockType) && isEarthyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    if (isWoodFamilyBlockType(targetBlockType) && isWoodFamilyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    return 1.15f;
}

[[nodiscard]] float miningDurationSeconds(
    const world::BlockType targetBlockType,
    const world::BlockType equippedBlockType,
    const EquippedItem equippedItem)
{
    constexpr float kHardnessToSeconds = 0.65f;
    constexpr float kMinimumBreakDurationSeconds = 0.06f;
    const world::BlockMetadata metadata = world::blockMetadata(targetBlockType);
    if (!metadata.breakable)
    {
        return std::numeric_limits<float>::max();
    }

    const float blockMultiplier = miningSpeedMultiplier(equippedBlockType, targetBlockType);
    const float toolMultiplier = toolMiningSpeedMultiplier(equippedItem, targetBlockType);
    const float speedMultiplier = std::max(blockMultiplier, toolMultiplier);
    const float rawDurationSeconds = metadata.hardness * kHardnessToSeconds / speedMultiplier;
    return std::max(kMinimumBreakDurationSeconds, rawDurationSeconds);
}

template <std::size_t SlotCount>
[[nodiscard]] bool addEquippedItemToSlots(
    std::array<InventorySlot, SlotCount>& slots,
    const EquippedItem equippedItem,
    std::size_t* const selectedHotbarIndex)
{
    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.equippedItem == equippedItem && slot.count < kMaxStackSize)
        {
            ++slot.count;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.count == 0)
        {
            slot.blockType = world::BlockType::Air;
            slot.equippedItem = equippedItem;
            slot.count = 1;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool addEquippedItemToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const EquippedItem equippedItem,
    std::size_t& selectedHotbarIndex)
{
    if (equippedItem == EquippedItem::None)
    {
        return false;
    }
    if (addEquippedItemToSlots(hotbarSlots, equippedItem, &selectedHotbarIndex))
    {
        return true;
    }
    return addEquippedItemToSlots(bagSlots, equippedItem, nullptr);
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
        return;
    }
    if (inputState_.rightMousePressed)
    {
        InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        const PortableOxygenItemUseResult oxygenUseResult = tryUsePortableOxygenItem(selectedSlot, oxygenSystem_);
        if (oxygenUseResult.handled)
        {
            soundEffects_.playItemConsume();
            const game::OxygenTankTier tankTier = oxygenTankTierForUpgradeItem(selectedSlot.equippedItem);
            if (tankTier != game::OxygenTankTier::None)
            {
                const InventorySlot previousTank = equipmentSlots_[equipmentSlotIndex(EquipmentSlotKind::OxygenTank)];
                syncOxygenEquipmentSlotFromSystem(equipmentSlots_, oxygenSystem_);
                if (!creativeModeEnabled_
                    && previousTank.count > 0
                    && previousTank.equippedItem != EquippedItem::None
                    && previousTank.equippedItem != selectedSlot.equippedItem)
                {
                    const bool returnedToInventory = addEquippedItemToInventory(
                        hotbarSlots_,
                        bagSlots_,
                        previousTank.equippedItem,
                        selectedHotbarIndex_);
                    if (!returnedToInventory)
                    {
                        spawnDroppedItemAtPosition(
                            previousTank.equippedItem,
                            playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                }
            }
            if (oxygenUseResult.consumeSlot && !creativeModeEnabled_)
            {
                consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
            }
            if (!oxygenUseResult.notice.empty())
            {
                respawnNotice_ = oxygenUseResult.notice;
            }
            return;
        }
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
                    const auto chestIt = chestSlotsByPosition_.find(chestStorageKey(raycastHit->solidBlock));
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
                if (!creativeModeEnabled_)
                {
                    spawnDroppedItem(raycastHit->blockType, raycastHit->solidBlock);
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
                            .air = encodeLegacyNetworkAir(oxygenSystem_.state()),
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

    InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
    if (selectedSlot.count == 0 || selectedSlot.blockType == world::BlockType::Air
        || selectedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    const world::WorldEditCommand command{
        .action = world::WorldEditAction::Place,
        .position = raycastHit->buildTarget,
        .blockType = selectedSlot.blockType,
    };
    if (selectedSlot.blockType == world::BlockType::OxygenGenerator
        && !canPlaceRelayAtTarget(
            world_,
            command.position,
            playerFeetPosition_,
            kPlayerMovementSettings.standingColliderHeight))
    {
        respawnNotice_ = "Atmos relay blocked here.";
        return;
    }
    const BotanyPlacementResult botanyPlacement = validateBotanyBlockPlacement(
        world_,
        command.position,
        selectedSlot.blockType,
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
                    .air = encodeLegacyNetworkAir(oxygenSystem_.state()),
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
