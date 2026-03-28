#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

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
}  // namespace

MobSpawnSystem::MobSpawnSystem(const MobSpawnSettings& settings)
    : settings_(settings)
    , rng_(std::random_device{}())
{
}

const std::vector<EnemyInstance>& MobSpawnSystem::enemies() const
{
    return enemies_;
}

const MobSpawnSettings& MobSpawnSystem::settings() const
{
    return settings_;
}

void MobSpawnSystem::setRngSeedForTests(const std::uint32_t seed)
{
    rng_.seed(seed);
}

void MobSpawnSystem::clearAllMobs()
{
    enemies_.clear();
    spawnAccumulatorSeconds_ = 0.0f;
}

void MobSpawnSystem::despawnDistant(const glm::vec3& playerFeet)
{
    const float d = settings_.despawnHorizontalDistance;
    enemies_.erase(
        std::remove_if(
            enemies_.begin(),
            enemies_.end(),
            [&](const EnemyInstance& e)
            {
                const float dx = e.feetX - playerFeet.x;
                const float dz = e.feetZ - playerFeet.z;
                return std::sqrt(dx * dx + dz * dz) > d;
            }),
        enemies_.end());
}

void MobSpawnSystem::moveTowardPlayerAxis(
    const world::World& world,
    EnemyInstance& enemy,
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
        glm::vec3 feet{enemy.feetX, enemy.feetY, enemy.feetZ};
        const glm::vec3 basePosition = feet;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        const Aabb candidateAabb =
            aabbAtFeet(candidatePosition, settings_.mobHalfWidth, settings_.mobHeight);
        if (!collidesWithSolidBlock(world, candidateAabb))
        {
            feet = candidatePosition;
            enemy.feetX = feet.x;
            enemy.feetY = feet.y;
            enemy.feetZ = feet.z;
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
                    aabbAtFeet(sweepPosition, settings_.mobHalfWidth, settings_.mobHeight)))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feet[axisIndex] = basePosition[axisIndex] + step * low;
        enemy.feetX = feet.x;
        enemy.feetY = feet.y;
        enemy.feetZ = feet.z;
        break;
    }
}

void MobSpawnSystem::applyMelee(
    EnemyInstance& enemy,
    const glm::vec3& playerFeet,
    PlayerVitals& playerVitals)
{
    if (enemy.attackCooldownSeconds > 0.0f)
    {
        return;
    }

    const glm::vec3 mobFeet{enemy.feetX, enemy.feetY, enemy.feetZ};
    const glm::vec3 delta = playerFeet - mobFeet;
    const float dist = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (dist > settings_.meleeReach)
    {
        return;
    }

    static_cast<void>(playerVitals.applyDamage(
        DamageEvent{.cause = DamageCause::EnemyAttack, .amount = settings_.meleeDamage}));
    enemy.attackCooldownSeconds = settings_.attackCooldownSeconds;
}

bool MobSpawnSystem::trySpawnOne(
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
    if (enemies_.size() >= settings_.maxMobsNearPlayer)
    {
        return false;
    }

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
        const Aabb mobAabb = aabbAtFeet(candidateFeet, settings_.mobHalfWidth, settings_.mobHeight);

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
        for (const EnemyInstance& other : enemies_)
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

        enemies_.push_back(EnemyInstance{
            .id = nextId_++,
            .kind = MobKind::HostileStalker,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = std::atan2(playerFeet.x - feetX, playerFeet.z - feetZ),
            .attackCooldownSeconds = 0.0f,
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

    for (EnemyInstance& enemy : enemies_)
    {
        enemy.attackCooldownSeconds = std::max(0.0f, enemy.attackCooldownSeconds - deltaSeconds);
    }

    for (EnemyInstance& enemy : enemies_)
    {
        const glm::vec3 mobFeet{enemy.feetX, enemy.feetY, enemy.feetZ};
        glm::vec3 horiz = playerFeet - mobFeet;
        horiz.y = 0.0f;
        const float horizLen = glm::length(horiz);
        if (horizLen > kAabbEpsilon)
        {
            const glm::vec3 dir = horiz / horizLen;
            enemy.yawRadians = std::atan2(dir.x, dir.z);
            const float move = settings_.mobMoveSpeed * deltaSeconds;
            moveTowardPlayerAxis(world, enemy, 0, dir.x * move);
            moveTowardPlayerAxis(world, enemy, 2, dir.z * move);
        }

        const int mobIx = static_cast<int>(std::floor(enemy.feetX));
        const int mobIz = static_cast<int>(std::floor(enemy.feetZ));
        enemy.feetY = static_cast<float>(terrain.surfaceHeightAt(mobIx, mobIz)) + 1.0f;

        applyMelee(enemy, playerFeet, playerVitals);
    }

    if (!spawningEnabled)
    {
        return;
    }

    spawnAccumulatorSeconds_ += deltaSeconds;
    while (spawnAccumulatorSeconds_ >= settings_.spawnAttemptIntervalSeconds)
    {
        spawnAccumulatorSeconds_ -= settings_.spawnAttemptIntervalSeconds;
        static_cast<void>(trySpawnOne(world, terrain, playerFeet, playerHalfWidth, timePeriod));
    }
}
}  // namespace vibecraft::game
