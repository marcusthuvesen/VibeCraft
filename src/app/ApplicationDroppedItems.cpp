#include "vibecraft/app/Application.hpp"

#include "vibecraft/game/MobTypes.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace vibecraft::app
{
void Application::spawnDroppedItem(
    const world::BlockType blockType,
    const glm::ivec3& blockPosition)
{
    const world::BlockType normalizedBlock = world::normalizePlaceVariantBlockType(blockType);
    const world::BlockType dropBlockType =
        normalizedBlock == world::BlockType::Stone ? world::BlockType::Cobblestone : normalizedBlock;

    if (dropBlockType == world::BlockType::Air || dropBlockType == world::BlockType::Water
        || dropBlockType == world::BlockType::Lava)
    {
        return;
    }

    if (dropBlockType == world::BlockType::CoalOre)
    {
        spawnDroppedItemAtPosition(
            EquippedItem::Coal,
            glm::vec3(
                static_cast<float>(blockPosition.x) + 0.5f,
                static_cast<float>(blockPosition.y) + 0.2f,
                static_cast<float>(blockPosition.z) + 0.5f));
        return;
    }

    spawnDroppedItemAtPosition(
        dropBlockType,
        glm::vec3(
            static_cast<float>(blockPosition.x) + 0.5f,
            static_cast<float>(blockPosition.y) + 0.2f,
            static_cast<float>(blockPosition.z) + 0.5f));
}

void Application::spawnDroppedItemAtPosition(
    const world::BlockType blockType,
    const glm::vec3& worldPosition)
{
    if (blockType == world::BlockType::Air || blockType == world::BlockType::Water
        || blockType == world::BlockType::Lava)
    {
        return;
    }

    const float seed = worldPosition.x * 0.73f + worldPosition.z * 1.17f + worldPosition.y * 0.41f;
    droppedItems_.push_back(DroppedItem{
        .blockType = blockType,
        .equippedItem = EquippedItem::None,
        .worldPosition = worldPosition,
        .velocity = glm::vec3(
            std::sin(seed) * 1.05f,
            2.0f,
            std::cos(seed * 1.37f) * 1.05f),
        .ageSeconds = 0.0f,
        .pickupDelaySeconds = 0.2f,
        .spinRadians = 0.0f,
    });
}

void Application::spawnDroppedItemAtPosition(
    const EquippedItem equippedItem,
    const glm::vec3& worldPosition)
{
    if (equippedItem == EquippedItem::None)
    {
        return;
    }

    const float seed = worldPosition.x * 0.73f + worldPosition.z * 1.17f + worldPosition.y * 0.41f;
    droppedItems_.push_back(DroppedItem{
        .blockType = world::BlockType::Air,
        .equippedItem = equippedItem,
        .worldPosition = worldPosition,
        .velocity = glm::vec3(
            std::sin(seed) * 1.05f,
            2.0f,
            std::cos(seed * 1.37f) * 1.05f),
        .ageSeconds = 0.0f,
        .pickupDelaySeconds = 0.2f,
        .spinRadians = 0.0f,
    });
}

void Application::spawnMobKillDrops(const game::MobKind mobKind, const glm::vec3& feetPosition)
{
    const glm::vec3 base = feetPosition + glm::vec3(0.0f, 0.35f, 0.0f);
    const auto scatter = [&base](const int index)
    {
        const float s = static_cast<float>(index);
        return base + glm::vec3(0.09f * s, 0.0f, 0.07f * s);
    };
    const auto hashRoll = [&base](const int salt) -> unsigned int
    {
        const glm::vec3 p = base + glm::vec3(static_cast<float>(salt) * 0.31f, 0.0f, static_cast<float>(salt) * 0.29f);
        unsigned int h = static_cast<unsigned int>(std::lround(std::abs(p.x * 12.9898 + p.y * 78.233 + p.z * 37.719)));
        h ^= h >> 16U;
        h *= 2246822519U;
        h ^= h >> 13U;
        h *= 3266489917U;
        h ^= h >> 16U;
        return h % 3U;
    };

    using MK = game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
        spawnDroppedItemAtPosition(EquippedItem::RottenFlesh, base);
        break;
    case MK::Skeleton:
    {
        const unsigned int arrowDrops = hashRoll(11);
        for (unsigned int i = 0; i < arrowDrops; ++i)
        {
            spawnDroppedItemAtPosition(EquippedItem::Arrow, scatter(static_cast<int>(i)));
        }
        break;
    }
    case MK::Creeper:
    {
        const unsigned int powderDrops = hashRoll(17);
        for (unsigned int i = 0; i < powderDrops; ++i)
        {
            spawnDroppedItemAtPosition(EquippedItem::Gunpowder, scatter(static_cast<int>(i)));
        }
        break;
    }
    case MK::Spider:
    {
        const unsigned int stringDrops = hashRoll(23);
        for (unsigned int i = 0; i < stringDrops; ++i)
        {
            spawnDroppedItemAtPosition(EquippedItem::String, scatter(static_cast<int>(i)));
        }
        break;
    }
    case MK::Cow:
        spawnDroppedItemAtPosition(EquippedItem::Leather, base);
        break;
    case MK::Pig:
        spawnDroppedItemAtPosition(EquippedItem::RawPorkchop, base);
        break;
    case MK::Sheep:
        spawnDroppedItemAtPosition(EquippedItem::Mutton, base);
        break;
    case MK::Chicken:
        spawnDroppedItemAtPosition(EquippedItem::Feather, base);
        break;
    case MK::Player:
    default:
        break;
    }
}

