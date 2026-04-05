#include "vibecraft/game/MobNavigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::game
{
namespace
{
constexpr float kPi = 3.14159265359f;

[[nodiscard]] bool aabbTouchesFluidType(const world::World& worldState, const Aabb& aabb, const world::BlockType fluidType)
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
                if (worldState.blockAt(x, y, z) == fluidType)
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

[[nodiscard]] bool occupancyBlocked(
    const world::World& worldState,
    const Aabb& aabb,
    const bool allowSwimming)
{
    if (collidesWithSolidBlock(worldState, aabb))
    {
        return true;
    }
    return !allowSwimming && mobBodyTouchesFluid(worldState, aabb);
}

[[nodiscard]] bool tryStepOrJumpOverLedge(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& basePosition,
    const int axisIndex,
    const float step,
    const bool allowSwimming,
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
        if (occupancyBlocked(worldState, liftedAabb, allowSwimming))
        {
            continue;
        }

        glm::vec3 moved = lifted;
        moved[axisIndex] += step;
        const Aabb movedAabb = aabbAtFeet(moved, mob.halfWidth, mob.height);
        if (occupancyBlocked(worldState, movedAabb, allowSwimming))
        {
            continue;
        }
        if (!allowSwimming && !hasSolidSupportBelowAabb(worldState, movedAabb))
        {
            continue;
        }

        outFeetPosition = moved;
        return true;
    }

    return false;
}

[[nodiscard]] float horizontalDistanceSquared(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}
}  // namespace

bool mobBodyTouchesFluid(const world::World& worldState, const Aabb& aabb)
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

