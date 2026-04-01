#pragma once

#include <glm/vec3.hpp>

#include "vibecraft/game/OxygenSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct PlayerSurvivalOxygenTickResult
{
    vibecraft::game::OxygenEnvironment oxygenEnvironment{};
    bool playerTookDamage = false;
};

[[nodiscard]] vibecraft::game::OxygenEnvironment refreshPlayerOxygenEnvironment(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    bool creativeModeEnabled);

[[nodiscard]] PlayerSurvivalOxygenTickResult tickPlayerSurvivalOxygen(
    float deltaTimeSeconds,
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    bool creativeModeEnabled,
    vibecraft::game::PlayerVitals& playerVitals,
    vibecraft::game::OxygenSystem& oxygenSystem);
}  // namespace vibecraft::app