glm::vec3 Application::dropSpawnPositionInFront(
    const float forwardDistance,
    const float verticalOffset) const
{
    glm::vec3 horizontalForward = camera_.forward();
    horizontalForward.y = 0.0f;
    const float horizontalLengthSq = glm::dot(horizontalForward, horizontalForward);
    if (horizontalLengthSq <= 0.0001f)
    {
        horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }
    else
    {
        horizontalForward = glm::normalize(horizontalForward);
    }

    return playerFeetPosition_ + glm::vec3(0.0f, verticalOffset, 0.0f)
        + horizontalForward * forwardDistance;
}

void Application::dropSingleItemFromSlotAt(InventorySlot& slot, const glm::vec3& worldPosition)
{
    if (isInventorySlotEmpty(slot))
    {
        return;
    }
    if (slot.equippedItem != EquippedItem::None)
    {
        spawnDroppedItemAtPosition(slot.equippedItem, worldPosition);
    }
    else if (slot.blockType != world::BlockType::Air)
    {
        spawnDroppedItemAtPosition(slot.blockType, worldPosition);
    }
    else
    {
        return;
    }

    --slot.count;
    if (slot.count == 0)
    {
        clearInventorySlot(slot);
    }
}

void Application::dropSingleItemFromSlotInFront(InventorySlot& slot, const float forwardDistance)
{
    dropSingleItemFromSlotAt(slot, dropSpawnPositionInFront(forwardDistance));
}

void Application::dropEntireSlotInFront(InventorySlot& slot, const float forwardDistance)
{
    const glm::vec3 dropPosition = dropSpawnPositionInFront(forwardDistance);
    while (!isInventorySlotEmpty(slot))
    {
        dropSingleItemFromSlotAt(slot, dropPosition);
    }
}

void Application::updateDroppedItems(const float deltaTimeSeconds, const float eyeHeight)
{
    const glm::vec3 pickupCenter = playerFeetPosition_ + glm::vec3(0.0f, eyeHeight * 0.6f, 0.0f);
    constexpr float kPickupRadius = 1.25f;
    constexpr float kPickupRadiusSq = kPickupRadius * kPickupRadius;
    constexpr float kMagnetRadius = 3.8f;
    constexpr float kMagnetRadiusSq = kMagnetRadius * kMagnetRadius;
    constexpr float kGravity = 16.0f;
    constexpr float kTau = 6.28318530718f;

    std::size_t itemIndex = 0;
    while (itemIndex < droppedItems_.size())
    {
        DroppedItem& droppedItem = droppedItems_[itemIndex];
        droppedItem.ageSeconds += deltaTimeSeconds;
        droppedItem.pickupDelaySeconds =
            std::max(0.0f, droppedItem.pickupDelaySeconds - deltaTimeSeconds);
        droppedItem.spinRadians = std::fmod(
            droppedItem.spinRadians + deltaTimeSeconds * 4.2f,
            kTau);

        droppedItem.velocity.y -= kGravity * deltaTimeSeconds;
        droppedItem.velocity *= std::pow(0.96f, deltaTimeSeconds * 60.0f);

        const glm::vec3 toPlayer = pickupCenter - droppedItem.worldPosition;
        const float distanceToPlayerSq = glm::dot(toPlayer, toPlayer);
        if (droppedItem.pickupDelaySeconds <= 0.0f && distanceToPlayerSq <= kMagnetRadiusSq)
        {
            const float distanceToPlayer = std::sqrt(std::max(distanceToPlayerSq, 0.0001f));
            const glm::vec3 pullDirection = toPlayer / distanceToPlayer;
            const float pullStrength = 8.0f + (1.0f - distanceToPlayer / kMagnetRadius) * 18.0f;
            droppedItem.velocity += pullDirection * pullStrength * deltaTimeSeconds;
        }

        droppedItem.worldPosition += droppedItem.velocity * deltaTimeSeconds;

        const int belowX = static_cast<int>(std::floor(droppedItem.worldPosition.x));
        const int belowY = static_cast<int>(std::floor(droppedItem.worldPosition.y - 0.36f));
        const int belowZ = static_cast<int>(std::floor(droppedItem.worldPosition.z));
        if (world::isSolid(world_.blockAt(belowX, belowY, belowZ)) && droppedItem.velocity.y < 0.0f)
        {
            droppedItem.worldPosition.y = static_cast<float>(belowY + 1) + 0.36f;
            droppedItem.velocity.y = 0.0f;
        }

        const glm::vec3 delta = droppedItem.worldPosition - pickupCenter;
        if (droppedItem.pickupDelaySeconds <= 0.0f && glm::dot(delta, delta) <= kPickupRadiusSq)
        {
            bool pickedUp = false;
            if (droppedItem.equippedItem != EquippedItem::None)
            {
                pickedUp = addEquippedItemToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    droppedItem.equippedItem,
                    selectedHotbarIndex_,
                    InventorySelectionBehavior::PreserveCurrent);
            }
            else
            {
                pickedUp = addBlockToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    droppedItem.blockType,
                    selectedHotbarIndex_,
                    InventorySelectionBehavior::PreserveCurrent);
            }

            if (pickedUp)
            {
                droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(itemIndex));
                continue;
            }
        }

        ++itemIndex;
    }
}
}  // namespace vibecraft::app
