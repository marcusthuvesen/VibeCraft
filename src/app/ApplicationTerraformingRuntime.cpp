#include "vibecraft/app/ApplicationTerraformingRuntime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::app
{
namespace
{
constexpr float kTerraformPulseIntervalSeconds = 1.5f;
constexpr int kTerraformZoneSearchRadiusBlocks = 48;
constexpr std::size_t kMaxTerraformZones = 16;
constexpr std::size_t kSampleAttemptsPerZone = 8;

using SampleOffset = std::array<int, 2>;

constexpr std::array<SampleOffset, 24> kTerraformSampleOffsets{{
    {{1, 0}},
    {{0, 1}},
    {{-1, 0}},
    {{0, -1}},
    {{2, 0}},
    {{1, 1}},
    {{0, 2}},
    {{-1, 1}},
    {{-2, 0}},
    {{-1, -1}},
    {{0, -2}},
    {{1, -1}},
    {{2, 1}},
    {{1, 2}},
    {{-1, 2}},
    {{-2, 1}},
    {{-2, -1}},
    {{-1, -2}},
    {{1, -2}},
    {{2, -1}},
    {{3, 0}},
    {{0, 3}},
    {{-3, 0}},
    {{0, -3}},
}};

[[nodiscard]] vibecraft::world::BlockType nextTerraformBlockType(
    const vibecraft::world::BlockType blockType,
    const std::size_t generatorCount)
{
    using B = vibecraft::world::BlockType;
    switch (blockType)
    {
    case B::Sand:
        return B::Dirt;
    case B::Dirt:
        return B::Grass;
    case B::Grass:
        return generatorCount >= 2 ? B::JungleGrass : B::Grass;
    default:
        return blockType;
    }
}

[[nodiscard]] bool isTerraformDecoration(const vibecraft::world::BlockType blockType)
{
    return blockType == vibecraft::world::BlockType::DeadBush
        || blockType == vibecraft::world::BlockType::Dandelion
        || blockType == vibecraft::world::BlockType::Poppy
        || blockType == vibecraft::world::BlockType::BlueOrchid
        || blockType == vibecraft::world::BlockType::Allium
        || blockType == vibecraft::world::BlockType::OxeyeDaisy
        || blockType == vibecraft::world::BlockType::BrownMushroom
        || blockType == vibecraft::world::BlockType::RedMushroom
        || blockType == vibecraft::world::BlockType::GrassTuft
        || blockType == vibecraft::world::BlockType::FlowerTuft
        || blockType == vibecraft::world::BlockType::DryTuft
        || blockType == vibecraft::world::BlockType::LushTuft
        || blockType == vibecraft::world::BlockType::FrostTuft
        || blockType == vibecraft::world::BlockType::SparseTuft
        || blockType == vibecraft::world::BlockType::CloverTuft
        || blockType == vibecraft::world::BlockType::SproutTuft;
}

[[nodiscard]] bool sampleFallsInsideSafeZone(
    const OxygenSafeZone& safeZone,
    const int worldX,
    const int surfaceY,
    const int worldZ)
{
    const glm::vec3 samplePoint(
        static_cast<float>(worldX) + 0.5f,
        static_cast<float>(surfaceY) + 0.5f,
        static_cast<float>(worldZ) + 0.5f);
    const glm::vec3 delta = samplePoint - safeZone.center;
    return glm::dot(delta, delta) <= safeZone.radius * safeZone.radius;
}

[[nodiscard]] std::uint32_t terraformHash(const int worldX, const int worldZ, const std::size_t salt)
{
    std::uint32_t value = static_cast<std::uint32_t>(worldX) * 0x27d4eb2dU;
    value ^= static_cast<std::uint32_t>(worldZ) * 0x165667b1U + static_cast<std::uint32_t>(salt * 0x9e3779b9U);
    value ^= value >> 15U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] bool tryTerraformOneSample(
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const OxygenSafeZone& safeZone,
    const std::size_t sampleIndex)
{
    const SampleOffset& offset = kTerraformSampleOffsets[sampleIndex % kTerraformSampleOffsets.size()];
    const int worldX = static_cast<int>(std::floor(safeZone.center.x)) + offset[0];
    const int worldZ = static_cast<int>(std::floor(safeZone.center.z)) + offset[1];
    const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
    if (surfaceY < vibecraft::world::kWorldMinY || surfaceY >= vibecraft::world::kWorldMaxY)
    {
        return false;
    }
    if (!sampleFallsInsideSafeZone(safeZone, worldX, surfaceY, worldZ))
    {
        return false;
    }

    const vibecraft::world::BlockType surfaceBlock = world.blockAt(worldX, surfaceY, worldZ);
    const vibecraft::world::BlockType nextBlock = nextTerraformBlockType(surfaceBlock, safeZone.generatorCount);
    if (nextBlock == surfaceBlock)
    {
        return false;
    }

    const vibecraft::world::BlockType blockAbove = world.blockAt(worldX, surfaceY + 1, worldZ);
    if (blockAbove != vibecraft::world::BlockType::Air && !isTerraformDecoration(blockAbove))
    {
        return false;
    }

    if (isTerraformDecoration(blockAbove))
    {
        if (!world.applyEditCommand({
                .action = vibecraft::world::WorldEditAction::Remove,
                .position = {worldX, surfaceY + 1, worldZ},
                .blockType = vibecraft::world::BlockType::Air,
            }))
        {
            return false;
        }
    }

    if (!world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {worldX, surfaceY, worldZ},
        .blockType = nextBlock,
    }))
    {
        return false;
    }

    if ((nextBlock == vibecraft::world::BlockType::Grass || nextBlock == vibecraft::world::BlockType::JungleGrass)
        && world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::Air)
    {
        const std::uint32_t hash = terraformHash(worldX, worldZ, safeZone.generatorCount);
        const float roll = static_cast<float>(hash & 0xffffu) / 65535.0f;
        const float tuftChance = std::min(0.92f, 0.28f + static_cast<float>(safeZone.generatorCount) * 0.12f);
        if (roll < tuftChance)
        {
            const vibecraft::world::BlockType tuftBlock = safeZone.generatorCount >= 3
                ? vibecraft::world::BlockType::LushTuft
                : (safeZone.generatorCount >= 2
                        ? vibecraft::world::BlockType::FlowerTuft
                        : vibecraft::world::BlockType::GrassTuft);
            world.applyEditCommand({
                .action = vibecraft::world::WorldEditAction::Place,
                .position = {worldX, surfaceY + 1, worldZ},
                .blockType = tuftBlock,
            });
        }
    }

    return true;
}
}  // namespace

