#include "vibecraft/app/ApplicationMovementHelpers.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool aabbTouchesFluid(
    const world::World& worldState,
    const game::Aabb& aabb)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - game::kAabbEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - game::kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - game::kAabbEpsilon));

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

[[nodiscard]] bool feetRestOnDryLand(
    const world::World& worldState,
    const glm::vec3& feetPosition)
{
    const int belowX = static_cast<int>(std::floor(feetPosition.x));
    const int belowY = static_cast<int>(std::floor(feetPosition.y - kFloatEpsilon));
    const int belowZ = static_cast<int>(std::floor(feetPosition.z));
    if (belowY < world::kWorldMinY || belowY > world::kWorldMaxY)
    {
        return false;
    }
    const world::BlockType belowBlock = worldState.blockAt(belowX, belowY, belowZ);
    return world::isSolid(belowBlock) && !world::isFluid(belowBlock);
}
}  // namespace

game::Aabb playerAabbAt(const glm::vec3& feetPosition, const float colliderHeight)
{
    return game::aabbAtFeet(feetPosition, kPlayerMovementSettings.colliderHalfWidth, colliderHeight);
}

bool aabbOverlapsBlockCell(const game::Aabb& aabb, const glm::ivec3& blockPosition)
{
    const glm::vec3 blockMin(blockPosition);
    const glm::vec3 blockMax = blockMin + glm::vec3(1.0f);
    return aabb.min.x < blockMax.x && aabb.max.x > blockMin.x
        && aabb.min.y < blockMax.y && aabb.max.y > blockMin.y
        && aabb.min.z < blockMax.z && aabb.max.z > blockMin.z;
}

bool aabbTouchesBlockType(
    const world::World& worldState,
    const game::Aabb& aabb,
    const world::BlockType blockType)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - game::kAabbEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - game::kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - game::kAabbEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (worldState.blockAt(x, y, z) == blockType)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool isSpawnFeetPositionSafe(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight)
{
    const game::Aabb candidateAabb = playerAabbAt(feetPosition, colliderHeight);
    if (game::collidesWithSolidBlock(worldState, candidateAabb))
    {
        return false;
    }
    if (aabbTouchesFluid(worldState, candidateAabb))
    {
        return false;
    }
    return feetRestOnDryLand(worldState, feetPosition);
}

bool canPlaceRelayAtTarget(
    const world::World& worldState,
    const glm::ivec3& buildTarget,
    const glm::vec3& playerFeetPosition,
    const float colliderHeight)
{
    if (buildTarget.y < world::kWorldMinY || buildTarget.y > world::kWorldMaxY)
    {
        return false;
    }

    const world::BlockType existingType = worldState.blockAt(buildTarget.x, buildTarget.y, buildTarget.z);
    if (existingType != world::BlockType::Air && !world::blockMetadata(existingType).breakable)
    {
        return false;
    }

    return !aabbOverlapsBlockCell(playerAabbAt(playerFeetPosition, colliderHeight), buildTarget);
}

glm::vec3 findInitialSpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& preferredCameraPosition,
    const float colliderHeight)
{
    const int spawnWorldX = static_cast<int>(std::floor(preferredCameraPosition.x));
    const int spawnWorldZ = static_cast<int>(std::floor(preferredCameraPosition.z));
    glm::vec3 spawnFeetPosition(
        preferredCameraPosition.x,
        static_cast<float>(terrainGenerator.surfaceHeightAt(spawnWorldX, spawnWorldZ) + 1),
        preferredCameraPosition.z);

    constexpr int kSpawnClearanceSearchLimit = world::kWorldHeight;
    for (int attempt = 0; attempt <= kSpawnClearanceSearchLimit; ++attempt)
    {
        if (spawnFeetPosition.y >= static_cast<float>(world::kWorldMaxY) - colliderHeight)
        {
            break;
        }
        if (isSpawnFeetPositionSafe(worldState, spawnFeetPosition, colliderHeight))
        {
            return spawnFeetPosition;
        }
        spawnFeetPosition.y += 1.0f;
    }

    return spawnFeetPosition;
}

std::optional<glm::vec3> findNearbyDrySpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& probePosition,
    const float colliderHeight)
{
    constexpr int kSearchStep = 8;
    constexpr int kSearchRadius = 256;
    for (int radius = 0; radius <= kSearchRadius; radius += kSearchStep)
    {
        for (int dz = -radius; dz <= radius; dz += kSearchStep)
        {
            for (int dx = -radius; dx <= radius; dx += kSearchStep)
            {
                if (radius > 0 && std::abs(dx) != radius && std::abs(dz) != radius)
                {
                    continue;
                }
                const glm::vec3 candidateProbe{
                    probePosition.x + static_cast<float>(dx),
                    probePosition.y,
                    probePosition.z + static_cast<float>(dz),
                };
                const glm::vec3 candidateFeet = findInitialSpawnFeetPosition(
                    worldState,
                    terrainGenerator,
                    candidateProbe,
                    colliderHeight);
                if (isSpawnFeetPositionSafe(worldState, candidateFeet, colliderHeight))
                {
                    return candidateFeet;
                }
            }
        }
    }
    return std::nullopt;
}

