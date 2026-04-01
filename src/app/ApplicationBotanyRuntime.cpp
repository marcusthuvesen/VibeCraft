#include "vibecraft/app/ApplicationBotanyRuntime.hpp"

#include <glm/vec2.hpp>

#include <algorithm>
#include <array>
#include <cmath>

#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::app
{
namespace
{
constexpr float kBotanyPulseIntervalSeconds = 1.0f;
constexpr float kSaplingGrowthSeconds = 45.0f;
constexpr float kSproutGrowthThresholdSeconds = 18.0f;
constexpr int kBotanySearchRadiusBlocks = 56;

[[nodiscard]] std::int64_t packBlockKey(const int x, const int y, const int z)
{
    constexpr std::int64_t kMask26 = (static_cast<std::int64_t>(1) << 26) - 1;
    constexpr std::int64_t kMask12 = (static_cast<std::int64_t>(1) << 12) - 1;
    return ((static_cast<std::int64_t>(x) & kMask26) << 38)
        | ((static_cast<std::int64_t>(z) & kMask26) << 12)
        | (static_cast<std::int64_t>(y - vibecraft::world::kWorldMinY) & kMask12);
}

[[nodiscard]] bool isPositionInsideAnySafeZone(
    const std::vector<OxygenSafeZone>& safeZones,
    const glm::vec3& worldCenter,
    OxygenSafeZone* matchedZone = nullptr)
{
    for (const OxygenSafeZone& safeZone : safeZones)
    {
        const glm::vec3 delta = worldCenter - safeZone.center;
        if (glm::dot(delta, delta) <= safeZone.radius * safeZone.radius)
        {
            if (matchedZone != nullptr)
            {
                *matchedZone = safeZone;
            }
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isReplaceableByTree(const vibecraft::world::BlockType blockType)
{
    using B = vibecraft::world::BlockType;
    return blockType == B::Air || blockType == B::FiberSapling || blockType == B::FiberSprout;
}

[[nodiscard]] bool isBotanyGrowthBlock(const vibecraft::world::BlockType blockType)
{
    return blockType == vibecraft::world::BlockType::FiberSapling
        || blockType == vibecraft::world::BlockType::FiberSprout;
}

[[nodiscard]] int greenhouseStructureScore(
    const vibecraft::world::World& world,
    const glm::ivec3& saplingPosition)
{
    int score = 0;
    for (int dz = -2; dz <= 2; ++dz)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            for (int dy = -1; dy <= 2; ++dy)
            {
                const vibecraft::world::BlockType blockType =
                    world.blockAt(saplingPosition.x + dx, saplingPosition.y + dy, saplingPosition.z + dz);
                if (blockType == vibecraft::world::BlockType::GreenhouseGlass)
                {
                    score += 2;
                }
                else if (blockType == vibecraft::world::BlockType::HabitatFrame)
                {
                    score += 1;
                }
                else if (blockType == vibecraft::world::BlockType::PowerConduit)
                {
                    score += 2;
                }
            }
        }
    }
    return score;
}

[[nodiscard]] float growthSpeedMultiplier(const OxygenSafeZone& safeZone, const int structureScore)
{
    float multiplier = safeZone.generatorCount >= 2 ? 1.35f : 1.0f;
    if (structureScore >= 12)
    {
        multiplier += 0.55f;
    }
    else if (structureScore >= 6)
    {
        multiplier += 0.30f;
    }
    return multiplier;
}

[[nodiscard]] bool canGrowTreeAt(
    const vibecraft::world::World& world,
    const glm::ivec3& saplingPosition,
    const int trunkHeight,
    const int crownRadius)
{
    const int canopyTopY = saplingPosition.y + trunkHeight + crownRadius + 1;
    if (canopyTopY > vibecraft::world::kWorldMaxY)
    {
        return false;
    }
    if (world.blockAt(saplingPosition.x, saplingPosition.y - 1, saplingPosition.z)
        != vibecraft::world::BlockType::PlanterTray)
    {
        return false;
    }

    for (int y = saplingPosition.y; y <= canopyTopY; ++y)
    {
        if (!isReplaceableByTree(world.blockAt(saplingPosition.x, y, saplingPosition.z)))
        {
            return false;
        }
    }

    const int crownCenterY = saplingPosition.y + trunkHeight - 1;
    for (int dy = -crownRadius; dy <= 1; ++dy)
    {
        const int radius = dy <= -1 ? crownRadius : std::max(1, crownRadius - 1);
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (dx == 0 && dz == 0)
                {
                    continue;
                }
                const bool outerCorner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (outerCorner && radius >= 2)
                {
                    continue;
                }
                const int x = saplingPosition.x + dx;
                const int y = crownCenterY + dy;
                const int z = saplingPosition.z + dz;
                if (!isReplaceableByTree(world.blockAt(x, y, z)))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

[[nodiscard]] bool placeBlock(
    vibecraft::world::World& world,
    const glm::ivec3& position,
    const vibecraft::world::BlockType blockType)
{
    return world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = position,
        .blockType = blockType,
    });
}

[[nodiscard]] bool growTreeFromSapling(
    vibecraft::world::World& world,
    const glm::ivec3& saplingPosition,
    const OxygenSafeZone& safeZone,
    const int structureScore)
{
    const bool lushTree = safeZone.generatorCount >= 2 || structureScore >= 10;
    const int trunkHeight = lushTree ? 5 : 4;
    const int crownRadius = lushTree ? 3 : 2;
    const vibecraft::world::BlockType trunkBlock =
        lushTree ? vibecraft::world::BlockType::JungleTreeTrunk : vibecraft::world::BlockType::TreeTrunk;
    const vibecraft::world::BlockType crownBlock =
        lushTree ? vibecraft::world::BlockType::JungleTreeCrown : vibecraft::world::BlockType::TreeCrown;
    if (!canGrowTreeAt(world, saplingPosition, trunkHeight, crownRadius))
    {
        return false;
    }

    for (int y = saplingPosition.y; y < saplingPosition.y + trunkHeight; ++y)
    {
        if (!placeBlock(world, {saplingPosition.x, y, saplingPosition.z}, trunkBlock))
        {
            return false;
        }
    }

    const int crownCenterY = saplingPosition.y + trunkHeight - 1;
    for (int dy = -crownRadius; dy <= 1; ++dy)
    {
        const int radius = dy <= -1 ? crownRadius : std::max(1, crownRadius - 1);
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (dx == 0 && dz == 0)
                {
                    continue;
                }
                const bool outerCorner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (outerCorner && radius >= 2)
                {
                    continue;
                }
                if (!placeBlock(world, {saplingPosition.x + dx, crownCenterY + dy, saplingPosition.z + dz}, crownBlock))
                {
                    return false;
                }
            }
        }
    }

    return placeBlock(world, {saplingPosition.x, crownCenterY + 2, saplingPosition.z}, crownBlock);
}
}  // namespace

