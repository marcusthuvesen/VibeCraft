#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"
#include "vibecraft/world/World.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool intersectRayAabbLimited(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const float maxDistance,
    const Aabb& aabb,
    float& outHitDistance)
{
    float tMin = 0.0f;
    float tMax = maxDistance;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float originComponent = origin[axis];
        const float directionComponent = direction[axis];
        const float minValue = aabb.min[axis];
        const float maxValue = aabb.max[axis];

        if (std::abs(directionComponent) <= kAabbEpsilon)
        {
            if (originComponent < minValue || originComponent > maxValue)
            {
                return false;
            }
            continue;
        }

        const float inverseDirection = 1.0f / directionComponent;
        float t0 = (minValue - originComponent) * inverseDirection;
        float t1 = (maxValue - originComponent) * inverseDirection;
        if (t0 > t1)
        {
            std::swap(t0, t1);
        }
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMax < tMin)
        {
            return false;
        }
    }

    outHitDistance = tMin;
    return outHitDistance <= maxDistance;
}

[[nodiscard]] Aabb playerAabbAtFeet(const glm::vec3& feetPosition, const float halfWidth)
{
    return aabbAtFeet(feetPosition, halfWidth, 2.0f);
}
}  // namespace

void MobSpawnSystem::tickProjectiles(
    const world::World& world,
    const float playerHalfWidth,
    const float deltaSeconds,
    const glm::vec3& hostPlayerFeet,
    PlayerVitals& playerVitals,
    const float hostDamageMultiplier,
    const std::span<const glm::vec3> remotePlayerFeet,
    const std::span<float> remotePlayerHealth,
    const float remotePlayerMaxHealth,
    const float remotePlayerDamageMultiplier)
{
    for (auto it = projectiles_.begin(); it != projectiles_.end();)
    {
        HostileProjectile& projectile = *it;
        projectile.remainingLifeSeconds -= deltaSeconds;
        if (projectile.remainingLifeSeconds <= 0.0f)
        {
            it = projectiles_.erase(it);
            continue;
        }

        const float speed = glm::length(projectile.velocity);
        const int stepCount = std::max(1, static_cast<int>(std::ceil(std::max(speed, 1.0f) * deltaSeconds / 0.35f)));
        const float stepSeconds = deltaSeconds / static_cast<float>(stepCount);
        bool destroyed = false;

        for (int step = 0; step < stepCount && !destroyed; ++step)
        {
            const glm::vec3 start = projectile.position;
            const glm::vec3 stepVelocity = projectile.velocity;
            const glm::vec3 delta = stepVelocity * stepSeconds;
            const float travelDistance = glm::length(delta);
            if (travelDistance <= kAabbEpsilon)
            {
                projectile.velocity.y -= projectile.gravity * stepSeconds;
                continue;
            }

            const glm::vec3 direction = delta / travelDistance;
            if (world.raycast(start, direction, travelDistance + projectile.radius).has_value())
            {
                combatEvents_.push_back(MobCombatEvent{
                    .type = MobCombatEventType::ProjectileHitBlock,
                    .actorKind = projectile.ownerMobKind,
                    .worldPosition = start + direction * travelDistance,
                    .projectileKind = projectile.kind,
                });
                destroyed = true;
                break;
            }

            if (projectile.ownerMobKind == MobKind::Player)
            {
                if (const std::optional<std::size_t> mobIndex = findClosestMobIndexAlongRay(
                        mobs_,
                        start,
                        direction,
                        travelDistance + projectile.radius);
                    mobIndex.has_value())
                {
                    if (const std::optional<MobDamageResult> mobDamage = damageMobAtIndex(
                            world,
                            *mobIndex,
                            projectile.damage,
                            hostPlayerFeet,
                            direction,
                            0.18f);
                        mobDamage.has_value())
                    {
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::ProjectileHitMob,
                            .actorKind = mobDamage->mobKind,
                            .worldPosition = mobDamage->feetPosition,
                            .projectileKind = projectile.kind,
                            .projectileMobHitLethal = mobDamage->killed,
                        });
                        destroyed = true;
                        break;
                    }
                }

                std::size_t hitPlayerIndex = std::numeric_limits<std::size_t>::max();
                float bestHitDistance = travelDistance + projectile.radius;
                for (std::size_t remoteIndex = 0; remoteIndex < remotePlayerFeet.size(); ++remoteIndex)
                {
                    if (remoteIndex >= remotePlayerHealth.size() || remotePlayerHealth[remoteIndex] <= 0.0f)
                    {
                        continue;
                    }
                    float hitDistance = 0.0f;
                    if (!intersectRayAabbLimited(
                            start,
                            direction,
                            travelDistance + projectile.radius,
                            playerAabbAtFeet(remotePlayerFeet[remoteIndex], 0.30f),
                            hitDistance))
                    {
                        continue;
                    }
                    if (hitDistance < bestHitDistance)
                    {
                        bestHitDistance = hitDistance;
                        hitPlayerIndex = 1 + remoteIndex;
                    }
                }

                if (hitPlayerIndex != std::numeric_limits<std::size_t>::max())
                {
                    const std::size_t remoteIndex = hitPlayerIndex - 1;
                    const float clampedDamageMultiplier = std::clamp(remotePlayerDamageMultiplier, 0.0f, 1.0f);
                    remotePlayerHealth[remoteIndex] = std::clamp(
                        remotePlayerHealth[remoteIndex] - projectile.damage * clampedDamageMultiplier,
                        0.0f,
                        remotePlayerMaxHealth);
                    combatEvents_.push_back(MobCombatEvent{
                        .type = MobCombatEventType::ProjectileHitPlayer,
                        .actorKind = projectile.ownerMobKind,
                        .worldPosition = start + direction * bestHitDistance,
                        .projectileKind = projectile.kind,
                    });
                    destroyed = true;
                    break;
                }
            }
            else
            {
                std::size_t hitPlayerIndex = std::numeric_limits<std::size_t>::max();
                float bestHitDistance = travelDistance + projectile.radius;
                if (!playerVitals.isDead())
                {
                    float hitDistance = 0.0f;
                    if (intersectRayAabbLimited(
                            start,
                            direction,
                            travelDistance + projectile.radius,
                            playerAabbAtFeet(hostPlayerFeet, playerHalfWidth),
                            hitDistance))
                    {
                        hitPlayerIndex = 0;
                        bestHitDistance = hitDistance;
                    }
                }
                for (std::size_t remoteIndex = 0; remoteIndex < remotePlayerFeet.size(); ++remoteIndex)
                {
                    if (remoteIndex >= remotePlayerHealth.size() || remotePlayerHealth[remoteIndex] <= 0.0f)
                    {
                        continue;
                    }
                    float hitDistance = 0.0f;
                    if (!intersectRayAabbLimited(
                            start,
                            direction,
                            travelDistance + projectile.radius,
                            playerAabbAtFeet(remotePlayerFeet[remoteIndex], 0.30f),
                            hitDistance))
                    {
                        continue;
                    }
                    if (hitDistance < bestHitDistance)
                    {
                        bestHitDistance = hitDistance;
                        hitPlayerIndex = 1 + remoteIndex;
                    }
                }

                if (hitPlayerIndex != std::numeric_limits<std::size_t>::max())
                {
                    if (hitPlayerIndex == 0)
                    {
                        const float clampedDamageMultiplier = std::clamp(hostDamageMultiplier, 0.0f, 1.0f);
                        static_cast<void>(playerVitals.applyDamage(DamageEvent{
                            .cause = DamageCause::EnemyAttack,
                            .amount = projectile.damage * clampedDamageMultiplier,
                        }));
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::ProjectileHitPlayer,
                            .actorKind = projectile.ownerMobKind,
                            .worldPosition = start + direction * bestHitDistance,
                            .projectileKind = projectile.kind,
                        });
                    }
                    else
                    {
                        const std::size_t remoteIndex = hitPlayerIndex - 1;
                        const float clampedDamageMultiplier = std::clamp(remotePlayerDamageMultiplier, 0.0f, 1.0f);
                        remotePlayerHealth[remoteIndex] = std::clamp(
                            remotePlayerHealth[remoteIndex] - projectile.damage * clampedDamageMultiplier,
                            0.0f,
                            remotePlayerMaxHealth);
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::ProjectileHitPlayer,
                            .actorKind = projectile.ownerMobKind,
                            .worldPosition = start + direction * bestHitDistance,
                            .projectileKind = projectile.kind,
                        });
                    }
                    destroyed = true;
                    break;
                }
            }

            projectile.position = start + delta;
            projectile.velocity.y -= projectile.gravity * stepSeconds;
        }

        if (destroyed)
        {
            it = projectiles_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
}  // namespace vibecraft::game
