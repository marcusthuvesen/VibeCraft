#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool horizontalDistLessThan(
    const float ax,
    const float az,
    const float bx,
    const float bz,
    const float threshold)
{
    const float dx = ax - bx;
    const float dz = az - bz;
    return (dx * dx + dz * dz) < threshold * threshold;
}

[[nodiscard]] bool mobBodyTouchesFluid(const world::World& worldState, const Aabb& aabb)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - kAabbEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - kAabbEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (world::isFluid(worldState.blockAt(x, y, z)))
                {
                    return true;
                }
            }
        }
    }
    return false;
}

[[nodiscard]] bool isHostileSpawnPeriod(const TimeOfDayPeriod period)
{
    return period == TimeOfDayPeriod::Night || period == TimeOfDayPeriod::Dusk;
}

[[nodiscard]] bool isPassiveSpawnPeriod(const TimeOfDayPeriod period)
{
    return period == TimeOfDayPeriod::Day || period == TimeOfDayPeriod::Dawn;
}

struct MobDimensionsForKind
{
    float halfWidth = 0.28f;
    float height = 1.75f;
};

[[nodiscard]] float healthForKind(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::HostileStalker:
        return 20.0f;
    case MobKind::Cow:
        return 10.0f;
    case MobKind::Pig:
        return 10.0f;
    case MobKind::Sheep:
        return 8.0f;
    case MobKind::Chicken:
        return 4.0f;
    }
    return 10.0f;
}

[[nodiscard]] MobDimensionsForKind dimensionsForKind(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::HostileStalker:
        return {.halfWidth = 0.28f, .height = 1.75f};
    case MobKind::Cow:
        return {.halfWidth = 0.46f, .height = 1.48f};
    case MobKind::Pig:
        return {.halfWidth = 0.36f, .height = 0.92f};
    case MobKind::Sheep:
        return {.halfWidth = 0.38f, .height = 1.28f};
    case MobKind::Chicken:
        return {.halfWidth = 0.16f, .height = 0.62f};
    }
    return {.halfWidth = 0.28f, .height = 1.75f};
}

[[nodiscard]] std::size_t countMobsMatching(
    const std::vector<MobInstance>& mobs,
    const bool wantHostile)
{
    std::size_t n = 0;
    for (const MobInstance& m : mobs)
    {
        if (isHostileMob(m.kind) == wantHostile)
        {
            ++n;
        }
    }
    return n;
}

[[nodiscard]] MobKind randomPassiveKind(std::mt19937& rng)
{
    std::uniform_int_distribution<int> dist(0, 3);
    switch (dist(rng))
    {
    case 0:
        return MobKind::Cow;
    case 1:
        return MobKind::Pig;
    case 2:
        return MobKind::Sheep;
    default:
        return MobKind::Chicken;
    }
}

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
        const float o = origin[axis];
        const float d = direction[axis];
        const float minV = aabb.min[axis];
        const float maxV = aabb.max[axis];

        if (std::abs(d) <= kAabbEpsilon)
        {
            if (o < minV || o > maxV)
            {
                return false;
            }
            continue;
        }

        const float invD = 1.0f / d;
        float t0 = (minV - o) * invD;
        float t1 = (maxV - o) * invD;
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
    std::size_t bestMobIndex = mobs_.size();
    float bestHitDistance = maxDistance;

    for (std::size_t i = 0; i < mobs_.size(); ++i)
    {
        const MobInstance& mob = mobs_[i];
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

    if (bestMobIndex >= mobs_.size())
    {
        return std::nullopt;
    }

    MobInstance& target = mobs_[bestMobIndex];
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
        mobs_.erase(mobs_.begin() + static_cast<std::ptrdiff_t>(bestMobIndex));
    }

    return result;
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

void MobSpawnSystem::despawnDistant(const glm::vec3& playerFeet)
{
    const float d = settings_.despawnHorizontalDistance;
    mobs_.erase(
        std::remove_if(
            mobs_.begin(),
            mobs_.end(),
            [&](const MobInstance& e)
            {
                const float dx = e.feetX - playerFeet.x;
                const float dz = e.feetZ - playerFeet.z;
                return std::sqrt(dx * dx + dz * dz) > d;
            }),
        mobs_.end());
}

