#include "vibecraft/game/MobSpawnSystem.hpp"

#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/game/MobNavigation.hpp"
#include "vibecraft/game/mobs/MobSpecies.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"
#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

#include <cmath>
#include <random>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool isHostileSpawnPeriod(const TimeOfDayPeriod period)
{
    return period == TimeOfDayPeriod::Night || period == TimeOfDayPeriod::Dusk;
}

[[nodiscard]] bool isPassiveSpawnPeriod(const TimeOfDayPeriod period)
{
    return period == TimeOfDayPeriod::Day || period == TimeOfDayPeriod::Dawn;
}

[[nodiscard]] std::size_t countMobsMatching(
    const std::vector<MobInstance>& mobs,
    const bool wantHostile)
{
    std::size_t count = 0;
    for (const MobInstance& mob : mobs)
    {
        if (isHostileMob(mob.kind) == wantHostile)
        {
            ++count;
        }
    }
    return count;
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

[[nodiscard]] MobKind randomHostileKind(std::mt19937& rng)
{
    // Weighted mix so zombies remain the most common baseline hostile.
    std::uniform_int_distribution<int> dist(0, 99);
    const int roll = dist(rng);
    if (roll < 40)
    {
        return MobKind::Zombie;
    }
    if (roll < 60)
    {
        return MobKind::Skeleton;
    }
    if (roll < 75)
    {
        return MobKind::Creeper;
    }
    if (roll < 85)
    {
        return MobKind::Spider;
    }
    if (roll < 93)
    {
        return MobKind::Wolf;
    }
    if (roll < 97)
    {
        return MobKind::Bear;
    }
    return MobKind::SandScorpion;
}

[[nodiscard]] bool tooCloseToAnyPlayerFeet(
    const float feetX,
    const float feetZ,
    const glm::vec3& hostFeet,
    const float playerHalfWidth,
    const float mobHalfWidth,
    const float minSeparation,
    const std::span<const glm::vec3> remoteFeet)
{
    if (horizontalDistLessThan(
            feetX,
            feetZ,
            hostFeet.x,
            hostFeet.z,
            minSeparation + playerHalfWidth + mobHalfWidth))
    {
        return true;
    }
    for (const glm::vec3& remoteFeetPosition : remoteFeet)
    {
        if (horizontalDistLessThan(
                feetX,
                feetZ,
                remoteFeetPosition.x,
                remoteFeetPosition.z,
                minSeparation + playerHalfWidth + mobHalfWidth))
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
                if (world::isTorchBlock(worldState.blockAt(x, y, z)))
                {
                    return true;
                }
            }
        }
    }

    return false;
}
}  // namespace

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

    const MobKind kind =
        countMobsMatching(mobs_, true) == 0 ? MobKind::Zombie : randomHostileKind(rng_);
    const MobDimensions dims = adultDimensionsForMobKind(kind);

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

        if (collidesWithSolidBlock(world, mobAabb) || mobBodyTouchesFluid(world, mobAabb))
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
                dims.halfWidth,
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
        if (kind == MobKind::SandScorpion && !world::biomes::isSandySurfaceBiome(surfaceBiome))
        {
            continue;
        }
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
            .kind = kind,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = std::atan2(faceToward.x - feetX, faceToward.z - feetZ),
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = 0.0f,
            .wanderYawRadians = 0.0f,
            .breedCooldownSeconds = 0.0f,
            .growthSecondsRemaining = 0.0f,
            .health = mobKindDefaultMaxHealth(kind),
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
    const MobDimensions dims = adultDimensionsForMobKind(kind);

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

        if (collidesWithSolidBlock(world, mobAabb) || mobBodyTouchesFluid(world, mobAabb))
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
                dims.halfWidth,
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

        const float wanderYaw = wanderYawDist(rng_);
        mobs_.push_back(MobInstance{
            .id = nextId_++,
            .kind = kind,
            .feetX = feetX,
            .feetY = feetY,
            .feetZ = feetZ,
            .yawRadians = wanderYaw,
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = wanderTimerDist(rng_),
            .wanderYawRadians = wanderYaw,
            .breedCooldownSeconds = 0.0f,
            .growthSecondsRemaining = 0.0f,
            .health = mobKindDefaultMaxHealth(kind),
            .halfWidth = dims.halfWidth,
            .height = dims.height,
        });
        return true;
    }

    return false;
}
}  // namespace vibecraft::game
