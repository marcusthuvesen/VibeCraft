#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
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

[[nodiscard]] float horizontalDistSqXZ(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

struct LivingPlayerTarget
{
    glm::vec3 feet{};
    /// 0 = host vitals, `1 + i` = remote index `i` in parallel feet/health spans.
    std::size_t index = 0;
};

[[nodiscard]] std::optional<LivingPlayerTarget> findNearestLivingPlayer(
    const glm::vec3& mobFeet,
    const glm::vec3& hostFeet,
    const PlayerVitals& hostVitals,
    const bool multiTarget,
    std::span<const glm::vec3> remoteFeet,
    std::span<const float> remoteHealth)
{
    std::optional<LivingPlayerTarget> best;
    float bestDistSq = std::numeric_limits<float>::infinity();

    if (!hostVitals.isDead())
    {
        const float d = horizontalDistSqXZ(mobFeet, hostFeet);
        best = LivingPlayerTarget{hostFeet, 0};
        bestDistSq = d;
    }

    if (multiTarget && remoteFeet.size() == remoteHealth.size())
    {
        for (std::size_t i = 0; i < remoteFeet.size(); ++i)
        {
            if (remoteHealth[i] <= 0.0f)
            {
                continue;
            }
            const float d = horizontalDistSqXZ(mobFeet, remoteFeet[i]);
            if (!best.has_value() || d < bestDistSq)
            {
                bestDistSq = d;
                best = LivingPlayerTarget{remoteFeet[i], 1 + i};
            }
        }
    }

    return best;
}

[[nodiscard]] glm::vec3 nearestPlayerFeetForFacing(
    const glm::vec3& candidateFeet,
    const glm::vec3& hostFeet,
    std::span<const glm::vec3> remoteFeet)
{
    glm::vec3 nearest = hostFeet;
    float bestD = horizontalDistSqXZ(candidateFeet, hostFeet);
    for (const glm::vec3& rf : remoteFeet)
    {
        const float d = horizontalDistSqXZ(candidateFeet, rf);
        if (d < bestD)
        {
            bestD = d;
            nearest = rf;
        }
    }
    return nearest;
}

[[nodiscard]] bool tooCloseToAnyPlayerFeet(
    const float feetX,
    const float feetZ,
    const glm::vec3& hostFeet,
    const float playerHalfWidth,
    const float minSeparation,
    std::span<const glm::vec3> remoteFeet)
{
    if (horizontalDistLessThan(
            feetX,
            feetZ,
            hostFeet.x,
            hostFeet.z,
            minSeparation + playerHalfWidth))
    {
        return true;
    }
    for (const glm::vec3& rf : remoteFeet)
    {
        if (horizontalDistLessThan(feetX, feetZ, rf.x, rf.z, minSeparation + playerHalfWidth))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hostileSpawnBlockedByTorch(
    const world::World& worldState,
    const glm::vec3& candidateFeet,
    const float exclusionRadius)
{
    if (exclusionRadius <= 0.0f)
    {
        return false;
    }

    const int centerX = static_cast<int>(std::floor(candidateFeet.x));
    const int centerZ = static_cast<int>(std::floor(candidateFeet.z));
    const int radiusBlocks = static_cast<int>(std::ceil(exclusionRadius));
    const float radiusSq = exclusionRadius * exclusionRadius;
    const int minY = static_cast<int>(std::floor(candidateFeet.y)) - 2;
    const int maxY = static_cast<int>(std::floor(candidateFeet.y)) + 3;

    for (int z = centerZ - radiusBlocks; z <= centerZ + radiusBlocks; ++z)
    {
        const float dz = static_cast<float>(z - centerZ);
        for (int x = centerX - radiusBlocks; x <= centerX + radiusBlocks; ++x)
        {
            const float dx = static_cast<float>(x - centerX);
            if (dx * dx + dz * dz > radiusSq)
            {
                continue;
            }

            for (int y = minY; y <= maxY; ++y)
            {
                if (worldState.blockAt(x, y, z) == world::BlockType::Torch)
                {
                    return true;
                }
            }
        }
    }

    return false;
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

[[nodiscard]] bool hasSolidSupportBelowAabb(const world::World& worldState, const Aabb& aabb)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x + kAabbEpsilon));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - kAabbEpsilon));
    const int minZ = static_cast<int>(std::floor(aabb.min.z + kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - kAabbEpsilon));
    const int supportY = static_cast<int>(std::floor(aabb.min.y - kAabbEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            if (world::isSolid(worldState.blockAt(x, supportY, z)))
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool tryStepOrJumpOverLedge(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& basePosition,
    const int axisIndex,
    const float step,
    glm::vec3& outFeetPosition)
{
    if (axisIndex == 1 || settings.mobStepHeight <= kAabbEpsilon)
    {
        return false;
    }

    const std::array<float, 3> liftCandidates{
        settings.mobStepHeight * 0.45f,
        settings.mobStepHeight * 0.75f,
        settings.mobStepHeight,
    };

    for (const float lift : liftCandidates)
    {
        if (lift <= kAabbEpsilon)
        {
            continue;
        }

        glm::vec3 lifted = basePosition;
        lifted.y += lift;
        const Aabb liftedAabb = aabbAtFeet(lifted, mob.halfWidth, mob.height);
        if (collidesWithSolidBlock(worldState, liftedAabb) || mobBodyTouchesFluid(worldState, liftedAabb))
        {
            continue;
        }

        glm::vec3 moved = lifted;
        moved[axisIndex] += step;
        const Aabb movedAabb = aabbAtFeet(moved, mob.halfWidth, mob.height);
        if (collidesWithSolidBlock(worldState, movedAabb) || mobBodyTouchesFluid(worldState, movedAabb))
        {
            continue;
        }
        if (!hasSolidSupportBelowAabb(worldState, movedAabb))
        {
            continue;
        }

        outFeetPosition = moved;
        return true;
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

[[nodiscard]] MobDimensionsForKind dimensionsForKind(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::Zombie:
        return {.halfWidth = 0.28f, .height = 1.75f};
    case MobKind::Player:
        return {.halfWidth = 0.30f, .height = 2.0f};
    case MobKind::Cow:
        return {.halfWidth = 0.45f, .height = 1.40f};
    case MobKind::Pig:
        return {.halfWidth = 0.42f, .height = 0.94f};
    case MobKind::Sheep:
        return {.halfWidth = 0.43f, .height = 1.24f};
    case MobKind::Chicken:
        return {.halfWidth = 0.20f, .height = 0.78f};
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
    const float d = settings_.despawnHorizontalDistance;
    const float dSq = d * d;
    mobs_.erase(
        std::remove_if(
            mobs_.begin(),
            mobs_.end(),
            [&](const MobInstance& e)
            {
                const glm::vec3 mobPos{e.feetX, e.feetY, e.feetZ};
                float minDistSq = horizontalDistSqXZ(mobPos, hostFeet);
                for (const glm::vec3& rf : remotePlayerFeet)
                {
                    minDistSq = std::min(minDistSq, horizontalDistSqXZ(mobPos, rf));
                }
                return minDistSq > dSq;
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

        // Horizontal movement can hop/step over one-block ledges when blocked.
        glm::vec3 steppedFeet{0.0f};
        if (tryStepOrJumpOverLedge(world, settings_, mob, basePosition, axisIndex, step, steppedFeet))
        {
            feet = steppedFeet;
            mob.feetX = feet.x;
            mob.feetY = feet.y;
            mob.feetZ = feet.z;
            remaining -= step;
            continue;
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

    const float dist = std::sqrt(horizontalDistSqXZ(mobFeet, target->feet));
    if (dist > settings_.meleeReach)
    {
        return;
    }

    const float baseDamage = settings_.meleeDamage;
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
    mob.attackCooldownSeconds = settings_.attackCooldownSeconds;
}

bool MobSpawnSystem::trySpawnOneHostile(
    const world::World& world,
    const world::TerrainGenerator& terrain,
    const glm::vec3& playerFeet,
    const float playerHalfWidth,
    const TimeOfDayPeriod timePeriod,
    const std::span<const glm::vec3> remotePlayerFeet)
{
    if (!isHostileSpawnPeriod(timePeriod))
    {
        return false;
    }
    if (countMobsMatching(mobs_, true) >= settings_.maxHostileMobsNearPlayer)
    {
        return false;
    }

    const MobDimensionsForKind dims = dimensionsForKind(MobKind::Zombie);

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

        if (tooCloseToAnyPlayerFeet(
                feetX,
                feetZ,
                playerFeet,
                playerHalfWidth,
                settings_.minSeparationFromPlayer,
                remotePlayerFeet))
        {
            continue;
        }
        if (hostileSpawnBlockedByTorch(world, candidateFeet, settings_.hostileTorchExclusionRadius))
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

        const world::SurfaceBiome surfaceBiome = terrain.surfaceBiomeAt(ix, iz);
        if (world::biomes::isJungleSurfaceBiome(surfaceBiome))
        {
            std::uniform_real_distribution<float> grovePeaceDist(0.0f, 1.0f);
            if (grovePeaceDist(rng_) < 0.58f)
            {
                continue;
            }
        }
        else if (world::biomes::isSandySurfaceBiome(surfaceBiome))
        {
            std::uniform_real_distribution<float> dustHostileDist(0.0f, 1.0f);
            if (dustHostileDist(rng_) < 0.12f)
            {
                continue;
            }
        }

        const glm::vec3 faceToward = nearestPlayerFeetForFacing(candidateFeet, playerFeet, remotePlayerFeet);
        mobs_.push_back(MobInstance{
            .id = nextId_++,
            .kind = MobKind::Zombie,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = std::atan2(faceToward.x - feetX, faceToward.z - feetZ),
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = 0.0f,
            .wanderYawRadians = 0.0f,
            .health = mobKindDefaultMaxHealth(MobKind::Zombie),
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
    const TimeOfDayPeriod timePeriod,
    const std::span<const glm::vec3> remotePlayerFeet)
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

        if (tooCloseToAnyPlayerFeet(
                feetX,
                feetZ,
                playerFeet,
                playerHalfWidth,
                settings_.minSeparationFromPlayer,
                remotePlayerFeet))
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
            .health = mobKindDefaultMaxHealth(kind),
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

    for (MobInstance& mob : mobs_)
    {
        const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};

        if (isHostileMob(mob.kind))
        {
            const std::optional<LivingPlayerTarget> chaseTarget = findNearestLivingPlayer(
                mobFeet,
                playerFeet,
                playerVitals,
                multiTarget,
                remoteFeetForAi,
                remoteHealthForAi);
            if (chaseTarget.has_value())
            {
                glm::vec3 horiz = chaseTarget->feet - mobFeet;
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
                mobFeet,
                playerFeet,
                playerVitals,
                multiTarget,
                remoteFeetForAi,
                remoteHealthForAi);

            glm::vec3 dir{0.0f, 0.0f, 0.0f};
            float speed = settings_.passiveWanderSpeed;
            if (nearestLiving.has_value())
            {
                glm::vec3 horizToPlayer = nearestLiving->feet - mobFeet;
                horizToPlayer.y = 0.0f;
                const float distToPlayer = glm::length(horizToPlayer);
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
