#include "vibecraft/app/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace vibecraft::app
{
namespace
{
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

void Application::spawnDroppedItem(
    const world::BlockType blockType,
    const glm::ivec3& blockPosition)
{
    if (blockType == world::BlockType::Air || blockType == world::BlockType::Water
        || blockType == world::BlockType::Lava)
    {
        return;
    }

    if (blockType == world::BlockType::CoalOre)
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
        blockType,
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
                    selectedHotbarIndex_);
            }
            else
            {
                pickedUp = addBlockToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    droppedItem.blockType,
                    selectedHotbarIndex_);
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