TerraformingTickResult tickLocalTerraforming(
    const float deltaTimeSeconds,
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& referencePosition,
    TerraformingRuntimeState& runtimeState)
{
    TerraformingTickResult result{};
    if (deltaTimeSeconds <= 0.0f)
    {
        return result;
    }

    runtimeState.pulseAccumulatorSeconds += deltaTimeSeconds;
    while (runtimeState.pulseAccumulatorSeconds >= kTerraformPulseIntervalSeconds)
    {
        runtimeState.pulseAccumulatorSeconds -= kTerraformPulseIntervalSeconds;
        const std::vector<OxygenSafeZone> safeZones = collectOxygenSafeZones(
            world,
            referencePosition,
            kTerraformZoneSearchRadiusBlocks,
            kMaxTerraformZones);
        for (std::size_t zoneIndex = 0; zoneIndex < safeZones.size(); ++zoneIndex)
        {
            const OxygenSafeZone& safeZone = safeZones[zoneIndex];
            for (std::size_t attempt = 0; attempt < kSampleAttemptsPerZone; ++attempt)
            {
                const std::size_t sampleIndex =
                    static_cast<std::size_t>(runtimeState.pulseCounter) + zoneIndex * 5 + attempt;
                if (tryTerraformOneSample(world, terrainGenerator, safeZone, sampleIndex))
                {
                    ++result.blocksChanged;
                    break;
                }
            }
        }
        ++runtimeState.pulseCounter;
    }

    return result;
}
}  // namespace vibecraft::app
