#pragma once

#include <glm/vec3.hpp>

#include <string>
#include <vector>

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/game/WeatherSystem.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
/// Influence radius around industrial relay blocks (`BlockType::OxygenGenerator`) for greenhouse botany/terraforming.
struct GreenhouseZone
{
    glm::vec3 center{0.0f};
    float radius = 0.0f;
    std::size_t generatorCount = 0;
};

[[nodiscard]] const char* timeOfDayLabel(vibecraft::game::TimeOfDayPeriod period);
[[nodiscard]] const char* weatherLabel(vibecraft::game::WeatherType weatherType);
[[nodiscard]] const char* hazardLabel(const vibecraft::game::EnvironmentalHazards& hazards);
[[nodiscard]] const char* surfaceBiomeGuidance(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition);
[[nodiscard]] std::string survivalTipLine(float sessionPlaySeconds);
[[nodiscard]] std::vector<GreenhouseZone> buildGreenhouseZones(const std::vector<glm::vec3>& generatorCenters);
[[nodiscard]] std::vector<GreenhouseZone> collectGreenhouseZones(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    int searchRadiusBlocks,
    std::size_t maxZones = 32);
}  // namespace vibecraft::app
