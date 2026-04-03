#include "vibecraft/app/ApplicationSurvival.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <queue>
#include <string>
#include <unordered_set>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::app
{
namespace
{
constexpr int kOxygenGeneratorRadiusBlocks = 7;
constexpr int kAirlockOutletRadiusBlocks = 4;
constexpr int kPoweredAirlockConduitReachBlocks = 12;
constexpr float kGeneratorStackingBonusPerRelay = 2.35f;
constexpr float kLegacyNetworkAirCapacity = 10.0f;
constexpr float kLegacyNetworkAirTierStride = 100.0f;

[[nodiscard]] glm::ivec3 blockPositionFromCenter(const glm::vec3& center)
{
    return {
        static_cast<int>(std::floor(center.x)),
        static_cast<int>(std::floor(center.y)),
        static_cast<int>(std::floor(center.z)),
    };
}

[[nodiscard]] glm::vec3 blockCenter(const glm::ivec3& blockPosition)
{
    return glm::vec3(blockPosition) + glm::vec3(0.5f);
}

[[nodiscard]] std::int64_t packBlockKey(const glm::ivec3& blockPosition)
{
    constexpr std::int64_t kComponentMask = (static_cast<std::int64_t>(1) << 21) - 1;
    const auto encodeComponent = [](const int value) -> std::int64_t {
        return static_cast<std::int64_t>(value) & kComponentMask;
    };
    return (encodeComponent(blockPosition.x) << 42)
        | (encodeComponent(blockPosition.y) << 21)
        | encodeComponent(blockPosition.z);
}

[[nodiscard]] bool isPoweredAirlockNetworkBlock(const vibecraft::world::BlockType blockType)
{
    return blockType == vibecraft::world::BlockType::OxygenGenerator
        || blockType == vibecraft::world::BlockType::PowerConduit;
}

[[nodiscard]] std::vector<glm::vec3> collectPoweredAirlockCenters(
    const vibecraft::world::World& world,
    const std::vector<glm::vec3>& generatorCenters,
    const glm::vec3& referencePosition,
    const std::size_t maxResults)
{
    std::vector<glm::vec3> airlockCenters;
    if (generatorCenters.empty() || maxResults == 0)
    {
        return airlockCenters;
    }

    static constexpr std::array<glm::ivec3, 6> kNeighborOffsets{{
        {1, 0, 0},
        {-1, 0, 0},
        {0, 1, 0},
        {0, -1, 0},
        {0, 0, 1},
        {0, 0, -1},
    }};

    const glm::vec3 referenceCenter = referencePosition + glm::vec3(0.0f, 0.9f, 0.0f);
    const float maxGeneratorDistance =
        static_cast<float>(kOxygenGeneratorRadiusBlocks + kPoweredAirlockConduitReachBlocks + kAirlockOutletRadiusBlocks);
    const float radiusSq = maxGeneratorDistance * maxGeneratorDistance;
    std::queue<std::pair<glm::ivec3, int>> frontier;
    std::unordered_set<std::int64_t> visitedNetworkBlocks;
    std::unordered_set<std::int64_t> recordedAirlocks;

    for (const glm::vec3& center : generatorCenters)
    {
        const glm::ivec3 generatorBlock = blockPositionFromCenter(center);
        const std::int64_t key = packBlockKey(generatorBlock);
        if (visitedNetworkBlocks.insert(key).second)
        {
            frontier.push({generatorBlock, 0});
        }
    }

    while (!frontier.empty() && airlockCenters.size() < maxResults)
    {
        const auto [currentBlock, stepsFromGenerator] = frontier.front();
        frontier.pop();

        for (const glm::ivec3& offset : kNeighborOffsets)
        {
            const glm::ivec3 candidateBlock = currentBlock + offset;
            const vibecraft::world::BlockType blockType =
                world.blockAt(candidateBlock.x, candidateBlock.y, candidateBlock.z);

            if (blockType == vibecraft::world::BlockType::AirlockPanel)
            {
                const glm::vec3 center = blockCenter(candidateBlock);
                const glm::vec3 delta = center - referenceCenter;
                if (glm::dot(delta, delta) <= radiusSq)
                {
                    const std::int64_t airlockKey = packBlockKey(candidateBlock);
                    if (recordedAirlocks.insert(airlockKey).second)
                    {
                        airlockCenters.push_back(center);
                        if (airlockCenters.size() >= maxResults)
                        {
                            return airlockCenters;
                        }
                    }
                }
                continue;
            }

            if (stepsFromGenerator >= kPoweredAirlockConduitReachBlocks || !isPoweredAirlockNetworkBlock(blockType))
            {
                continue;
            }

            const std::int64_t candidateKey = packBlockKey(candidateBlock);
            if (visitedNetworkBlocks.insert(candidateKey).second)
            {
                frontier.push({candidateBlock, stepsFromGenerator + 1});
            }
        }
    }

    return airlockCenters;
}

[[nodiscard]] vibecraft::game::OxygenTankTier decodeLegacyNetworkTankTier(
    const int rawTier,
    const vibecraft::game::OxygenTankTier fallbackTier)
{
    switch (rawTier)
    {
    case static_cast<int>(vibecraft::game::OxygenTankTier::None):
        return vibecraft::game::OxygenTankTier::None;
    case static_cast<int>(vibecraft::game::OxygenTankTier::Starter):
        return vibecraft::game::OxygenTankTier::Starter;
    case static_cast<int>(vibecraft::game::OxygenTankTier::Field):
        return vibecraft::game::OxygenTankTier::Field;
    case static_cast<int>(vibecraft::game::OxygenTankTier::Expedition):
        return vibecraft::game::OxygenTankTier::Expedition;
    default:
        return fallbackTier;
    }
}

[[nodiscard]] vibecraft::world::SurfaceBiome surfaceBiomeAtPlayerFeet(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition)
{
    return terrainGenerator.surfaceBiomeAt(
        static_cast<int>(std::floor(playerFeetPosition.x)),
        static_cast<int>(std::floor(playerFeetPosition.z)));
}

[[nodiscard]] float oxygenDrainMultiplierForBiome(const vibecraft::world::SurfaceBiome biome)
{
    if (vibecraft::world::biomes::isJungleSurfaceBiome(biome))
    {
        return 0.0f;
    }
    if (vibecraft::world::biomes::isSandySurfaceBiome(biome))
    {
        return 1.45f;
    }
    if (vibecraft::world::biomes::isSnowySurfaceBiome(biome))
    {
        return 1.15f;
    }
    if (biome == vibecraft::world::SurfaceBiome::DarkForest)
    {
        return 0.92f;
    }
    return 1.0f;
}

[[nodiscard]] const char* oxygenBiomeLabel(const vibecraft::world::SurfaceBiome biome)
{
    return vibecraft::world::surfaceBiomeLabel(biome);
}

[[nodiscard]] bool oxygenGrovesProvideRefill(const vibecraft::world::SurfaceBiome biome)
{
    return vibecraft::world::biomes::isJungleSurfaceBiome(biome);
}

[[nodiscard]] const char* oxygenBiomeGuidanceForBiome(const vibecraft::world::SurfaceBiome biome)
{
    using B = vibecraft::world::SurfaceBiome;
    switch (biome)
    {
    case B::Plains:
        return "Plains: open grassland with gentle terrain, exposed ore near the surface, and easy routes between forest belts.";
    case B::Forest:
        return "Forest: the default woodland around spawn, with mixed oak and birch cover and frequent safe tree lines.";
    case B::BirchForest:
        return "Birch forest: brighter woodland with cleaner spacing and tall pale trunks for easy navigation.";
    case B::DarkForest:
        return "Dark forest: denser canopy, more mushrooms, and heavier shade than the starter woods.";
    case B::Taiga:
        return "Taiga: spruce-heavy woodland with rougher undergrowth, ferns, and darker forest floor patches.";
    case B::SnowyPlains:
        return "Snowy plains: colder shelves, bright snow cover, and exposed stone along steeper ridges.";
    case B::SnowyTaiga:
        return "Snowy taiga: cold spruce woods with snowy ground and sparser cover than warm forests.";
    case B::Desert:
        return "Desert: warm sand, exposed sandstone, and harsher open ridgelines between safer basins.";
    case B::Jungle:
    case B::SparseJungle:
    case B::BambooJungle:
        return "Jungle: dense canopy, heavy ground cover, and the thickest vegetation in the world.";
    }

    return {};
}

[[nodiscard]] std::vector<glm::vec3> collectNearbyGeneratorCenters(
    const vibecraft::world::World& world,
    const glm::vec3& playerFeetPosition)
{
    const glm::vec3 playerCenter = playerFeetPosition + glm::vec3(0.0f, 0.9f, 0.0f);
    const int centerX = static_cast<int>(std::floor(playerCenter.x));
    const int centerY = static_cast<int>(std::floor(playerCenter.y));
    const int centerZ = static_cast<int>(std::floor(playerCenter.z));
    const float radiusSq = static_cast<float>(kOxygenGeneratorRadiusBlocks * kOxygenGeneratorRadiusBlocks);
    std::vector<glm::vec3> generatorCenters;

    for (int y = centerY - kOxygenGeneratorRadiusBlocks; y <= centerY + kOxygenGeneratorRadiusBlocks; ++y)
    {
        for (int z = centerZ - kOxygenGeneratorRadiusBlocks; z <= centerZ + kOxygenGeneratorRadiusBlocks; ++z)
        {
            for (int x = centerX - kOxygenGeneratorRadiusBlocks; x <= centerX + kOxygenGeneratorRadiusBlocks; ++x)
            {
                if (world.blockAt(x, y, z) != vibecraft::world::BlockType::OxygenGenerator)
                {
                    continue;
                }

                const glm::vec3 generatorCenter(
                    static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f,
                    static_cast<float>(z) + 0.5f);
                const glm::vec3 delta = generatorCenter - playerCenter;
                if (glm::dot(delta, delta) <= radiusSq)
                {
                    generatorCenters.push_back(generatorCenter);
                }
            }
        }
    }

    return generatorCenters;
}

[[nodiscard]] std::vector<glm::vec3> collectGeneratorCentersInRange(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    const int searchRadiusBlocks,
    const std::size_t maxResults)
{
    constexpr int kVerticalSearchRadiusBlocks = 24;
    const glm::vec3 referenceCenter = referencePosition + glm::vec3(0.0f, 0.9f, 0.0f);
    const int centerX = static_cast<int>(std::floor(referenceCenter.x));
    const int centerY = static_cast<int>(std::floor(referenceCenter.y));
    const int centerZ = static_cast<int>(std::floor(referenceCenter.z));
    const int verticalRadiusBlocks = std::min(searchRadiusBlocks, kVerticalSearchRadiusBlocks);
    const float radiusSq = static_cast<float>(searchRadiusBlocks * searchRadiusBlocks);
    std::vector<glm::vec3> generatorCenters;

    for (int y = centerY - verticalRadiusBlocks; y <= centerY + verticalRadiusBlocks; ++y)
    {
        for (int z = centerZ - searchRadiusBlocks; z <= centerZ + searchRadiusBlocks; ++z)
        {
            for (int x = centerX - searchRadiusBlocks; x <= centerX + searchRadiusBlocks; ++x)
            {
                if (world.blockAt(x, y, z) != vibecraft::world::BlockType::OxygenGenerator)
                {
                    continue;
                }

                const glm::vec3 generatorCenter(
                    static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f,
                    static_cast<float>(z) + 0.5f);
                const glm::vec3 delta = generatorCenter - referenceCenter;
                if (glm::dot(delta, delta) <= radiusSq)
                {
                    generatorCenters.push_back(generatorCenter);
                    if (generatorCenters.size() >= maxResults)
                    {
                        return generatorCenters;
                    }
                }
            }
        }
    }

    return generatorCenters;
}

[[nodiscard]] bool hasNearbyGeneratorInSafeZoneRange(
    const vibecraft::world::World& world,
    const glm::vec3& playerFeetPosition)
{
    constexpr int kSafeZoneSearchRadiusBlocks = kOxygenGeneratorRadiusBlocks * 3;
    const glm::vec3 playerCenter = playerFeetPosition + glm::vec3(0.0f, 0.9f, 0.0f);
    const std::vector<OxygenSafeZone> safeZones = collectOxygenSafeZones(
        world,
        playerFeetPosition,
        kSafeZoneSearchRadiusBlocks,
        48);
    for (const OxygenSafeZone& zone : safeZones)
    {
        const glm::vec3 delta = zone.center - playerCenter;
        if (glm::dot(delta, delta) <= zone.radius * zone.radius)
        {
            return true;
        }
    }
    return false;
}
}  // namespace

const char* timeOfDayLabel(const vibecraft::game::TimeOfDayPeriod period)
{
    switch (period)
    {
    case vibecraft::game::TimeOfDayPeriod::Dawn:
        return "dawn";
    case vibecraft::game::TimeOfDayPeriod::Day:
        return "day";
    case vibecraft::game::TimeOfDayPeriod::Dusk:
        return "dusk";
    case vibecraft::game::TimeOfDayPeriod::Night:
    default:
        return "night";
    }
}

const char* weatherLabel(const vibecraft::game::WeatherType weatherType)
{
    switch (weatherType)
    {
    case vibecraft::game::WeatherType::Cloudy:
        return "cloudy";
    case vibecraft::game::WeatherType::Rain:
        return "rain";
    case vibecraft::game::WeatherType::Clear:
    default:
        return "clear";
    }
}

const char* hazardLabel(const vibecraft::game::EnvironmentalHazards& hazards)
{
    if (hazards.bodyInLava)
    {
        return "lava";
    }
    if (hazards.headSubmergedInWater)
    {
        return "underwater";
    }
    if (hazards.bodyInWater)
    {
        return "water";
    }
    return "clear";
}

vibecraft::game::OxygenEnvironment sampleOxygenEnvironment(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::EnvironmentalHazards& hazards,
    const bool creativeModeEnabled)
{
    if (creativeModeEnabled)
    {
        return {.insideSafeZone = true, .drainMultiplier = 0.0f, .safeZoneRefillPerSecond = 999.0f};
    }

    const vibecraft::world::SurfaceBiome biome = surfaceBiomeAtPlayerFeet(terrainGenerator, playerFeetPosition);
    const bool generatorSafeZone = hasNearbyGeneratorInSafeZoneRange(world, playerFeetPosition);
    vibecraft::game::OxygenEnvironment environment{
        .insideSafeZone = generatorSafeZone,
        .drainMultiplier = oxygenDrainMultiplierForBiome(biome),
        .safeZoneRefillPerSecond = generatorSafeZone ? 16.0f : 0.0f,
    };

    if (!generatorSafeZone && oxygenGrovesProvideRefill(biome))
    {
        environment.insideSafeZone = true;
        environment.drainMultiplier = 0.0f;
        environment.safeZoneRefillPerSecond = 4.5f;
    }

    if (hazards.headSubmergedInWater)
    {
        environment.insideSafeZone = false;
        environment.drainMultiplier = std::max(environment.drainMultiplier, 2.35f);
        environment.safeZoneRefillPerSecond = 0.0f;
    }
    else if (hazards.bodyInWater)
    {
        environment.insideSafeZone = false;
        environment.drainMultiplier = std::max(environment.drainMultiplier, 1.35f);
        environment.safeZoneRefillPerSecond = 0.0f;
    }

    if (hazards.bodyInLava)
    {
        environment.insideSafeZone = false;
        environment.drainMultiplier = std::max(environment.drainMultiplier, 2.1f);
        environment.safeZoneRefillPerSecond = 0.0f;
    }

    return environment;
}

std::string buildOxygenStatusLine(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition,
    const vibecraft::game::OxygenState& oxygenState,
    const vibecraft::game::OxygenEnvironment& oxygenEnvironment)
{
    const vibecraft::world::SurfaceBiome biome = surfaceBiomeAtPlayerFeet(terrainGenerator, playerFeetPosition);
    return fmt::format(
        "O2: {:.0f}/{:.0f}  Tank: {}  Zone: {}  Biome: {}  Drain: {}",
        oxygenState.oxygen,
        oxygenState.capacity,
        vibecraft::game::oxygenTankTierName(oxygenState.tankTier),
        oxygenZoneLabel(oxygenEnvironment),
        oxygenBiomeLabel(biome),
        oxygenEnvironment.insideSafeZone ? "paused" : fmt::format("x{:.2f}", oxygenEnvironment.drainMultiplier));
}

const char* oxygenZoneLabel(const vibecraft::game::OxygenEnvironment& oxygenEnvironment)
{
    if (!oxygenEnvironment.insideSafeZone)
    {
        return "hostile";
    }

    // Relay-safe zones use a strong fixed refill (16/s) while groves use a lower biome refill.
    if (oxygenEnvironment.safeZoneRefillPerSecond >= 15.0f)
    {
        return "relay";
    }

    if (oxygenEnvironment.safeZoneRefillPerSecond > 0.0f)
    {
        return "grove";
    }

    return "safe";
}

const char* oxygenBiomeGuidance(
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& playerFeetPosition)
{
    return oxygenBiomeGuidanceForBiome(surfaceBiomeAtPlayerFeet(terrainGenerator, playerFeetPosition));
}

std::string survivalTipLine(const float sessionPlaySeconds)
{
    constexpr float kSurvivalTipDurationSeconds = 240.0f;
    constexpr float kSurvivalTipPhaseSeconds = 40.0f;
    if (!std::isfinite(sessionPlaySeconds) || sessionPlaySeconds < 0.0f
        || sessionPlaySeconds >= kSurvivalTipDurationSeconds)
    {
        return {};
    }

    const int phase = static_cast<int>(sessionPlaySeconds / kSurvivalTipPhaseSeconds);
    switch (phase)
    {
    case 0:
        return "Tip: E — inventory, oxygen tank slot, armor, 2×2 craft.";
    case 1:
        return "Tip: right-click places Oxygen Generator; the sphere shows the breathable radius.";
    case 2:
        return "Tip: watch O2; jungles and oxygen generators refill you in safe conditions.";
    case 3:
        return "Tip: water and lava override oxygen safety — move or build upward.";
    case 4:
        return "Tip: cured hide (3×3 workbench) → Scout armor — less damage from hostiles.";
    case 5:
    default:
        return "Tip: use biome hints - iron, glass, glowstone, and moss help with your next goal.";
    }
}

std::vector<glm::vec3> collectNearbyOxygenGenerators(
    const vibecraft::world::World& world,
    const glm::vec3& playerFeetPosition)
{
    return collectNearbyGeneratorCenters(world, playerFeetPosition);
}

std::vector<glm::vec3> collectVisibleOxygenGenerators(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    const int searchRadiusBlocks,
    const std::size_t maxResults)
{
    return collectGeneratorCentersInRange(
        world,
        referencePosition,
        std::max(searchRadiusBlocks, kOxygenGeneratorRadiusBlocks),
        std::max<std::size_t>(1, maxResults));
}

std::vector<OxygenSafeZone> buildOxygenSafeZones(const std::vector<glm::vec3>& generatorCenters)
{
    std::vector<OxygenSafeZone> safeZones;
    if (generatorCenters.empty())
    {
        return safeZones;
    }
    const float baseRadius = static_cast<float>(kOxygenGeneratorRadiusBlocks);
    const float mergeDistance = baseRadius * 2.0f;
    const float mergeDistanceSq = mergeDistance * mergeDistance;
    std::vector<int> clusterIds(generatorCenters.size(), -1);
    int clusterCounter = 0;

    for (std::size_t i = 0; i < generatorCenters.size(); ++i)
    {
        if (clusterIds[i] != -1)
        {
            continue;
        }

        std::vector<std::size_t> stack;
        stack.push_back(i);
        clusterIds[i] = clusterCounter;
        std::vector<std::size_t> members;

        while (!stack.empty())
        {
            const std::size_t idx = stack.back();
            stack.pop_back();
            members.push_back(idx);
            for (std::size_t j = 0; j < generatorCenters.size(); ++j)
            {
                if (clusterIds[j] != -1)
                {
                    continue;
                }
                const glm::vec3 delta = generatorCenters[j] - generatorCenters[idx];
                if (glm::dot(delta, delta) <= mergeDistanceSq)
                {
                    clusterIds[j] = clusterCounter;
                    stack.push_back(j);
                }
            }
        }

        glm::vec3 centroid{0.0f};
        for (const std::size_t memberIdx : members)
        {
            centroid += generatorCenters[memberIdx];
        }
        centroid /= static_cast<float>(members.size());

        float maxDistance = 0.0f;
        for (const std::size_t memberIdx : members)
        {
            const float distance = glm::length(generatorCenters[memberIdx] - centroid);
            maxDistance = std::max(maxDistance, distance);
        }

        const float stackingBonus =
            kGeneratorStackingBonusPerRelay * static_cast<float>(members.size() > 1 ? members.size() - 1 : 0);
        const float combinedRadius = baseRadius + std::max(maxDistance, stackingBonus);
        safeZones.push_back(OxygenSafeZone{
            .center = centroid,
            .radius = combinedRadius,
            .generatorCount = members.size(),
        });
        ++clusterCounter;
    }

    return safeZones;
}

std::vector<OxygenSafeZone> collectOxygenSafeZones(
    const vibecraft::world::World& world,
    const glm::vec3& referencePosition,
    const int searchRadiusBlocks,
    const std::size_t maxZones)
{
    const std::size_t generatorBudget = std::max<std::size_t>(64, maxZones * 4);
    const std::vector<glm::vec3> centers = collectGeneratorCentersInRange(
        world,
        referencePosition,
        std::max(searchRadiusBlocks, kOxygenGeneratorRadiusBlocks),
        generatorBudget);
    std::vector<OxygenSafeZone> zones = buildOxygenSafeZones(centers);
    const std::size_t airlockBudget = maxZones > 0
        ? (zones.size() < maxZones ? maxZones - zones.size() : 0)
        : generatorBudget;
    for (const glm::vec3& airlockCenter : collectPoweredAirlockCenters(world, centers, referencePosition, airlockBudget))
    {
        zones.push_back(OxygenSafeZone{
            .center = airlockCenter,
            .radius = static_cast<float>(kAirlockOutletRadiusBlocks),
            .generatorCount = 1,
        });
    }
    if (maxZones > 0 && zones.size() > maxZones)
    {
        zones.resize(maxZones);
    }
    return zones;
}

float encodeLegacyNetworkAir(const vibecraft::game::OxygenState& oxygenState)
{
    if (oxygenState.capacity <= 0.0f)
    {
        return 0.0f;
    }

    const float normalizedFill = std::clamp(oxygenState.oxygen / oxygenState.capacity, 0.0f, 1.0f);
    const float legacyFill = normalizedFill * kLegacyNetworkAirCapacity;
    const float encodedTier =
        static_cast<float>(static_cast<std::uint8_t>(oxygenState.tankTier)) * kLegacyNetworkAirTierStride;
    return encodedTier + legacyFill;
}

void applyLegacyNetworkAirToOxygenSystem(vibecraft::game::OxygenSystem& oxygenSystem, const float encodedAir)
{
    vibecraft::game::OxygenState state = oxygenSystem.state();
    if (!std::isfinite(encodedAir))
    {
        return;
    }

    float legacyFill = encodedAir;
    if (encodedAir >= kLegacyNetworkAirTierStride)
    {
        const int rawTier = static_cast<int>(std::floor(encodedAir / kLegacyNetworkAirTierStride));
        state.tankTier = decodeLegacyNetworkTankTier(rawTier, state.tankTier);
        legacyFill = encodedAir - static_cast<float>(rawTier) * kLegacyNetworkAirTierStride;
    }

    state.capacity = vibecraft::game::oxygenTankDefinition(state.tankTier).capacity;
    if (state.capacity <= 0.0f)
    {
        oxygenSystem.setState(state);
        return;
    }

    const float normalizedFill = std::clamp(legacyFill / kLegacyNetworkAirCapacity, 0.0f, 1.0f);
    state.oxygen = state.capacity * normalizedFill;
    oxygenSystem.setState(state);
}
}  // namespace vibecraft::app