void MobSpawnSystem::moveMobAxis(
    const world::World& world,
    MobInstance& mob,
    const int axisIndex,
    const float displacement)
{
    if (std::abs(displacement) <= kAabbEpsilon)
    {
        return;
    }

    float remaining = displacement;

    while (std::abs(remaining) > kAabbEpsilon)
    {
        const float step = std::clamp(
            remaining,
            -settings_.collisionSweepStep,
            settings_.collisionSweepStep);
        glm::vec3 feet{mob.feetX, mob.feetY, mob.feetZ};
        const glm::vec3 basePosition = feet;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        const Aabb candidateAabb = aabbAtFeet(candidatePosition, mob.halfWidth, mob.height);
        if (!collidesWithSolidBlock(world, candidateAabb))
        {
            feet = candidatePosition;
            mob.feetX = feet.x;
            mob.feetY = feet.y;
            mob.feetZ = feet.z;
            remaining -= step;
            continue;
        }

        // Horizontal movement can step up one block when blocked, instead of clipping into terrain.
        if (axisIndex != 1 && settings_.mobStepHeight > kAabbEpsilon)
        {
            glm::vec3 stepUpPosition = basePosition;
            stepUpPosition.y += settings_.mobStepHeight;
            const Aabb stepUpAabb = aabbAtFeet(stepUpPosition, mob.halfWidth, mob.height);
            if (!collidesWithSolidBlock(world, stepUpAabb) && !mobBodyTouchesFluid(world, stepUpAabb))
            {
                glm::vec3 steppedCandidate = stepUpPosition;
                steppedCandidate[axisIndex] += step;
                const Aabb steppedAabb = aabbAtFeet(steppedCandidate, mob.halfWidth, mob.height);
                if (!collidesWithSolidBlock(world, steppedAabb) && !mobBodyTouchesFluid(world, steppedAabb))
                {
                    const int supportX = static_cast<int>(std::floor(steppedCandidate.x));
                    const int supportY = static_cast<int>(std::floor(steppedCandidate.y - kAabbEpsilon));
                    const int supportZ = static_cast<int>(std::floor(steppedCandidate.z));
                    if (world::isSolid(world.blockAt(supportX, supportY, supportZ)))
                    {
                        feet = steppedCandidate;
                        mob.feetX = feet.x;
                        mob.feetY = feet.y;
                        mob.feetZ = feet.z;
                        remaining -= step;
                        continue;
                    }
                }
            }
        }

        float low = 0.0f;
        float high = 1.0f;
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float mid = (low + high) * 0.5f;
            glm::vec3 sweepPosition = basePosition;
            sweepPosition[axisIndex] += step * mid;
            if (collidesWithSolidBlock(
                    world,
                    aabbAtFeet(sweepPosition, mob.halfWidth, mob.height)))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feet[axisIndex] = basePosition[axisIndex] + step * low;
        mob.feetX = feet.x;
        mob.feetY = feet.y;
        mob.feetZ = feet.z;
        break;
    }
}

void MobSpawnSystem::applyMelee(
    MobInstance& mob,
    const glm::vec3& playerFeet,
    PlayerVitals& playerVitals)
{
    if (!isHostileMob(mob.kind))
    {
        return;
    }
    if (mob.attackCooldownSeconds > 0.0f)
    {
        return;
    }

    const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};
    const glm::vec3 delta = playerFeet - mobFeet;
    const float dist = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (dist > settings_.meleeReach)
    {
        return;
    }

    static_cast<void>(playerVitals.applyDamage(
        DamageEvent{.cause = DamageCause::EnemyAttack, .amount = settings_.meleeDamage}));
    mob.attackCooldownSeconds = settings_.attackCooldownSeconds;
}

