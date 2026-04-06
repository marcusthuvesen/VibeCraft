#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/MobNavigation.hpp"
#include "vibecraft/game/mobs/HostileMobBehavior.hpp"
#include "vibecraft/game/mobs/MobBreeding.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"
#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <unordered_set>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool hasLineOfSightToPlayer(
    const world::World& world,
    const glm::vec3& attackOrigin,
    const glm::vec3& targetFeet)
{
    const glm::vec3 targetPoint = playerAimPointFromFeet(targetFeet);
    const glm::vec3 delta = targetPoint - attackOrigin;
    const float distance = glm::length(delta);
    if (distance <= kAabbEpsilon)
    {
        return true;
    }

    return !world.raycast(attackOrigin, delta / distance, distance - 0.05f).has_value();
}

[[nodiscard]] bool isMobSubmergedInFluid(const world::World& world, const MobInstance& mob)
{
    const int x = static_cast<int>(std::floor(mob.feetX));
    const int z = static_cast<int>(std::floor(mob.feetZ));
    const int yFeet = static_cast<int>(std::floor(mob.feetY));
    const int yMid = static_cast<int>(std::floor(mob.feetY + mob.height * 0.5f));
    for (const int y : {yFeet, yFeet + 1, yMid, yMid + 1})
    {
        if (world::isFluid(world.blockAt(x, y, z)))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasOpenSkyAbove(const world::World& world, const MobInstance& mob)
{
    const glm::vec3 origin(mob.feetX, mob.feetY + mob.height * 0.92f, mob.feetZ);
    return !world.raycast(origin, glm::vec3(0.0f, 1.0f, 0.0f), 420.0f).has_value();
}
}  // namespace

MobSpawnSystem::MobSpawnSystem(const MobSpawnSettings& settings)
    : settings_(settings)
    , rng_(std::random_device{}())
{
}

const std::vector<MobInstance>& MobSpawnSystem::mobs() const
{
    return mobs_;
}

const std::vector<HostileProjectile>& MobSpawnSystem::projectiles() const
{
    return projectiles_;
}

const MobSpawnSettings& MobSpawnSystem::settings() const
{
    return settings_;
}

std::vector<MobCombatEvent> MobSpawnSystem::takeCombatEvents()
{
    std::vector<MobCombatEvent> events = std::move(combatEvents_);
    combatEvents_.clear();
    return events;
}

void MobSpawnSystem::setSettings(const MobSpawnSettings& settings)
{
    settings_ = settings;
}

void MobSpawnSystem::setRngSeedForTests(const std::uint32_t seed)
{
    rng_.seed(seed);
}

void MobSpawnSystem::addMobForTests(MobInstance mob)
{
    nextId_ = std::max(nextId_, mob.id + 1);
    mobs_.push_back(std::move(mob));
}

void MobSpawnSystem::clearAllMobs()
{
    mobs_.clear();
    projectiles_.clear();
    combatEvents_.clear();
    hostileSpawnAccumulatorSeconds_ = 0.0f;
    passiveSpawnAccumulatorSeconds_ = 0.0f;
    daylightBurnSoundCooldownSeconds_ = 0.0f;
}

void MobSpawnSystem::spawnPlayerArrow(
    const glm::vec3& origin,
    const glm::vec3& directionUnit,
    const float speed,
    const float gravity,
    const float damage,
    const float radius,
    const float lifeSeconds)
{
    float directionLength = glm::length(directionUnit);
    if (directionLength <= kAabbEpsilon)
    {
        return;
    }
    const glm::vec3 direction = directionUnit / directionLength;
    projectiles_.push_back(HostileProjectile{
        .id = nextProjectileId_++,
        .ownerMobId = 0,
        .ownerMobKind = MobKind::Player,
        .kind = HostileProjectileKind::Arrow,
        .position = origin,
        .velocity = direction * speed,
        .radius = radius,
        .gravity = gravity,
        .damage = damage,
        .remainingLifeSeconds = lifeSeconds,
    });
}

void MobSpawnSystem::despawnDistant(
    const glm::vec3& hostFeet,
    const std::span<const glm::vec3> remotePlayerFeet)
{
    const float despawnDistance = settings_.despawnHorizontalDistance;
    const float despawnDistanceSq = despawnDistance * despawnDistance;
    mobs_.erase(
        std::remove_if(
            mobs_.begin(),
            mobs_.end(),
            [&](const MobInstance& mob)
            {
                const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};
                float minDistanceSq = horizontalDistSqXZ(mobFeet, hostFeet);
                for (const glm::vec3& remoteFeet : remotePlayerFeet)
                {
                    minDistanceSq = std::min(minDistanceSq, horizontalDistSqXZ(mobFeet, remoteFeet));
                }
                return minDistanceSq > despawnDistanceSq;
            }),
        mobs_.end());
}

