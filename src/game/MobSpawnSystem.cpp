#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/MobNavigation.hpp"
#include "vibecraft/game/mobs/MobBreeding.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>

namespace vibecraft::game
{
MobSpawnSystem::MobSpawnSystem(const MobSpawnSettings& settings)
    : settings_(settings)
    , rng_(std::random_device{}())
{
}

const std::vector<MobInstance>& MobSpawnSystem::mobs() const
{
    return mobs_;
}

const MobSpawnSettings& MobSpawnSystem::settings() const
{
    return settings_;
}

void MobSpawnSystem::setSettings(const MobSpawnSettings& settings)
{
    settings_ = settings;
}

void MobSpawnSystem::setRngSeedForTests(const std::uint32_t seed)
{
    rng_.seed(seed);
}

void MobSpawnSystem::clearAllMobs()
{
    mobs_.clear();
    hostileSpawnAccumulatorSeconds_ = 0.0f;
    passiveSpawnAccumulatorSeconds_ = 0.0f;
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
        }
    }
    tickPassiveMobLifecycle(mobs_, settings_, deltaSeconds);

    for (MobInstance& mob : mobs_)
    {
        const glm::vec3 initialFeet{mob.feetX, mob.feetY, mob.feetZ};
        glm::vec3 movementTargetFeet = initialFeet;

        if (isHostileMob(mob.kind))
        {
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
                glm::vec3 horizontalTarget = chaseTarget->feet - initialFeet;
                horizontalTarget.y = 0.0f;
                const float horizontalTargetLength = glm::length(horizontalTarget);
                if (horizontalTargetLength > kAabbEpsilon)
                {
                    const float probeDistance = std::clamp(horizontalTargetLength, 2.0f, 4.5f);
                    const glm::vec3 direction = chooseSteeredMoveDirection(
                        world,
                        settings_,
                        mob,
                        initialFeet,
                        horizontalTarget / horizontalTargetLength,
                        chaseTarget->feet,
                        probeDistance);
                    mob.yawRadians = std::atan2(direction.x, direction.z);
                    const float movementDistance = settings_.mobMoveSpeed * deltaSeconds;
                    moveMobAxis(world, mob, 0, direction.x * movementDistance);
                    moveMobAxis(world, mob, 2, direction.z * movementDistance);
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

        applyMelee(
            mob,
            playerFeet,
            playerVitals,
            playerDamageMultiplier,
            multiTarget ? remotePlayerFeetForMultiTarget : std::span<const glm::vec3>{},
            multiTarget ? remotePlayerHealthForMelee : std::span<float>{},
            remotePlayerMaxHealth,
            remotePlayerDamageMultiplier);
    }

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
}  // namespace vibecraft::game