BotanyPlacementResult validateBotanyBlockPlacement(
    const vibecraft::world::World& world,
    const glm::ivec3& targetPosition,
    const vibecraft::world::BlockType blockType,
    const glm::vec3& playerFeetPosition,
    const bool creativeModeEnabled)
{
    if (blockType != vibecraft::world::BlockType::FiberSapling)
    {
        return {.allowed = true};
    }

    if (world.blockAt(targetPosition.x, targetPosition.y - 1, targetPosition.z) != vibecraft::world::BlockType::PlanterTray)
    {
        return {.allowed = false, .failureReason = "Fiber saplings need a planter tray."};
    }

    if (creativeModeEnabled)
    {
        return {.allowed = true};
    }

    const std::vector<OxygenSafeZone> safeZones = collectOxygenSafeZones(world, playerFeetPosition, 48, 24);
    const glm::vec3 worldCenter = glm::vec3(targetPosition) + glm::vec3(0.5f);
    if (!isPositionInsideAnySafeZone(safeZones, worldCenter))
    {
        return {.allowed = false, .failureReason = "Fiber saplings need oxygen to take root."};
    }

    return {.allowed = true};
}

BotanyTickResult tickLocalBotany(
    const float deltaTimeSeconds,
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& referencePosition,
    BotanyRuntimeState& runtimeState)
{
    static_cast<void>(terrainGenerator);

    BotanyTickResult result{};
    if (deltaTimeSeconds <= 0.0f)
    {
        return result;
    }

    runtimeState.pulseAccumulatorSeconds += deltaTimeSeconds;
    if (runtimeState.pulseAccumulatorSeconds < kBotanyPulseIntervalSeconds)
    {
        return result;
    }
    const float pulseSeconds = runtimeState.pulseAccumulatorSeconds;
    runtimeState.pulseAccumulatorSeconds = 0.0f;

    const std::vector<OxygenSafeZone> safeZones =
        collectOxygenSafeZones(world, referencePosition, kBotanySearchRadiusBlocks, 24);
    const glm::vec2 referenceXZ(referencePosition.x, referencePosition.z);
    std::unordered_map<std::int64_t, bool> seenSaplings;

    for (const auto& [coord, chunk] : world.chunks())
    {
        const int chunkMinX = coord.x * vibecraft::world::Chunk::kSize;
        const int chunkMinZ = coord.z * vibecraft::world::Chunk::kSize;
        const glm::vec2 chunkCenter(
            static_cast<float>(chunkMinX + vibecraft::world::Chunk::kSize / 2),
            static_cast<float>(chunkMinZ + vibecraft::world::Chunk::kSize / 2));
        if (glm::distance(chunkCenter, referenceXZ) > static_cast<float>(kBotanySearchRadiusBlocks + 24))
        {
            continue;
        }

        for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
        {
            for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
            {
                const int worldX = chunkMinX + localX;
                const int worldZ = chunkMinZ + localZ;
                for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
                {
                    const vibecraft::world::BlockType growthBlock = chunk.blockAt(localX, y, localZ);
                    if (!isBotanyGrowthBlock(growthBlock))
                    {
                        continue;
                    }

                    const std::int64_t key = packBlockKey(worldX, y, worldZ);
                    seenSaplings[key] = true;
                    ++result.saplingsTracked;
                    OxygenSafeZone matchedZone{};
                    const bool insideSafeZone = isPositionInsideAnySafeZone(
                        safeZones,
                        glm::vec3(static_cast<float>(worldX) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(worldZ) + 0.5f),
                        &matchedZone);
                    if (!insideSafeZone
                        || world.blockAt(worldX, y - 1, worldZ) != vibecraft::world::BlockType::PlanterTray)
                    {
                        runtimeState.saplingGrowthSeconds.erase(key);
                        continue;
                    }

                    const int structureScore = greenhouseStructureScore(world, {worldX, y, worldZ});
                    const float newGrowthSeconds =
                        runtimeState.saplingGrowthSeconds[key] + pulseSeconds * growthSpeedMultiplier(matchedZone, structureScore);
                    if (growthBlock == vibecraft::world::BlockType::FiberSapling
                        && newGrowthSeconds >= kSproutGrowthThresholdSeconds)
                    {
                        static_cast<void>(placeBlock(
                            world,
                            {worldX, y, worldZ},
                            vibecraft::world::BlockType::FiberSprout));
                    }
                    if (newGrowthSeconds + 0.001f < kSaplingGrowthSeconds)
                    {
                        runtimeState.saplingGrowthSeconds[key] = newGrowthSeconds;
                        continue;
                    }

                    if (growTreeFromSapling(world, {worldX, y, worldZ}, matchedZone, structureScore))
                    {
                        runtimeState.saplingGrowthSeconds.erase(key);
                        ++result.treesGrown;
                    }
                    else
                    {
                        runtimeState.saplingGrowthSeconds[key] = kSaplingGrowthSeconds - 5.0f;
                    }
                }
            }
        }
    }

    std::vector<std::int64_t> staleKeys;
    staleKeys.reserve(runtimeState.saplingGrowthSeconds.size());
    for (const auto& [key, growthSeconds] : runtimeState.saplingGrowthSeconds)
    {
        static_cast<void>(growthSeconds);
        if (!seenSaplings.contains(key))
        {
            staleKeys.push_back(key);
        }
    }
    for (const std::int64_t key : staleKeys)
    {
        runtimeState.saplingGrowthSeconds.erase(key);
    }

    return result;
}
}  // namespace vibecraft::app