void MobSpawnSystem::moveMobAxis(
    const world::World& world,
    MobInstance& mob,
    const int axisIndex,
    const float displacement)
{
    const glm::vec3 result = sweepMobAxis(
        world,
        settings_,
        mob,
        glm::vec3(mob.feetX, mob.feetY, mob.feetZ),
        axisIndex,
        displacement);
    mob.feetX = result.x;
    mob.feetY = result.y;
    mob.feetZ = result.z;
}

void MobSpawnSystem::tick(
    const world::World& world,
    const world::TerrainGenerator& terrain,
    const glm::vec3& playerFeet,
    const float playerHalfWidth,
    const float deltaSeconds,
    const TimeOfDayPeriod timePeriod,
    const float sunVisibility01,
    const bool spawningEnabled,
    PlayerVitals& playerVitals,
    const float playerDamageMultiplier,
    const std::span<const glm::vec3> remotePlayerFeetForMultiTarget,
    const std::span<float> remotePlayerHealthForMelee,
    const float remotePlayerMaxHealth,
    const float remotePlayerDamageMultiplier)
{
    const bool multiTarget =
        !remotePlayerFeetForMultiTarget.empty()
        && remotePlayerFeetForMultiTarget.size() == remotePlayerHealthForMelee.size();
    combatEvents_.clear();
    const std::span<const glm::vec3> spawnRemoteFeet =
        multiTarget ? remotePlayerFeetForMultiTarget : std::span<const glm::vec3>{};

    despawnDistant(playerFeet, spawnRemoteFeet);

    const std::span<const glm::vec3> remoteFeetForAi =
        multiTarget ? remotePlayerFeetForMultiTarget : std::span<const glm::vec3>{};
    const std::span<const float> remoteHealthForAi =
        multiTarget ? std::span<const float>(remotePlayerHealthForMelee.data(), remotePlayerHealthForMelee.size())
                    : std::span<const float>{};

    for (MobInstance& mob : mobs_)
    {
        if (isHostileMob(mob.kind))
        {
            mob.attackCooldownSeconds = std::max(0.0f, mob.attackCooldownSeconds - deltaSeconds);
            mob.pitchRadians = 0.0f;
        }
    }
    tickPassiveMobLifecycle(mobs_, settings_, deltaSeconds);

    std::unordered_set<std::uint32_t> mobIdsToEraseAfterCombat{};

    for (MobInstance& mob : mobs_)
    {
        const glm::vec3 initialFeet{mob.feetX, mob.feetY, mob.feetZ};
        glm::vec3 movementTargetFeet = initialFeet;

        if (isHostileMob(mob.kind))
        {
            const HostileMobBehavior behavior = hostileMobBehaviorForKind(mob.kind);
            const std::optional<LivingPlayerTarget> chaseTarget = findNearestLivingPlayer(
                initialFeet,
                playerFeet,
                playerVitals,
                multiTarget,
                remoteFeetForAi,
                remoteHealthForAi);
            if (chaseTarget.has_value())
            {
                movementTargetFeet = chaseTarget->feet;
                const glm::vec3 attackOrigin = hostileAttackOrigin(mob, behavior);
                const glm::vec3 targetAimPoint = playerAimPointFromFeet(chaseTarget->feet);
                glm::vec3 horizontalTarget = chaseTarget->feet - initialFeet;
                horizontalTarget.y = 0.0f;
                const float horizontalTargetLength = glm::length(horizontalTarget);
                const bool hasLineOfSight =
                    !behavior.requiresLineOfSight
                    || hasLineOfSightToPlayer(world, attackOrigin, chaseTarget->feet);
                const glm::vec3 aimDelta = targetAimPoint - attackOrigin;
                const float aimHorizontal = std::sqrt(aimDelta.x * aimDelta.x + aimDelta.z * aimDelta.z);
                if (glm::length(aimDelta) > kAabbEpsilon)
                {
                    mob.pitchRadians = std::atan2(aimDelta.y, std::max(aimHorizontal, kAabbEpsilon));
                }
                if (horizontalTargetLength > kAabbEpsilon)
                {
                    glm::vec3 preferredHorizontalDirection = horizontalTarget / horizontalTargetLength;
                    bool shouldMove = true;
                    if (behavior.usesProjectile && hasLineOfSight)
                    {
                        if (horizontalTargetLength < behavior.preferredMinDistance)
                        {
                            preferredHorizontalDirection = -preferredHorizontalDirection;
                        }
                        else if (horizontalTargetLength <= behavior.preferredMaxDistance)
                        {
                            shouldMove = false;
                        }
                    }
                    if (behavior.usesExplosion && hasLineOfSight
                        && horizontalTargetLength <= behavior.explosionPrimeHorizontalDistance + 0.02f)
                    {
                        shouldMove = false;
                    }
                    if (behavior.usesExplosion && mob.creeperFuseSeconds > 0.0f)
                    {
                        shouldMove = false;
                    }
                    if (shouldMove)
                    {
                        const float probeDistance = std::clamp(horizontalTargetLength, 2.0f, 4.5f);
                        const glm::vec3 direction = chooseSteeredMoveDirection(
                            world,
                            settings_,
                            mob,
                            initialFeet,
                            preferredHorizontalDirection,
                            chaseTarget->feet,
                            probeDistance);
                        mob.yawRadians = std::atan2(direction.x, direction.z);
                        const float movementDistance = hostileMoveSpeedForMob(settings_, mob.kind) * deltaSeconds;
                        moveMobAxis(world, mob, 0, direction.x * movementDistance);
                        moveMobAxis(world, mob, 2, direction.z * movementDistance);
                    }
                }
            }
        }
        else
        {
            mob.wanderTimerSeconds -= deltaSeconds;
            if (mob.wanderTimerSeconds <= 0.0f)
            {
                std::uniform_real_distribution<float> wanderTimerDist(
                    settings_.passiveWanderTimerMinSeconds,
                    settings_.passiveWanderTimerMaxSeconds);
                std::uniform_real_distribution<float> wanderYawDist(0.0f, 6.28318530718f);
                mob.wanderTimerSeconds = wanderTimerDist(rng_);
                mob.wanderYawRadians = wanderYawDist(rng_);
            }

            const std::optional<LivingPlayerTarget> nearestLiving = findNearestLivingPlayer(
                initialFeet,
                playerFeet,
                playerVitals,
                multiTarget,
                remoteFeetForAi,
                remoteHealthForAi);

            glm::vec3 direction{0.0f, 0.0f, 0.0f};
            float speed = settings_.passiveWanderSpeed;
            if (nearestLiving.has_value())
            {
                glm::vec3 horizontalToPlayer = nearestLiving->feet - initialFeet;
                horizontalToPlayer.y = 0.0f;
                const float playerDistance = glm::length(horizontalToPlayer);
                if (playerDistance < settings_.passiveFleePlayerDistance && playerDistance > kAabbEpsilon)
                {
                    direction = -horizontalToPlayer / playerDistance;
                    speed = settings_.passiveFleeSpeed;
                    movementTargetFeet = initialFeet + direction * settings_.passiveFleePlayerDistance;
                }
                else
                {
                    direction.x = std::sin(mob.wanderYawRadians);
                    direction.z = std::cos(mob.wanderYawRadians);
                    movementTargetFeet = initialFeet + direction * 4.0f;
                }
            }
            else
            {
                direction.x = std::sin(mob.wanderYawRadians);
                direction.z = std::cos(mob.wanderYawRadians);
                movementTargetFeet = initialFeet + direction * 4.0f;
            }

            const float directionLength = glm::length(direction);
            if (directionLength > kAabbEpsilon)
            {
                const float targetDeltaX = movementTargetFeet.x - initialFeet.x;
                const float targetDeltaZ = movementTargetFeet.z - initialFeet.z;
                const float probeDistance = std::clamp(
                    std::sqrt(targetDeltaX * targetDeltaX + targetDeltaZ * targetDeltaZ),
                    1.5f,
                    4.0f);
                const glm::vec3 steeredDirection = chooseSteeredMoveDirection(
                    world,
                    settings_,
                    mob,
                    initialFeet,
                    direction / directionLength,
                    movementTargetFeet,
                    probeDistance);
                mob.yawRadians = std::atan2(steeredDirection.x, steeredDirection.z);
                const float movementDistance = speed * deltaSeconds;
                moveMobAxis(world, mob, 0, steeredDirection.x * movementDistance);
                moveMobAxis(world, mob, 2, steeredDirection.z * movementDistance);
            }
        }

        mob.feetY = resolveMobFeetY(
            world,
            terrain,
            settings_,
            mob,
            glm::vec3(mob.feetX, mob.feetY, mob.feetZ),
            movementTargetFeet,
            deltaSeconds);

        if (isHostileMob(mob.kind))
        {
            const HostileMobBehavior behavior = hostileMobBehaviorForKind(mob.kind);
            const glm::vec3 attackOrigin = hostileAttackOrigin(mob, behavior);
            const std::optional<LivingPlayerTarget> target = findNearestLivingPlayer(
                glm::vec3(mob.feetX, mob.feetY, mob.feetZ),
                playerFeet,
                playerVitals,
                multiTarget,
                remoteFeetForAi,
                remoteHealthForAi);
            const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};
            if (target.has_value() && behavior.usesExplosion)
            {
                const float horizontalDistance = std::sqrt(horizontalDistSqXZ(mobFeet, target->feet));
                const bool los = hasLineOfSightToPlayer(world, attackOrigin, target->feet);
                if (horizontalDistance <= behavior.explosionPrimeHorizontalDistance && los)
                {
                    mob.creeperFuseSeconds += deltaSeconds;
                    mob.creeperFuseSoundCooldownSeconds -= deltaSeconds;
                    if (mob.creeperFuseSoundCooldownSeconds <= 0.0f
                        && mob.creeperFuseSeconds < behavior.explosionFuseSeconds)
                    {
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::CreeperFuseSound,
                            .actorKind = mob.kind,
                            .worldPosition = attackOrigin,
                        });
                        mob.creeperFuseSoundCooldownSeconds = 0.42f;
                    }
                    if (mob.creeperFuseSeconds >= behavior.explosionFuseSeconds)
                    {
                        const glm::vec3 center{mob.feetX, mob.feetY + mob.height * 0.5f, mob.feetZ};
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::CreeperExplosion,
                            .actorKind = mob.kind,
                            .worldPosition = center,
                            .blastRadiusBlocks = behavior.explosionBlastRadiusBlocks,
                        });
                        static_cast<void>(mobIdsToEraseAfterCombat.insert(mob.id));
                    }
                }
                else
                {
                    mob.creeperFuseSeconds = 0.0f;
                    mob.creeperFuseSoundCooldownSeconds = 0.0f;
                }
            }
            else if (behavior.usesExplosion)
            {
                mob.creeperFuseSeconds = 0.0f;
                mob.creeperFuseSoundCooldownSeconds = 0.0f;
            }
            else if (target.has_value() && behavior.usesProjectile)
            {
                const float horizontalDistance =
                    std::sqrt(horizontalDistSqXZ(glm::vec3(mob.feetX, mob.feetY, mob.feetZ), target->feet));
                if (mob.attackCooldownSeconds <= 0.0f
                    && horizontalDistance <= behavior.preferredMaxDistance
                    && hasLineOfSightToPlayer(world, attackOrigin, target->feet))
                {
                    glm::vec3 launchVector = playerAimPointFromFeet(target->feet) - attackOrigin;
                    launchVector.y += horizontalDistance * 0.05f;
                    const float launchLength = glm::length(launchVector);
                    if (launchLength > kAabbEpsilon)
                    {
                        projectiles_.push_back(HostileProjectile{
                            .id = nextProjectileId_++,
                            .ownerMobId = mob.id,
                            .ownerMobKind = mob.kind,
                            .kind = HostileProjectileKind::Arrow,
                            .position = attackOrigin,
                            .velocity = launchVector / launchLength * behavior.projectileSpeed,
                            .radius = behavior.projectileRadius,
                            .gravity = behavior.projectileGravity,
                            .damage = behavior.projectileDamage,
                            .remainingLifeSeconds = behavior.projectileLifeSeconds,
                        });
                        combatEvents_.push_back(MobCombatEvent{
                            .type = MobCombatEventType::ProjectileFired,
                            .actorKind = mob.kind,
                            .worldPosition = attackOrigin,
                            .projectileKind = HostileProjectileKind::Arrow,
                        });
                        mob.attackCooldownSeconds = behavior.attackCooldownSeconds;
                    }
                }
            }
            else
            {
                applyMelee(
                    mob,
                    attackOrigin,
                    playerFeet,
                    playerVitals,
                    playerDamageMultiplier,
                    multiTarget ? remotePlayerFeetForMultiTarget : std::span<const glm::vec3>{},
                    multiTarget ? remotePlayerHealthForMelee : std::span<float>{},
                    remotePlayerMaxHealth,
                    remotePlayerDamageMultiplier);
            }
        }
    }

    if (!mobIdsToEraseAfterCombat.empty())
    {
        mobs_.erase(
            std::remove_if(
                mobs_.begin(),
                mobs_.end(),
                [&](const MobInstance& mob)
                {
                    return mobIdsToEraseAfterCombat.contains(mob.id);
                }),
            mobs_.end());
    }

    applyDaylightBurn(world, sunVisibility01, deltaSeconds);

    tickProjectiles(
        world,
        playerHalfWidth,
        deltaSeconds,
        playerFeet,
        playerVitals,
        playerDamageMultiplier,
        multiTarget ? remotePlayerFeetForMultiTarget : std::span<const glm::vec3>{},
        multiTarget ? remotePlayerHealthForMelee : std::span<float>{},
        remotePlayerMaxHealth,
        remotePlayerDamageMultiplier);

    if (!spawningEnabled)
    {
        return;
    }

    tickPassiveBreeding(mobs_, settings_, rng_, deltaSeconds, nextId_);

    hostileSpawnAccumulatorSeconds_ += deltaSeconds;
    passiveSpawnAccumulatorSeconds_ += deltaSeconds;
    while (hostileSpawnAccumulatorSeconds_ >= settings_.spawnAttemptIntervalSeconds)
    {
        hostileSpawnAccumulatorSeconds_ -= settings_.spawnAttemptIntervalSeconds;
        static_cast<void>(trySpawnOneHostile(world, terrain, playerFeet, playerHalfWidth, timePeriod, spawnRemoteFeet));
    }
    while (passiveSpawnAccumulatorSeconds_ >= settings_.spawnAttemptIntervalSeconds)
    {
        passiveSpawnAccumulatorSeconds_ -= settings_.spawnAttemptIntervalSeconds;
        static_cast<void>(trySpawnOnePassive(world, terrain, playerFeet, playerHalfWidth, timePeriod, spawnRemoteFeet));
    }
}