glm::vec3 sweepMobAxis(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& startFeet,
    const int axisIndex,
    const float displacement)
{
    if (std::abs(displacement) <= kAabbEpsilon)
    {
        return startFeet;
    }

    glm::vec3 feet = startFeet;
    float remaining = displacement;
    const bool allowSwimming =
        mobBodyTouchesFluid(worldState, aabbAtFeet(startFeet, mob.halfWidth, mob.height));

    while (std::abs(remaining) > kAabbEpsilon)
    {
        const float step = std::clamp(
            remaining,
            -settings.collisionSweepStep,
            settings.collisionSweepStep);
        const glm::vec3 basePosition = feet;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        const Aabb candidateAabb = aabbAtFeet(candidatePosition, mob.halfWidth, mob.height);
        if (!occupancyBlocked(worldState, candidateAabb, allowSwimming))
        {
            feet = candidatePosition;
            remaining -= step;
            continue;
        }

        glm::vec3 steppedFeet{0.0f};
        if (tryStepOrJumpOverLedge(
                worldState,
                settings,
                mob,
                basePosition,
                axisIndex,
                step,
                allowSwimming,
                steppedFeet))
        {
            feet = steppedFeet;
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
            if (occupancyBlocked(
                    worldState,
                    aabbAtFeet(sweepPosition, mob.halfWidth, mob.height),
                    allowSwimming))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feet[axisIndex] = basePosition[axisIndex] + step * low;
        break;
    }

    return feet;
}

glm::vec3 chooseSteeredMoveDirection(
    const world::World& worldState,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& startFeet,
    const glm::vec3& preferredHorizontalDirection,
    const glm::vec3& targetFeet,
    const float probeDistance)
{
    glm::vec3 baseDir = preferredHorizontalDirection;
    baseDir.y = 0.0f;
    const float baseLen = glm::length(baseDir);
    if (baseLen <= kAabbEpsilon)
    {
        return glm::vec3{0.0f};
    }
    baseDir /= baseLen;

    const float baseYaw = std::atan2(baseDir.x, baseDir.z);
    const float currentDistanceSq = horizontalDistanceSquared(startFeet, targetFeet);

    std::array<float, 9> yawOffsets{
        0.0f,
        kPi / 8.0f,
        -kPi / 8.0f,
        kPi / 4.0f,
        -kPi / 4.0f,
        3.0f * kPi / 8.0f,
        -3.0f * kPi / 8.0f,
        kPi / 2.0f,
        -kPi / 2.0f,
    };

    glm::vec3 bestDir = baseDir;
    float bestScore = -std::numeric_limits<float>::infinity();
    bool directPathBlocked = false;
    float directMovedDistance = 0.0f;

    {
        glm::vec3 directProbe = sweepMobAxis(
            worldState,
            settings,
            mob,
            startFeet,
            0,
            baseDir.x * probeDistance);
        directProbe = sweepMobAxis(
            worldState,
            settings,
            mob,
            directProbe,
            2,
            baseDir.z * probeDistance);
        const float directDeltaX = directProbe.x - startFeet.x;
        const float directDeltaZ = directProbe.z - startFeet.z;
        directMovedDistance = std::sqrt(
            directDeltaX * directDeltaX + directDeltaZ * directDeltaZ);
        directPathBlocked = directMovedDistance < probeDistance * 0.75f;
    }

    // Fast path: when the straight direction is clear enough, avoid expensive steer sampling.
    // This reduces zig-zag jitter and frame spikes with many active mobs.
    if (!directPathBlocked && directMovedDistance >= probeDistance * 0.92f)
    {
        return baseDir;
    }

    if (directPathBlocked)
    {
        const glm::vec3 sideBasisLeft{-baseDir.z, 0.0f, baseDir.x};
        float bestDetourScore = std::numeric_limits<float>::infinity();
        std::optional<glm::vec3> bestDetourDir;
        for (const glm::vec3& sideDir : std::array<glm::vec3, 2>{sideBasisLeft, -sideBasisLeft})
        {
            glm::vec3 sideFeet = startFeet;
            for (int sidestep = 1; sidestep <= 6; ++sidestep)
            {
                const glm::vec3 before = sideFeet;
                sideFeet = sweepMobAxis(
                    worldState,
                    settings,
                    mob,
                    sideFeet,
                    0,
                    sideDir.x * 0.9f);
                sideFeet = sweepMobAxis(
                    worldState,
                    settings,
                    mob,
                    sideFeet,
                    2,
                    sideDir.z * 0.9f);

                const float sideDeltaX = sideFeet.x - before.x;
                const float sideDeltaZ = sideFeet.z - before.z;
                const float sidestepDistance = std::sqrt(
                    sideDeltaX * sideDeltaX + sideDeltaZ * sideDeltaZ);
                if (sidestepDistance < 0.45f)
                {
                    break;
                }

                glm::vec3 forwardProbe = sweepMobAxis(
                    worldState,
                    settings,
                    mob,
                    sideFeet,
                    0,
                    baseDir.x * probeDistance);
                forwardProbe = sweepMobAxis(
                    worldState,
                    settings,
                    mob,
                    forwardProbe,
                    2,
                    baseDir.z * probeDistance);

                const float forwardDeltaX = forwardProbe.x - sideFeet.x;
                const float forwardDeltaZ = forwardProbe.z - sideFeet.z;
                const float forwardDistance = std::sqrt(
                    forwardDeltaX * forwardDeltaX + forwardDeltaZ * forwardDeltaZ);
                if (forwardDistance < probeDistance * 0.75f)
                {
                    continue;
                }

                const float detourScore = static_cast<float>(sidestep)
                    + std::sqrt(horizontalDistanceSquared(sideFeet, targetFeet)) * 0.2f;
                if (detourScore < bestDetourScore)
                {
                    bestDetourScore = detourScore;
                    bestDetourDir = sideDir;
                }
                break;
            }
        }

        if (bestDetourDir.has_value())
        {
            return *bestDetourDir;
        }
    }

    for (const float offset : yawOffsets)
    {
        const float yaw = baseYaw + offset;
        const glm::vec3 candidateDir{std::sin(yaw), 0.0f, std::cos(yaw)};

        glm::vec3 probeFeet = sweepMobAxis(
            worldState,
            settings,
            mob,
            startFeet,
            0,
            candidateDir.x * probeDistance);
        probeFeet = sweepMobAxis(
            worldState,
            settings,
            mob,
            probeFeet,
            2,
            candidateDir.z * probeDistance);

        const float deltaX = probeFeet.x - startFeet.x;
        const float deltaZ = probeFeet.z - startFeet.z;
        const float movedDistance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
        if (movedDistance <= probeDistance * 0.2f)
        {
            continue;
        }

        const float candidateDistanceSq = horizontalDistanceSquared(probeFeet, targetFeet);
        const float turnPenalty = std::abs(offset) * 0.35f;
        float score = 0.0f;
        if (directPathBlocked && std::abs(offset) > kAabbEpsilon)
        {
            const float targetDistance = std::sqrt(candidateDistanceSq);
            const float detourBias = std::abs(offset) * 1.2f;
            score = movedDistance * 2.0f + detourBias - targetDistance * 0.15f;
        }
        else
        {
            const float progressScore = currentDistanceSq - candidateDistanceSq;
            score = progressScore + movedDistance * 0.75f - turnPenalty;
        }
        if (score > bestScore)
        {
            bestScore = score;
            bestDir = candidateDir;
        }
    }

    return bestDir;
}

float resolveMobFeetY(
    const world::World& worldState,
    const world::TerrainGenerator& terrain,
    const MobSpawnSettings& settings,
    const MobInstance& mob,
    const glm::vec3& currentFeet,
    const glm::vec3& targetFeet,
    const float deltaSeconds)
{
    const Aabb currentAabb = aabbAtFeet(currentFeet, mob.halfWidth, mob.height);
    const bool touchingFluid = mobBodyTouchesFluid(worldState, currentAabb);
    const bool touchingWater = aabbTouchesFluidType(worldState, currentAabb, world::BlockType::Water);
    if (touchingFluid && touchingWater)
    {
        const float swimSpeed = std::max(settings.mobMoveSpeed * 0.7f, 1.0f) * deltaSeconds;
        const float targetDelta = targetFeet.y - currentFeet.y;
        float desiredFeetY = currentFeet.y;
        if (targetDelta > 0.2f)
        {
            desiredFeetY += std::min(swimSpeed, targetDelta);
        }
        else if (targetDelta < -0.8f)
        {
            desiredFeetY += std::max(-swimSpeed * 0.6f, targetDelta);
        }
        else
        {
            desiredFeetY += swimSpeed * 0.25f;
        }

        for (const float probeOffset : std::array<float, 5>{0.0f, 0.25f, -0.2f, 0.5f, -0.4f})
        {
            const float probeFeetY = desiredFeetY + probeOffset;
            const Aabb probeAabb = aabbAtFeet(
                glm::vec3(currentFeet.x, probeFeetY, currentFeet.z),
                mob.halfWidth,
                mob.height);
            if (!collidesWithSolidBlock(worldState, probeAabb))
            {
                return probeFeetY;
            }
        }
        return currentFeet.y;
    }

    const int mobIx = static_cast<int>(std::floor(currentFeet.x));
    const int mobIz = static_cast<int>(std::floor(currentFeet.z));
    const float previousFeetY = currentFeet.y;
    const float desiredFeetY = static_cast<float>(terrain.surfaceHeightAt(mobIx, mobIz)) + 1.0f;
    float resolvedFeetY = desiredFeetY;
    if (collidesWithSolidBlock(
            worldState,
            aabbAtFeet(glm::vec3(currentFeet.x, resolvedFeetY, currentFeet.z), mob.halfWidth, mob.height)))
    {
        bool foundClearance = false;
        for (int up = 1; up <= 4; ++up)
        {
            resolvedFeetY = desiredFeetY + static_cast<float>(up);
            const Aabb liftedAabb = aabbAtFeet(
                glm::vec3(currentFeet.x, resolvedFeetY, currentFeet.z),
                mob.halfWidth,
                mob.height);
            if (!collidesWithSolidBlock(worldState, liftedAabb)
                && !mobBodyTouchesFluid(worldState, liftedAabb))
            {
                foundClearance = true;
                break;
            }
        }
        if (!foundClearance)
        {
            resolvedFeetY = previousFeetY;
        }
    }

    return resolvedFeetY;
}
}  // namespace vibecraft::game
