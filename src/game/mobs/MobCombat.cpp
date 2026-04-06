#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/mobs/HostileMobBehavior.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool intersectRayAabb(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const Aabb& aabb,
    float& outHitDistance)
{
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

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
    return true;
}
}  // namespace

std::optional<std::size_t> findClosestMobIndexAlongRay(
    const std::vector<MobInstance>& mobs,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirection,
    const float maxDistance)
{
    if (maxDistance <= 0.0f || glm::length(rayDirection) <= kAabbEpsilon)
    {
        return std::nullopt;
    }

    const glm::vec3 direction = glm::normalize(rayDirection);
    std::size_t bestMobIndex = mobs.size();
    float bestHitDistance = maxDistance;

    for (std::size_t i = 0; i < mobs.size(); ++i)
    {
        const MobInstance& mob = mobs[i];
        const Aabb mobAabb = aabbAtFeet(
            glm::vec3{mob.feetX, mob.feetY, mob.feetZ},
            mob.halfWidth,
            mob.height);
        float hitDistance = 0.0f;
        if (!intersectRayAabb(rayOrigin, direction, mobAabb, hitDistance))
        {
            continue;
        }
        if (hitDistance < 0.0f || hitDistance > bestHitDistance)
        {
            continue;
        }
        bestHitDistance = hitDistance;
        bestMobIndex = i;
    }

    if (bestMobIndex >= mobs.size())
    {
        return std::nullopt;
    }

    return bestMobIndex;
}

std::optional<MobDamageResult> MobSpawnSystem::damageMobAtIndex(
    const world::World& world,
    const std::size_t mobIndex,
    const float damageAmount,
    const glm::vec3& attackerFeet,
    const glm::vec3& attackRayDirection,
    const float knockbackDistance)
{
    if (damageAmount <= 0.0f || mobIndex >= mobs_.size())
    {
        return std::nullopt;
    }

    const glm::vec3 direction = glm::length(attackRayDirection) > kAabbEpsilon
        ? glm::normalize(attackRayDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    MobInstance& target = mobs_[mobIndex];
    target.health = std::max(0.0f, target.health - damageAmount);
    if (target.health > 0.0f && knockbackDistance > kAabbEpsilon)
    {
        glm::vec3 knockbackDirection(
            target.feetX - attackerFeet.x,
            0.0f,
            target.feetZ - attackerFeet.z);
        if (glm::dot(knockbackDirection, knockbackDirection) <= kAabbEpsilon)
        {
            knockbackDirection = glm::vec3(direction.x, 0.0f, direction.z);
        }
        if (glm::dot(knockbackDirection, knockbackDirection) > kAabbEpsilon)
        {
            knockbackDirection = glm::normalize(knockbackDirection);
            target.yawRadians = std::atan2(knockbackDirection.x, knockbackDirection.z);
            moveMobAxis(world, target, 0, knockbackDirection.x * knockbackDistance);
            moveMobAxis(world, target, 2, knockbackDirection.z * knockbackDistance);
        }
    }
    MobDamageResult result{
        .mobId = target.id,
        .mobKind = target.kind,
        .feetPosition = glm::vec3(target.feetX, target.feetY, target.feetZ),
        .killed = target.health <= 0.0f,
    };

    if (result.killed)
    {
        mobs_.erase(mobs_.begin() + static_cast<std::ptrdiff_t>(mobIndex));
    }

    return result;
}

std::optional<MobDamageResult> MobSpawnSystem::damageClosestAlongRay(
    const world::World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirection,
    const float maxDistance,
    const float damageAmount,
    const glm::vec3& attackerFeet,
    const float knockbackDistance)
{
    if (damageAmount <= 0.0f || maxDistance <= 0.0f || glm::length(rayDirection) <= kAabbEpsilon)
    {
        return std::nullopt;
    }

    const glm::vec3 direction = glm::normalize(rayDirection);
    const std::optional<std::size_t> bestMobIndex =
        findClosestMobIndexAlongRay(mobs_, rayOrigin, rayDirection, maxDistance);
    if (!bestMobIndex.has_value())
    {
        return std::nullopt;
    }

    return damageMobAtIndex(world, *bestMobIndex, damageAmount, attackerFeet, direction, knockbackDistance);
}

void MobSpawnSystem::applyMelee(
    MobInstance& mob,
    const glm::vec3& mobAttackOrigin,
    const glm::vec3& hostPlayerFeet,
    PlayerVitals& playerVitals,
    const float hostDamageMultiplier,
    const std::span<const glm::vec3> remotePlayerFeet,
    const std::span<float> remotePlayerHealth,
    const float remotePlayerMaxHealth,
    const float remotePlayerDamageMultiplier)
{
    if (!isHostileMob(mob.kind))
    {
        return;
    }
    if (mob.kind == MobKind::Creeper)
    {
        return;
    }
    if (mob.attackCooldownSeconds > 0.0f)
    {
        return;
    }

    const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};
    const bool multiTarget =
        !remotePlayerFeet.empty() && remotePlayerFeet.size() == remotePlayerHealth.size();
    const std::optional<LivingPlayerTarget> target = findNearestLivingPlayer(
        mobFeet,
        hostPlayerFeet,
        playerVitals,
        multiTarget,
        remotePlayerFeet,
        remotePlayerHealth);
    if (!target.has_value())
    {
        return;
    }

    const HostileMobBehavior behavior = hostileMobBehaviorForKind(mob.kind);
    const float dist = std::sqrt(horizontalDistSqXZ(mobFeet, target->feet));
    if (dist > behavior.meleeReach)
    {
        return;
    }

    const float baseDamage = behavior.meleeDamage;
    if (target->index == 0)
    {
        const float clampedDamageMultiplier = std::clamp(hostDamageMultiplier, 0.0f, 1.0f);
        static_cast<void>(playerVitals.applyDamage(DamageEvent{
            .cause = DamageCause::EnemyAttack,
            .amount = baseDamage * clampedDamageMultiplier,
        }));
    }
    else
    {
        const std::size_t remoteIndex = target->index - 1;
        const float clampedRemoteMult = std::clamp(remotePlayerDamageMultiplier, 0.0f, 1.0f);
        const float amount = baseDamage * clampedRemoteMult;
        remotePlayerHealth[remoteIndex] =
            std::clamp(remotePlayerHealth[remoteIndex] - amount, 0.0f, remotePlayerMaxHealth);
    }
    combatEvents_.push_back(MobCombatEvent{
        .type = MobCombatEventType::MeleeAttack,
        .actorKind = mob.kind,
        .worldPosition = mobAttackOrigin,
        .projectileKind = HostileProjectileKind::Arrow,
    });
    mob.attackCooldownSeconds = behavior.attackCooldownSeconds;
}
}  // namespace vibecraft::game
