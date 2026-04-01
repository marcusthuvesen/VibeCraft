#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct BotanyPlacementResult
{
    bool allowed = false;
    std::string failureReason;
};

struct BotanyRuntimeState
{
    float pulseAccumulatorSeconds = 0.0f;
    std::unordered_map<std::int64_t, float> saplingGrowthSeconds;
};

struct BotanyTickResult
{
    std::size_t treesGrown = 0;
    std::size_t saplingsTracked = 0;
};

[[nodiscard]] BotanyPlacementResult validateBotanyBlockPlacement(
    const vibecraft::world::World& world,
    const glm::ivec3& targetPosition,
    vibecraft::world::BlockType blockType,
    const glm::vec3& playerFeetPosition,
    bool creativeModeEnabled);

[[nodiscard]] BotanyTickResult tickLocalBotany(
    float deltaTimeSeconds,
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& referencePosition,
    BotanyRuntimeState& runtimeState);
}  // namespace vibecraft::app
