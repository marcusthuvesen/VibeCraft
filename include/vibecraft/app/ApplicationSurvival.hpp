#pragma once

#include <glm/vec3.hpp>

#include <vector>
#include <string>

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/OxygenSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/game/WeatherSystem.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct OxygenSafeZone
{
    glm::vec3 center{0.0f};
    float radius = 0.0f;
    std::size_t generatorCount = 0;
};

[[nodiscard]] const char* timeOfDayLabel(vibecraft::game::TimeOfDayPeriod period);
[[nodiscard]] const char* weatherLabel(vibecraft::game::WeatherType weatherType);
[[nodiscard]] const char* hazardLabel(const vibecraft::game::EnvironmentalHazards& hazards);
[[nodiscard]] vibecraft::game::OxygenEnvironment sampleOxygenEnvironment(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    bool creativeModeEnabled);
[[nodiscard]] std::string buildOxygenStatusLine(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::OxygenState& oxygenState,
    const vibecraft::game::OxygenEnvironment& oxygenEnvironment);
[[nodiscard]] const char* oxygenZoneLabel(
    const vibecraft::game::OxygenEnvironment& oxygenEnvironment);
[[nodiscard]] const char* oxygenBiomeGuidance(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition);
/// Short rotating tips for the first minutes of a session (empty after `kSurvivalTipDurationSeconds`).
[[nodiscard]] std::string survivalTipLine(float sessionPlaySeconds);
[[nodiscard]] std::vector<glm::vec3> collectNearbyOxygenGenerators(
    const vibecraft::world::World& world,
    const glm::vec3& playerFeetPosition);
[[nodiscard]] std::vector<glm::vec3> collectVisibleOxygenGenerators(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    int searchRadiusBlocks,
    std::size_t maxResults = 64);
[[nodiscard]] std::vector<OxygenSafeZone> buildOxygenSafeZones(
    const std::vector<glm::vec3>& generatorCenters);
[[nodiscard]] std::vector<OxygenSafeZone> collectOxygenSafeZones(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    int searchRadiusBlocks,
    std::size_t maxZones = 32);
[[nodiscard]] float encodeLegacyNetworkAir(const vibecraft::game::OxygenState& oxygenState);
void applyLegacyNetworkAirToOxygenSystem(
    vibecraft::game::OxygenSystem& oxygenSystem,
    float encodedAir);
}  // namespace vibecraft::app
