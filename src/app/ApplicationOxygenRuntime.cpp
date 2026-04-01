#include "vibecraft/app/ApplicationOxygenRuntime.hpp"

#include "vibecraft/app/ApplicationSurvival.hpp"

namespace vibecraft::app
{
vibecraft::game::OxygenEnvironment refreshPlayerOxygenEnvironment(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    const bool creativeModeEnabled)
{
    return sampleOxygenEnvironment(world, terrainGenerator, playerFeetPosition, hazards, creativeModeEnabled);
}

PlayerSurvivalOxygenTickResult tickPlayerSurvivalOxygen(
    const float deltaTimeSeconds,
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    const bool creativeModeEnabled,
    vibecraft::game::PlayerVitals& playerVitals,
    vibecraft::game::OxygenSystem& oxygenSystem)
{
    PlayerSurvivalOxygenTickResult result{
        .oxygenEnvironment =
            refreshPlayerOxygenEnvironment(world, terrainGenerator, playerFeetPosition, hazards, creativeModeEnabled),
        .playerTookDamage = false,
    };
    if (creativeModeEnabled)
    {
        // Creative mode keeps survival resources topped off while still exposing the local biome/safe-zone state.
        playerVitals.reset();
        oxygenSystem.setTankTier(oxygenSystem.state().tankTier, true);
        return result;
    }

    const float healthBeforeTick = playerVitals.health();
    playerVitals.tickEnvironment(deltaTimeSeconds, hazards);
    static_cast<void>(oxygenSystem.tick(deltaTimeSeconds, result.oxygenEnvironment, &playerVitals));
    result.playerTookDamage = playerVitals.health() + 0.001f < healthBeforeTick;
    return result;
}
}  // namespace vibecraft::app
