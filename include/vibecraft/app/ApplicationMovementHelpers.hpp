#pragma once

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct AxisMoveResult
{
    bool blocked = false;
    float appliedDisplacement = 0.0f;
};

[[nodiscard]] vibecraft::game::Aabb playerAabbAt(const glm::vec3& feetPosition, float colliderHeight);
[[nodiscard]] bool aabbOverlapsBlockCell(const vibecraft::game::Aabb& aabb, const glm::ivec3& blockPosition);
[[nodiscard]] bool aabbTouchesBlockType(
    const vibecraft::world::World& worldState,
    const vibecraft::game::Aabb& aabb,
    vibecraft::world::BlockType blockType);
[[nodiscard]] bool isSpawnFeetPositionSafe(
    const vibecraft::world::World& worldState,
    const glm::vec3& feetPosition,
    float colliderHeight);
[[nodiscard]] bool canPlaceRelayAtTarget(
    const vibecraft::world::World& worldState,
    const glm::ivec3& buildTarget,
    const glm::vec3& playerFeetPosition,
    float colliderHeight);
[[nodiscard]] glm::vec3 findInitialSpawnFeetPosition(
    const vibecraft::world::World& worldState,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& preferredCameraPosition,
    float colliderHeight);
[[nodiscard]] std::optional<glm::vec3> findNearbyDrySpawnFeetPosition(
    const vibecraft::world::World& worldState,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& probePosition,
    float colliderHeight);
[[nodiscard]] AxisMoveResult movePlayerAxisWithCollision(
    const vibecraft::world::World& worldState,
    glm::vec3& feetPosition,
    int axisIndex,
    float displacement,
    float colliderHeight);
[[nodiscard]] bool tryStepUpAfterHorizontalBlock(
    const vibecraft::world::World& worldState,
    glm::vec3& feetPosition,
    int axisIndex,
    float remainingDisplacement,
    float colliderHeight);
/// True when movement is blocked by a ~1-block ledge in `wishXZ` that step-up may miss (Minecraft-style auto-jump).
[[nodiscard]] bool canAutoJumpOneBlockLedge(
    const vibecraft::world::World& worldState,
    const glm::vec3& feetPosition,
    const glm::vec2& wishXZ,
    float colliderHeight);
}  // namespace vibecraft::app
