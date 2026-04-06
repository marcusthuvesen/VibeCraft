#pragma once

#include "vibecraft/game/MobSpawnSystem.hpp"

#include <glm/vec3.hpp>

namespace vibecraft::game
{
struct HostileMobBehavior
{
    float moveSpeedMultiplier = 1.0f;
    float meleeReach = 1.15f;
    float meleeDamage = 2.0f;
    float attackCooldownSeconds = 1.1f;
    float preferredMinDistance = 0.0f;
    float preferredMaxDistance = 0.0f;
    float eyeHeightFraction = 0.82f;
    bool usesProjectile = false;
    bool requiresLineOfSight = false;
    float projectileSpeed = 14.0f;
    float projectileGravity = 12.5f;
    float projectileDamage = 4.0f;
    float projectileRadius = 0.16f;
    float projectileLifeSeconds = 2.0f;
    /// Creeper: timed blast instead of melee.
    bool usesExplosion = false;
    float explosionPrimeHorizontalDistance = 3.0f;
    float explosionFuseSeconds = 1.5f;
    float explosionBlastRadiusBlocks = 3.0f;
};

[[nodiscard]] HostileMobBehavior hostileMobBehaviorForKind(MobKind kind);
[[nodiscard]] float hostileMoveSpeedForMob(const MobSpawnSettings& settings, MobKind kind);
[[nodiscard]] glm::vec3 hostileAttackOrigin(const MobInstance& mob, const HostileMobBehavior& behavior);
[[nodiscard]] glm::vec3 playerAimPointFromFeet(const glm::vec3& feetPosition);
}  // namespace vibecraft::game