bool MobSpawnSystem::trySpawnOneHostile(
    const world::World& world,
    const world::TerrainGenerator& terrain,
    const glm::vec3& playerFeet,
    const float playerHalfWidth,
    const TimeOfDayPeriod timePeriod)
{
    if (!isHostileSpawnPeriod(timePeriod))
    {
        return false;
    }
    if (countMobsMatching(mobs_, true) >= settings_.maxHostileMobsNearPlayer)
    {
        return false;
    }

    const MobDimensionsForKind dims = dimensionsForKind(MobKind::HostileStalker);

    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radiusDist(
        settings_.spawnMinHorizontalDistance,
        settings_.spawnMaxHorizontalDistance);

    for (int attempt = 0; attempt < 12; ++attempt)
    {
        const float angle = angleDist(rng_);
        const float radius = radiusDist(rng_);
        const float feetX = playerFeet.x + std::cos(angle) * radius;
        const float feetZ = playerFeet.z + std::sin(angle) * radius;

        const int ix = static_cast<int>(std::floor(feetX));
        const int iz = static_cast<int>(std::floor(feetZ));
        const int surfaceY = terrain.surfaceHeightAt(ix, iz);
        const float feetY = static_cast<float>(surfaceY) + 1.0f;

        const glm::vec3 candidateFeet{feetX, feetY, feetZ};
        const Aabb mobAabb = aabbAtFeet(candidateFeet, dims.halfWidth, dims.height);

        if (collidesWithSolidBlock(world, mobAabb))
        {
            continue;
        }
        if (mobBodyTouchesFluid(world, mobAabb))
        {
            continue;
        }

        const int belowY = static_cast<int>(std::floor(feetY - kAabbEpsilon));
        if (!world::isSolid(world.blockAt(ix, belowY, iz)))
        {
            continue;
        }

        if (horizontalDistLessThan(
                feetX,
                feetZ,
                playerFeet.x,
                playerFeet.z,
                settings_.minSeparationFromPlayer + playerHalfWidth))
        {
            continue;
        }

        bool tooCloseToMob = false;
        for (const MobInstance& other : mobs_)
        {
            if (horizontalDistLessThan(
                    feetX,
                    feetZ,
                    other.feetX,
                    other.feetZ,
                    settings_.minSeparationFromMob))
            {
                tooCloseToMob = true;
                break;
            }
        }
        if (tooCloseToMob)
        {
            continue;
        }

        mobs_.push_back(MobInstance{
            .id = nextId_++,
            .kind = MobKind::HostileStalker,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = std::atan2(playerFeet.x - feetX, playerFeet.z - feetZ),
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = 0.0f,
            .wanderYawRadians = 0.0f,
            .health = healthForKind(MobKind::HostileStalker),
            .halfWidth = dims.halfWidth,
            .height = dims.height,
        });
        return true;
    }

    return false;
}

bool MobSpawnSystem::trySpawnOnePassive(
    const world::World& world,
    const world::TerrainGenerator& terrain,
    const glm::vec3& playerFeet,
    const float playerHalfWidth,
    const TimeOfDayPeriod timePeriod)
{
    if (!isPassiveSpawnPeriod(timePeriod))
    {
        return false;
    }
    if (countMobsMatching(mobs_, false) >= settings_.maxPassiveMobsNearPlayer)
    {
        return false;
    }

    const MobKind kind = randomPassiveKind(rng_);
    const MobDimensionsForKind dims = dimensionsForKind(kind);

    std::uniform_real_distribution<float> angleDist(0.0f, 6.28318530718f);
    std::uniform_real_distribution<float> radiusDist(
        settings_.spawnMinHorizontalDistance,
        settings_.spawnMaxHorizontalDistance);
    std::uniform_real_distribution<float> wanderTimerDist(
        settings_.passiveWanderTimerMinSeconds,
        settings_.passiveWanderTimerMaxSeconds);
    std::uniform_real_distribution<float> wanderYawDist(0.0f, 6.28318530718f);

    for (int attempt = 0; attempt < 14; ++attempt)
    {
        const float angle = angleDist(rng_);
        const float radius = radiusDist(rng_);
        const float feetX = playerFeet.x + std::cos(angle) * radius;
        const float feetZ = playerFeet.z + std::sin(angle) * radius;

        const int ix = static_cast<int>(std::floor(feetX));
        const int iz = static_cast<int>(std::floor(feetZ));
        const int surfaceY = terrain.surfaceHeightAt(ix, iz);
        const float feetY = static_cast<float>(surfaceY) + 1.0f;

        const glm::vec3 candidateFeet{feetX, feetY, feetZ};
        const Aabb mobAabb = aabbAtFeet(candidateFeet, dims.halfWidth, dims.height);

        if (collidesWithSolidBlock(world, mobAabb))
        {
            continue;
        }
        if (mobBodyTouchesFluid(world, mobAabb))
        {
            continue;
        }

        const int belowY = static_cast<int>(std::floor(feetY - kAabbEpsilon));
        if (world.blockAt(ix, belowY, iz) != world::BlockType::Grass)
        {
            continue;
        }

        if (horizontalDistLessThan(
                feetX,
                feetZ,
                playerFeet.x,
                playerFeet.z,
                settings_.minSeparationFromPlayer + playerHalfWidth))
        {
            continue;
        }

        bool tooCloseToMob = false;
        for (const MobInstance& other : mobs_)
        {
            if (horizontalDistLessThan(
                    feetX,
                    feetZ,
                    other.feetX,
                    other.feetZ,
                    settings_.minSeparationFromMob))
            {
                tooCloseToMob = true;
                break;
            }
        }
        if (tooCloseToMob)
        {
            continue;
        }

        const float wy = wanderYawDist(rng_);
        mobs_.push_back(MobInstance{
            .id = nextId_++,
            .kind = kind,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = wy,
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = wanderTimerDist(rng_),
            .wanderYawRadians = wy,
            .health = healthForKind(kind),
            .halfWidth = dims.halfWidth,
            .height = dims.height,
        });
        return true;
    }

    return false;
}

