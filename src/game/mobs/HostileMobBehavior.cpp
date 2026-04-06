#include "vibecraft/game/mobs/HostileMobBehavior.hpp"

#include <algorithm>

namespace vibecraft::game
{
HostileMobBehavior hostileMobBehaviorForKind(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::Zombie:
        return HostileMobBehavior{
            .moveSpeedMultiplier = 1.0f,
            .meleeReach = 1.30f,
            .meleeDamage = 3.0f,
            .attackCooldownSeconds = 1.0f,
            .preferredMinDistance = 0.0f,
            .preferredMaxDistance = 0.0f,
            .eyeHeightFraction = 0.82f,
        };
    case MobKind::Skeleton:
        return HostileMobBehavior{
            .moveSpeedMultiplier = 0.94f,
            .meleeReach = 1.10f,
            .meleeDamage = 2.0f,
            .attackCooldownSeconds = 1.65f,
            .preferredMinDistance = 5.5f,
            .preferredMaxDistance = 13.0f,
            .eyeHeightFraction = 0.88f,
            .usesProjectile = true,
            .requiresLineOfSight = true,
            .projectileSpeed = 15.0f,
            .projectileGravity = 8.5f,
            .projectileDamage = 4.0f,
            .projectileRadius = 0.14f,
            .projectileLifeSeconds = 2.4f,
        };
    case MobKind::Creeper:
        return HostileMobBehavior{
            .moveSpeedMultiplier = 0.96f,
            .meleeReach = 1.25f,
            .meleeDamage = 4.0f,
            .attackCooldownSeconds = 1.15f,
            .preferredMinDistance = 0.0f,
            .preferredMaxDistance = 0.0f,
            .eyeHeightFraction = 0.82f,
            .requiresLineOfSight = true,
            .usesExplosion = true,
            .explosionPrimeHorizontalDistance = 3.25f,
            .explosionFuseSeconds = 1.5f,
            .explosionBlastRadiusBlocks = 3.0f,
        };
    case MobKind::Spider:
        return HostileMobBehavior{
            .moveSpeedMultiplier = 1.28f,
            .meleeReach = 1.65f,
            .meleeDamage = 2.5f,
            .attackCooldownSeconds = 0.78f,
            .preferredMinDistance = 0.0f,
            .preferredMaxDistance = 0.0f,
            .eyeHeightFraction = 0.68f,
        };
    case MobKind::Player:
    case MobKind::Cow:
    case MobKind::Pig:
    case MobKind::Sheep:
    case MobKind::Chicken:
        break;
    }

    return HostileMobBehavior{};
}

float hostileMoveSpeedForMob(const MobSpawnSettings& settings, const MobKind kind)
{
    const HostileMobBehavior behavior = hostileMobBehaviorForKind(kind);
    return settings.mobMoveSpeed * std::max(0.1f, behavior.moveSpeedMultiplier);
}

glm::vec3 hostileAttackOrigin(const MobInstance& mob, const HostileMobBehavior& behavior)
{
    return glm::vec3(
        mob.feetX,
        mob.feetY + std::max(0.2f, mob.height * behavior.eyeHeightFraction),
        mob.feetZ);
}

glm::vec3 playerAimPointFromFeet(const glm::vec3& feetPosition)
{
    return feetPosition + glm::vec3(0.0f, 1.15f, 0.0f);
}
}  // namespace vibecraft::game