AxisMoveResult movePlayerAxisWithCollision(
    const world::World& worldState,
    glm::vec3& feetPosition,
    const int axisIndex,
    const float displacement,
    const float colliderHeight)
{
    if (std::abs(displacement) <= kFloatEpsilon)
    {
        return {};
    }

    float remaining = displacement;
    bool blocked = false;
    const float startPosition = feetPosition[axisIndex];

    while (std::abs(remaining) > kFloatEpsilon)
    {
        const float step = std::clamp(
            remaining,
            -kPlayerMovementSettings.collisionSweepStep,
            kPlayerMovementSettings.collisionSweepStep);
        const glm::vec3 basePosition = feetPosition;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        if (!game::collidesWithSolidBlock(worldState, playerAabbAt(candidatePosition, colliderHeight)))
        {
            feetPosition = candidatePosition;
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
            if (game::collidesWithSolidBlock(worldState, playerAabbAt(sweepPosition, colliderHeight)))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feetPosition[axisIndex] += step * low;
        blocked = true;
        break;
    }

    return AxisMoveResult{
        .blocked = blocked,
        .appliedDisplacement = feetPosition[axisIndex] - startPosition,
    };
}

bool tryStepUpAfterHorizontalBlock(
    const world::World& worldState,
    glm::vec3& feetPosition,
    const int axisIndex,
    const float remainingDisplacement,
    const float colliderHeight)
{
    if (axisIndex == 1 || std::abs(remainingDisplacement) <= kFloatEpsilon)
    {
        return false;
    }

    glm::vec3 steppedPosition = feetPosition;
    const AxisMoveResult stepUpResult = movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        1,
        kPlayerMovementSettings.maxStepHeight,
        colliderHeight);
    if (stepUpResult.blocked || stepUpResult.appliedDisplacement < kPlayerMovementSettings.maxStepHeight * 0.5f)
    {
        return false;
    }

    const AxisMoveResult horizontalResult = movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        axisIndex,
        remainingDisplacement,
        colliderHeight);
    if (horizontalResult.blocked)
    {
        return false;
    }

    static_cast<void>(movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        1,
        -(kPlayerMovementSettings.maxStepHeight + kPlayerMovementSettings.groundProbeDistance + kFloatEpsilon),
        colliderHeight));
    glm::vec3 groundedProbe = steppedPosition;
    groundedProbe.y -= kPlayerMovementSettings.groundProbeDistance;
    if (!game::collidesWithSolidBlock(worldState, playerAabbAt(groundedProbe, colliderHeight)))
    {
        return false;
    }
    if (steppedPosition.y <= feetPosition.y + kFloatEpsilon)
    {
        return false;
    }

    feetPosition = steppedPosition;
    return true;
}

bool canAutoJumpOneBlockLedge(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const glm::vec2& wishXZ,
    const float colliderHeight)
{
    const float len = glm::length(wishXZ);
    if (len < 0.0001f)
    {
        return false;
    }
    const glm::vec2 dir = wishXZ / len;
    constexpr float kProbe = 0.38f;
    const int fx = static_cast<int>(std::floor(feetPosition.x + dir.x * kProbe));
    const int fz = static_cast<int>(std::floor(feetPosition.z + dir.y * kProbe));
    const int fy = static_cast<int>(std::floor(feetPosition.y));

    const world::BlockType low = worldState.blockAt(fx, fy, fz);
    const world::BlockType mid = worldState.blockAt(fx, fy + 1, fz);
    const world::BlockType high = worldState.blockAt(fx, fy + 2, fz);

    if (!world::isSolid(low))
    {
        return false;
    }
    if (world::isSolid(mid) && world::isSolid(high))
    {
        return false;
    }
    if (world::isSolid(mid) && !world::isSolid(high))
    {
        return false;
    }
    if (!world::isSolid(mid))
    {
        if (game::collidesWithSolidBlock(
                worldState, playerAabbAt(feetPosition + glm::vec3(0.0f, 1.05f, 0.0f), colliderHeight)))
        {
            return false;
        }
        return true;
    }

    return false;
}
}  // namespace vibecraft::app