void MobSpawnSystem::tick(
    const world::World& world,
    const world::TerrainGenerator& terrain,
    const glm::vec3& playerFeet,
    const float playerHalfWidth,
    const float deltaSeconds,
    const TimeOfDayPeriod timePeriod,
    const bool spawningEnabled,
    PlayerVitals& playerVitals)
{
    despawnDistant(playerFeet);

    for (MobInstance& mob : mobs_)
    {
        if (isHostileMob(mob.kind))
        {
            mob.attackCooldownSeconds = std::max(0.0f, mob.attackCooldownSeconds - deltaSeconds);
        }
    }

    for (MobInstance& mob : mobs_)
    {
        const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};

        if (isHostileMob(mob.kind))
        {
            glm::vec3 horiz = playerFeet - mobFeet;
            horiz.y = 0.0f;
            const float horizLen = glm::length(horiz);
            if (horizLen > kAabbEpsilon)
            {
                const glm::vec3 dir = horiz / horizLen;
                mob.yawRadians = std::atan2(dir.x, dir.z);
                const float move = settings_.mobMoveSpeed * deltaSeconds;
                moveMobAxis(world, mob, 0, dir.x * move);
                moveMobAxis(world, mob, 2, dir.z * move);
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

            glm::vec3 horizToPlayer = playerFeet - mobFeet;
            horizToPlayer.y = 0.0f;
            const float distToPlayer = glm::length(horizToPlayer);

            glm::vec3 dir{0.0f, 0.0f, 0.0f};
            float speed = settings_.passiveWanderSpeed;
            if (distToPlayer < settings_.passiveFleePlayerDistance && distToPlayer > kAabbEpsilon)
            {
                dir = -horizToPlayer / distToPlayer;
                speed = settings_.passiveFleeSpeed;
            }
            else
            {
                dir.x = std::sin(mob.wanderYawRadians);
                dir.z = std::cos(mob.wanderYawRadians);
            }

            mob.yawRadians = std::atan2(dir.x, dir.z);
            const float move = speed * deltaSeconds;
            moveMobAxis(world, mob, 0, dir.x * move);
            moveMobAxis(world, mob, 2, dir.z * move);
        }

        const int mobIx = static_cast<int>(std::floor(mob.feetX));
        const int mobIz = static_cast<int>(std::floor(mob.feetZ));
        const float previousFeetY = mob.feetY;
        const float desiredFeetY = static_cast<float>(terrain.surfaceHeightAt(mobIx, mobIz)) + 1.0f;
        mob.feetY = desiredFeetY;
        if (collidesWithSolidBlock(
                world,
                aabbAtFeet(glm::vec3(mob.feetX, mob.feetY, mob.feetZ), mob.halfWidth, mob.height)))
        {
            bool foundClearance = false;
            for (int up = 1; up <= 4; ++up)
            {
                mob.feetY = desiredFeetY + static_cast<float>(up);
                const Aabb liftedAabb = aabbAtFeet(
                    glm::vec3(mob.feetX, mob.feetY, mob.feetZ),
                    mob.halfWidth,
                    mob.height);
                if (!collidesWithSolidBlock(world, liftedAabb) && !mobBodyTouchesFluid(world, liftedAabb))
                {
                    foundClearance = true;
                    break;
                }
            }
            if (!foundClearance)
            {
                mob.feetY = previousFeetY;
            }
        }

        applyMelee(mob, playerFeet, playerVitals);
    }

    if (!spawningEnabled)
    {
        return;
    }

    hostileSpawnAccumulatorSeconds_ += deltaSeconds;
    passiveSpawnAccumulatorSeconds_ += deltaSeconds;
    while (hostileSpawnAccumulatorSeconds_ >= settings_.spawnAttemptIntervalSeconds)
    {
        hostileSpawnAccumulatorSeconds_ -= settings_.spawnAttemptIntervalSeconds;
        static_cast<void>(trySpawnOneHostile(world, terrain, playerFeet, playerHalfWidth, timePeriod));
    }
    while (passiveSpawnAccumulatorSeconds_ >= settings_.spawnAttemptIntervalSeconds)
    {
        passiveSpawnAccumulatorSeconds_ -= settings_.spawnAttemptIntervalSeconds;
        static_cast<void>(trySpawnOnePassive(world, terrain, playerFeet, playerHalfWidth, timePeriod));
    }
}
}  // namespace vibecraft::game