void MobSpawnSystem::applyDaylightBurn(
    const world::World& world,
    const float sunVisibility01,
    const float deltaSeconds)
{
    constexpr float kMinSunVisibility = 0.04f;
    if (sunVisibility01 <= kMinSunVisibility || deltaSeconds <= 0.0f)
    {
        return;
    }

    const float sun = glm::clamp(sunVisibility01, 0.0f, 1.0f);
    // Minecraft-like: steady fire damage while exposed; ~1 HP/s at full noon for a 20 HP mob.
    constexpr float kFullSunDamagePerSecond = 1.0f;
    const float damage = kFullSunDamagePerSecond * sun * deltaSeconds;

    daylightBurnSoundCooldownSeconds_ = std::max(0.0f, daylightBurnSoundCooldownSeconds_ - deltaSeconds);
    bool emittedBurnSound = false;

    for (std::size_t i = 0; i < mobs_.size();)
    {
        MobInstance& mob = mobs_[i];
        if (!burnsInDaylight(mob.kind))
        {
            ++i;
            continue;
        }

        if (isMobSubmergedInFluid(world, mob) || !hasOpenSkyAbove(world, mob))
        {
            ++i;
            continue;
        }

        mob.health -= damage;
        if (mob.health <= 0.0f)
        {
            combatEvents_.push_back(MobCombatEvent{
                .type = MobCombatEventType::HostileMobBurnDeath,
                .actorKind = mob.kind,
                .worldPosition = glm::vec3(mob.feetX, mob.feetY + mob.height * 0.5f, mob.feetZ),
            });
            mobs_.erase(mobs_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        if (!emittedBurnSound && daylightBurnSoundCooldownSeconds_ <= 0.0f)
        {
            combatEvents_.push_back(MobCombatEvent{
                .type = MobCombatEventType::DaylightBurnDamage,
                .actorKind = mob.kind,
                .worldPosition = glm::vec3(mob.feetX, mob.feetY + mob.height * 0.5f, mob.feetZ),
            });
            daylightBurnSoundCooldownSeconds_ = 0.38f;
            emittedBurnSound = true;
        }
        ++i;
    }
}
}  // namespace vibecraft::game
