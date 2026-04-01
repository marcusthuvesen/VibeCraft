#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/app/Crafting.hpp"
#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/OxygenSystem.hpp"

namespace vibecraft::app
{
struct SingleplayerWorldMetadata
{
    std::string displayName;
    std::uint32_t seed = 0;
    std::int64_t createdUnixSeconds = 0;
    std::int64_t lastPlayedUnixSeconds = 0;
};

struct SavedDroppedItem
{
    vibecraft::world::BlockType blockType = vibecraft::world::BlockType::Air;
    EquippedItem equippedItem = EquippedItem::None;
    glm::vec3 worldPosition{0.0f};
    glm::vec3 velocity{0.0f};
    float ageSeconds = 0.0f;
    float pickupDelaySeconds = 0.25f;
    float spinRadians = 0.0f;
};

struct SingleplayerPlayerState
{
    glm::vec3 playerFeetPosition{0.0f};
    glm::vec3 spawnFeetPosition{0.0f};
    float cameraYawDegrees = -90.0f;
    float cameraPitchDegrees = -20.0f;
    float health = 20.0f;
    float air = 10.0f;
    vibecraft::game::OxygenState oxygenState{};
    bool creativeModeEnabled = false;
    std::uint8_t selectedHotbarIndex = 0;
    float dayNightElapsedSeconds = 0.0f;
    float weatherElapsedSeconds = 0.0f;
    HotbarSlots hotbarSlots{};
    BagSlots bagSlots{};
    EquipmentSlots equipmentSlots{};
    std::vector<SavedDroppedItem> droppedItems;
    std::unordered_map<std::int64_t, CraftingGridSlots> chestSlotsByPosition;
};

class SingleplayerSaveSerializer
{
  public:
    static bool saveMetadata(const SingleplayerWorldMetadata& metadata, const std::filesystem::path& outputPath);
    static std::optional<SingleplayerWorldMetadata> loadMetadata(const std::filesystem::path& inputPath);

    static bool savePlayerState(const SingleplayerPlayerState& state, const std::filesystem::path& outputPath);
    static std::optional<SingleplayerPlayerState> loadPlayerState(const std::filesystem::path& inputPath);
};
}  // namespace vibecraft::app
