#pragma once

#include <glm/vec3.hpp>

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/MobSpawnSystem.hpp"

namespace vibecraft::world
{
class TerrainGenerator;
class World;
}

namespace vibecraft::game
{
[[nodiscard]] bool mobBodyTouchesFluid(const world::World& worldState, const Aabb& aabb);

[[nodiscard]] glm::vec3 sweepMobAxis(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& startFeet,
    int axisIndex,
    float displacement);

[[nodiscard]] glm::vec3 chooseSteeredMoveDirection(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& startFeet,
    const glm::vec3& preferredHorizontalDirection,
    const glm::vec3& targetFeet,
    float probeDistance);

[[nodiscard]] float resolveMobFeetY(
    const world::World& worldState,
    const world::TerrainGenerator& terrain,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& currentFeet,
    const glm::vec3& targetFeet,
    float deltaSeconds);
}  // namespace vibecraft::game
