#include "vibecraft/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"
#include "vibecraft/world/underground/CaveRules.hpp"
#include "vibecraft/world/underground/OreVeinRules.hpp"

namespace vibecraft::world
{
namespace
{
constexpr int kSeaLevel = 63;
constexpr int kTopsoilDepth = 3;
constexpr int kDeepslateFullStartY = -8;
constexpr int kDeepslateTransitionEndY = 8;
constexpr int kSandSurfaceMaxHeight = 74;
constexpr int kBeachMaxHeightAboveSea = 1;
constexpr int kMountainStoneCapStartY = 104;
constexpr int kMountainStoneCapThickness = 2;
constexpr int kLowlandPondMaxHeightAboveSea = 3;
constexpr std::uint32_t kPondNoiseSeed = 0xa53f210bU;
constexpr int kSandstoneStratumDepth = 6;
constexpr std::uint32_t kMoisturePocketNoiseSeed = 0xd47a38c1U;
constexpr std::uint32_t kSandyMesaNoiseSeed = 0xe58b49d2U;
constexpr std::uint32_t kSandyWindCutNoiseSeed = 0xf69c5ae3U;
constexpr std::uint32_t kSnowShelfNoiseSeed = 0x07ad6bf4U;
constexpr std::uint32_t kSnowCrownNoiseSeed = 0x18be7c05U;
constexpr std::uint32_t kJungleShelfNoiseSeed = 0x29cf8d16U;
constexpr std::uint32_t kJungleValleyNoiseSeed = 0x3ae09e27U;
constexpr std::uint32_t kTemperateTerraceNoiseSeed = 0x4bf1af38U;

[[nodiscard]] constexpr std::uint32_t mixedSeed(const std::uint32_t baseSeed, const std::uint32_t worldSeed)
{
    std::uint32_t mixed = baseSeed ^ (worldSeed + 0x9e3779b9U + (baseSeed << 6U) + (baseSeed >> 2U));
    mixed ^= mixed >> 16U;
    mixed *= 0x7feb352dU;
    mixed ^= mixed >> 15U;
    mixed *= 0x846ca68bU;
    mixed ^= mixed >> 16U;
    return mixed;
}

enum class ColumnBiome : std::uint8_t
{
    TemperateGrassland,
    Sandy,
    Snowy,
    Jungle,
};

struct ColumnContext
{
    int surfaceHeight = kSeaLevel;
    int topsoilDepth = kTopsoilDepth;
    int columnWaterLevel = kSeaLevel;
    int stratumTopExclusive = 0;
    int stratumBottomInclusive = 0;
    bool usesSandStrata = false;
    ColumnBiome biome = ColumnBiome::TemperateGrassland;
    BlockType surfaceBlockType = BlockType::Grass;
    BlockType subsurfaceBlockType = BlockType::Dirt;
};

[[nodiscard]] bool shouldPlaceBedrock(const int y)
{
    return y <= kBedrockFloorMaxY;
}

[[nodiscard]] int topsoilDepthAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double depthNoise = noise::valueNoise2d(
        static_cast<double>(worldX),
        static_cast<double>(worldZ),
        48.0,
        mixedSeed(0x4f1bbcdcU, worldSeed));
    return kTopsoilDepth + static_cast<int>(depthNoise * 2.0);
}

[[nodiscard]] double transitionBandNoise(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double phase = static_cast<double>(mixedSeed(0x6a0f91e3U, worldSeed) & 0xffffU) / 65535.0 * 6.283185307179586;
    const double phaseB =
        static_cast<double>((mixedSeed(0x1f2e3d4cU, worldSeed) >> 8U) & 0xffffU) / 65535.0 * 6.283185307179586;
    return 0.5 + 0.25 * std::sin(static_cast<double>(worldX) * 0.07 + static_cast<double>(worldZ) * 0.05 + phase)
        + 0.25 * std::cos(static_cast<double>(worldX - worldZ) * 0.04 + phaseB);
}

[[nodiscard]] BlockType undergroundBlockTypeAt(const int worldX, const int y, const int worldZ, const std::uint32_t worldSeed)
{
    if (y <= kDeepslateFullStartY)
    {
        return BlockType::Deepslate;
    }

    if (y >= kDeepslateTransitionEndY)
    {
        return BlockType::Stone;
    }

    const double transitionBias = static_cast<double>(kDeepslateTransitionEndY - y) /
        static_cast<double>(kDeepslateTransitionEndY - kDeepslateFullStartY);
    return transitionBandNoise(worldX, worldZ, worldSeed) < transitionBias ? BlockType::Deepslate : BlockType::Stone;
}

[[nodiscard]] double biomeTemperatureAt(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    // Broad climate bands keep the alien world readable: lush fringes, glow forests, crystal expanses, and dry wastes.
    const double baseTemperature =
        noise::fbmNoise2d(worldXd, worldZd, 4000.0, 4, mixedSeed(0x8b4d1e29U, worldSeed)) * 2.0 - 1.0;
    const double variation =
        noise::fbmNoise2d(worldXd + 73.0, worldZd - 59.0, 1800.0, 3, mixedSeed(0x1c0f3aa7U, worldSeed)) * 2.0 - 1.0;
    const double altitudeCooling = std::clamp(
        static_cast<double>(surfaceHeight - kSeaLevel) / 120.0,
        0.0,
        0.55);
    return baseTemperature + variation * 0.33 - altitudeCooling;
}

[[nodiscard]] double biomeHumidityAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    return noise::fbmNoise2d(worldXd - 31.0, worldZd + 43.0, 3800.0, 4, mixedSeed(0x32a7f1c4U, worldSeed)) * 2.0
        - 1.0;
}

[[nodiscard]] double steppedHeightOffset(const double value, const double stepSize, const double strength)
{
    const double steppedValue = std::round(value / stepSize) * stepSize;
    return (steppedValue - value) * strength;
}

[[nodiscard]] double moisturePocketNoiseAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    return noise::fbmNoise2d(
        static_cast<double>(worldX) - 19.0,
        static_cast<double>(worldZ) + 27.0,
        120.0,
        3,
        mixedSeed(kMoisturePocketNoiseSeed, worldSeed));
}

[[nodiscard]] ColumnBiome toColumnBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Sandy:
        return ColumnBiome::Sandy;
    case SurfaceBiome::Snowy:
        return ColumnBiome::Snowy;
    case SurfaceBiome::Jungle:
        return ColumnBiome::Jungle;
    case SurfaceBiome::TemperateGrassland:
    default:
        return ColumnBiome::TemperateGrassland;
    }
}

[[nodiscard]] double basinDepthFromContinents(double continents);
[[nodiscard]] double ridgeHeightContribution(double ridges, double continents, double uplands);

[[nodiscard]] double baseTerrainHeightAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    const double continents =
        noise::fbmNoise2d(worldXd, worldZd, 280.0, 4, mixedSeed(0x1234abcdU, worldSeed)) * 2.0 - 1.0;
    const double uplands =
        noise::fbmNoise2d(worldXd + 91.0, worldZd - 137.0, 620.0, 3, mixedSeed(0x31f4bc9dU, worldSeed)) * 2.0 - 1.0;
    const double ridges = noise::ridgeNoise2d(worldXd, worldZd, 110.0, mixedSeed(0x4422aa11U, worldSeed));
    const double hills =
        noise::fbmNoise2d(worldXd, worldZd, 84.0, 4, mixedSeed(0x90f0c55aU, worldSeed)) * 2.0 - 1.0;
    const double detail =
        noise::fbmNoise2d(worldXd, worldZd, 34.0, 3, mixedSeed(0x7a0f3e19U, worldSeed)) * 2.0 - 1.0;
    return static_cast<double>(kSeaLevel)
        - 12.0
        + continents * 42.0
        + uplands * 15.0
        + ridgeHeightContribution(ridges, continents, uplands)
        + hills * 10.0
        + detail * 4.0
        - basinDepthFromContinents(continents);
}

[[nodiscard]] double biomeTerrainContribution(
    const ColumnBiome biome,
    const int worldX,
    const int worldZ,
    const double continents,
    const double uplands,
    const double ridges,
    const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    switch (biome)
    {
    case ColumnBiome::Sandy:
    {
        const double mesaNoise =
            noise::fbmNoise2d(worldXd - 181.0, worldZd + 129.0, 150.0, 3, mixedSeed(kSandyMesaNoiseSeed, worldSeed))
            * 2.0
            - 1.0;
        const double windCutNoise =
            noise::fbmNoise2d(worldXd + 73.0, worldZd - 61.0, 210.0, 3, mixedSeed(kSandyWindCutNoiseSeed, worldSeed))
            * 2.0
            - 1.0;
        const double finNoise =
            noise::ridgeNoise2d(worldXd + 51.0, worldZd - 87.0, 72.0, mixedSeed(kSandyWindCutNoiseSeed + 17U, worldSeed));
        const double plateauMask = std::clamp((continents + uplands * 0.28 + 0.18) / 1.18, 0.0, 1.0);
        const double mesas = plateauMask * std::max(0.0, mesaNoise * 0.78 + finNoise * 0.52 - 0.16) * 14.0;
        const double windCuts = std::clamp((0.08 - windCutNoise) / 0.40, 0.0, 1.0) * 6.0;
        return mesas + steppedHeightOffset(mesas, 9.0, 0.24) - windCuts;
    }
    case ColumnBiome::Snowy:
    {
        const double shelfNoise =
            noise::fbmNoise2d(worldXd + 143.0, worldZd - 39.0, 132.0, 3, mixedSeed(kSnowShelfNoiseSeed, worldSeed))
            * 2.0
            - 1.0;
        const double crownNoise =
            noise::ridgeNoise2d(worldXd - 67.0, worldZd - 121.0, 84.0, mixedSeed(kSnowCrownNoiseSeed, worldSeed));
        const double shelfMask = std::clamp((continents + ridges * 0.48 + 0.22) / 1.36, 0.0, 1.0);
        const double shelves = shelfMask * std::max(0.0, shelfNoise + crownNoise * 0.74 - 0.12) * 14.0;
        const double cirque = std::clamp((0.06 - shelfNoise) / 0.34, 0.0, 1.0) * 5.0;
        return shelves + steppedHeightOffset(shelves, 10.0, 0.24) - cirque;
    }
    case ColumnBiome::Jungle:
    {
        const double shelfNoise =
            noise::ridgeNoise2d(worldXd - 141.0, worldZd + 63.0, 88.0, mixedSeed(kJungleShelfNoiseSeed, worldSeed));
        const double valleyNoise =
            noise::fbmNoise2d(worldXd + 95.0, worldZd - 173.0, 170.0, 3, mixedSeed(kJungleValleyNoiseSeed, worldSeed))
            * 2.0
            - 1.0;
        const double shelfMask = std::clamp((continents + uplands * 0.45 + 0.12) / 1.18, 0.0, 1.0);
        const double shelves = shelfMask * std::max(0.0, shelfNoise - 0.18) * (10.0 + ridges * 5.0);
        const double valleys = std::clamp((0.10 - valleyNoise) / 0.40, 0.0, 1.0) * (4.0 + shelfMask * 4.0);
        return shelves + steppedHeightOffset(shelves, 8.0, 0.18) - valleys;
    }
    case ColumnBiome::TemperateGrassland:
    default:
    {
        const double terraceNoise = noise::fbmNoise2d(
                                         worldXd + 11.0,
                                         worldZd + 157.0,
                                         180.0,
                                         3,
                                         mixedSeed(kTemperateTerraceNoiseSeed, worldSeed))
            * 2.0
            - 1.0;
        const double meadowLift =
            std::clamp((terraceNoise + 0.14) / 1.12, 0.0, 1.0) * (3.0 + std::max(0.0, uplands) * 3.5);
        return meadowLift + steppedHeightOffset(meadowLift, 6.0, 0.14);
    }
    }
}

[[nodiscard]] ColumnBiome columnBiomeAt(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    if (biomeOverride.has_value())
    {
        return toColumnBiome(*biomeOverride);
    }
    if (surfaceHeight <= kSeaLevel + kBeachMaxHeightAboveSea)
    {
        return ColumnBiome::Sandy;
    }

    const double temperature = biomeTemperatureAt(worldX, worldZ, surfaceHeight, worldSeed);
    const double humidity = biomeHumidityAt(worldX, worldZ, worldSeed);
    if (surfaceHeight <= kSandSurfaceMaxHeight && temperature > 0.16 && humidity < 0.02)
    {
        return ColumnBiome::Sandy;
    }
    if (temperature < -0.18 || (surfaceHeight >= 96 && temperature < -0.04))
    {
        return ColumnBiome::Snowy;
    }
    if (temperature > 0.17 && humidity > 0.12)
    {
        return ColumnBiome::Jungle;
    }
    return ColumnBiome::TemperateGrassland;
}

[[nodiscard]] int surfaceHeightForCoordinates(
    const int worldX,
    const int worldZ,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    const double continents =
        noise::fbmNoise2d(worldXd, worldZd, 280.0, 4, mixedSeed(0x1234abcdU, worldSeed)) * 2.0 - 1.0;
    const double uplands =
        noise::fbmNoise2d(worldXd + 91.0, worldZd - 137.0, 620.0, 3, mixedSeed(0x31f4bc9dU, worldSeed)) * 2.0 - 1.0;
    const double ridges = noise::ridgeNoise2d(worldXd, worldZd, 110.0, mixedSeed(0x4422aa11U, worldSeed));
    const int provisionalSurfaceHeight = std::clamp(
        static_cast<int>(std::round(baseTerrainHeightAt(worldX, worldZ, worldSeed))),
        -20,
        190);
    (void)biomeOverride;
    const ColumnBiome targetBiome = columnBiomeAt(worldX, worldZ, provisionalSurfaceHeight, worldSeed, std::nullopt);
    const double terrainHeight = baseTerrainHeightAt(worldX, worldZ, worldSeed)
        + biomeTerrainContribution(targetBiome, worldX, worldZ, continents, uplands, ridges, worldSeed);
    return std::clamp(static_cast<int>(std::round(terrainHeight)), -20, 190);
}

[[nodiscard]] int maxNeighborSurfaceDeltaAt(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    int maxDelta = 0;
    constexpr int kOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& offset : kOffsets)
    {
        const int neighborSurfaceHeight =
            surfaceHeightForCoordinates(worldX + offset[0], worldZ + offset[1], worldSeed, biomeOverride);
        maxDelta = std::max(maxDelta, std::abs(neighborSurfaceHeight - surfaceHeight));
    }
    return maxDelta;
}

/// Dust flats use sand on the surface; sandstone acts as a compacted dust-rock stratum below.
[[nodiscard]] bool columnUsesSandStrata(const ColumnBiome biome)
{
    return biome == ColumnBiome::Sandy;
}

[[nodiscard]] BlockType surfaceBlockTypeAt(
    const ColumnBiome biome,
    const int surfaceHeight,
    const int maxNeighborSurfaceDelta,
    const double moisturePocket)
{
    if (surfaceHeight >= kMountainStoneCapStartY)
    {
        return biome == ColumnBiome::Snowy ? BlockType::SnowGrass : BlockType::Stone;
    }
    if (biome == ColumnBiome::Sandy)
    {
        if (maxNeighborSurfaceDelta >= 10 || (surfaceHeight >= 96 && maxNeighborSurfaceDelta >= 6))
        {
            return BlockType::Sandstone;
        }
        if (moisturePocket > 0.72 && surfaceHeight <= kSeaLevel + 6)
        {
            return BlockType::Gravel;
        }
        return BlockType::Sand;
    }
    if (biome == ColumnBiome::Snowy)
    {
        if (maxNeighborSurfaceDelta >= 11)
        {
            return BlockType::Stone;
        }
        if (maxNeighborSurfaceDelta >= 6 || surfaceHeight >= 118)
        {
            return BlockType::Gravel;
        }
        return BlockType::SnowGrass;
    }
    if (biome == ColumnBiome::Jungle)
    {
        if (maxNeighborSurfaceDelta >= 12)
        {
            return BlockType::Stone;
        }
        if ((moisturePocket > 0.58 && maxNeighborSurfaceDelta <= 4)
            || (surfaceHeight <= kSeaLevel + 6 && moisturePocket > 0.48))
        {
            return BlockType::MossBlock;
        }
        return BlockType::JungleGrass;
    }
    if (maxNeighborSurfaceDelta >= 11 || (surfaceHeight >= 100 && maxNeighborSurfaceDelta >= 6))
    {
        return BlockType::Stone;
    }
    if (moisturePocket > 0.64 && maxNeighborSurfaceDelta <= 3)
    {
        return BlockType::MossBlock;
    }
    return BlockType::Grass;
}

[[nodiscard]] BlockType subsurfaceBlockTypeAt(
    const ColumnBiome biome,
    const int maxNeighborSurfaceDelta,
    const BlockType surfaceBlockType)
{
    if (biome == ColumnBiome::Sandy)
    {
        return surfaceBlockType == BlockType::Sandstone || maxNeighborSurfaceDelta >= 8 ? BlockType::Sandstone : BlockType::Sand;
    }
    if (surfaceBlockType == BlockType::Stone || (surfaceBlockType == BlockType::Gravel && maxNeighborSurfaceDelta >= 6))
    {
        return BlockType::Stone;
    }
    return BlockType::Dirt;
}

[[nodiscard]] double basinDepthFromContinents(const double continents)
{
    const double basinFactor = std::clamp((-continents - 0.04) / 0.96, 0.0, 1.0);
    return basinFactor * basinFactor * 30.0;
}

[[nodiscard]] double ridgeHeightContribution(
    const double ridges,
    const double continents,
    const double uplands)
{
    const double ridgeMask = std::clamp((continents + uplands * 0.35 + 0.18) / 1.15, 0.0, 1.0);
    const double shapedRidges = std::pow(std::clamp(ridges, 0.0, 1.0), 1.35);
    return shapedRidges * (10.0 + ridgeMask * 30.0);
}

[[nodiscard]] bool isMountainStoneCapLayer(const int y, const int surfaceHeight)
{
    return surfaceHeight >= kMountainStoneCapStartY && y >= surfaceHeight - kMountainStoneCapThickness;
}

[[nodiscard]] bool shouldFloodLowlandColumn(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed)
{
    if (surfaceHeight < kSeaLevel || surfaceHeight > kSeaLevel + kLowlandPondMaxHeightAboveSea)
    {
        return false;
    }

    const double floodNoise = noise::fbmNoise2d(
        static_cast<double>(worldX),
        static_cast<double>(worldZ),
        88.0,
        3,
        mixedSeed(kPondNoiseSeed, worldSeed));
    return floodNoise > 0.66;
}

[[nodiscard]] ColumnContext buildColumnContext(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    const ColumnBiome biome = columnBiomeAt(worldX, worldZ, surfaceHeight, worldSeed, biomeOverride);
    const int maxNeighborSurfaceDelta =
        maxNeighborSurfaceDeltaAt(worldX, worldZ, surfaceHeight, worldSeed, biomeOverride);
    const double moisturePocket = moisturePocketNoiseAt(worldX, worldZ, worldSeed);
    const bool floodLowland = shouldFloodLowlandColumn(worldX, worldZ, surfaceHeight, worldSeed);
    const bool lushPondPocket = !biomeOverride.has_value()
        && (biome == ColumnBiome::Jungle || biome == ColumnBiome::TemperateGrassland)
        && surfaceHeight >= kSeaLevel - 1
        && surfaceHeight <= kSeaLevel + 5
        && maxNeighborSurfaceDelta <= 2
        && moisturePocket > 0.74;
    const int columnWaterLevel = floodLowland || lushPondPocket ? surfaceHeight + 1 : kSeaLevel;
    const bool usesSandStrata = columnUsesSandStrata(biome);
    const bool forceCanonicalSurface = biomeOverride.has_value();
    const BlockType surfaceBlockType = forceCanonicalSurface
        ? surfaceBlockTypeAt(biome, surfaceHeight, 0, 0.0)
        : surfaceBlockTypeAt(biome, surfaceHeight, maxNeighborSurfaceDelta, moisturePocket);
    const BlockType subsurfaceBlockType = forceCanonicalSurface
        ? subsurfaceBlockTypeAt(biome, 0, surfaceBlockType)
        : subsurfaceBlockTypeAt(biome, maxNeighborSurfaceDelta, surfaceBlockType);
    int topsoilDepth = topsoilDepthAt(worldX, worldZ, worldSeed);
    if (!forceCanonicalSurface && surfaceBlockType == BlockType::MossBlock && maxNeighborSurfaceDelta <= 2)
    {
        topsoilDepth += 1;
    }
    if (!forceCanonicalSurface
        && (surfaceBlockType == BlockType::Stone || surfaceBlockType == BlockType::Sandstone)
        && topsoilDepth > 1)
    {
        topsoilDepth -= 1;
    }
    topsoilDepth = std::clamp(topsoilDepth, 1, 6);
    const int stratumTopExclusive = surfaceHeight - topsoilDepth;
    return ColumnContext{
        .surfaceHeight = surfaceHeight,
        .topsoilDepth = topsoilDepth,
        .columnWaterLevel = columnWaterLevel,
        .stratumTopExclusive = stratumTopExclusive,
        .stratumBottomInclusive = stratumTopExclusive - kSandstoneStratumDepth,
        .usesSandStrata = usesSandStrata,
        .biome = biome,
        .surfaceBlockType = surfaceBlockType,
        .subsurfaceBlockType = subsurfaceBlockType,
    };
}

[[nodiscard]] SurfaceBiome toSurfaceBiome(const ColumnBiome biome)
{
    switch (biome)
    {
    case ColumnBiome::Sandy:
        return SurfaceBiome::Sandy;
    case ColumnBiome::Snowy:
        return SurfaceBiome::Snowy;
    case ColumnBiome::Jungle:
        return SurfaceBiome::Jungle;
    case ColumnBiome::TemperateGrassland:
    default:
        return SurfaceBiome::TemperateGrassland;
    }
}

[[nodiscard]] underground::BiomeOreProfile biomeOreProfileFromColumnBiome(const ColumnBiome biome)
{
    switch (biome)
    {
    case ColumnBiome::Sandy:
        return underground::BiomeOreProfile::DustFlats;
    case ColumnBiome::Snowy:
        return underground::BiomeOreProfile::IceShelf;
    case ColumnBiome::Jungle:
        return underground::BiomeOreProfile::OxygenGrove;
    case ColumnBiome::TemperateGrassland:
    default:
        return underground::BiomeOreProfile::RegolithPlains;
    }
}

[[nodiscard]] BlockType blockTypeAtWithContext(
    const int worldX,
    const int y,
    const int worldZ,
    const ColumnContext& columnContext,
    const std::uint32_t worldSeed)
{
    if (shouldPlaceBedrock(y))
    {
        return BlockType::Bedrock;
    }
    if (y > columnContext.surfaceHeight)
    {
        return y <= columnContext.columnWaterLevel ? BlockType::Water : BlockType::Air;
    }
    if (y == columnContext.surfaceHeight)
    {
        return columnContext.surfaceBlockType;
    }
    if (isMountainStoneCapLayer(y, columnContext.surfaceHeight))
    {
        return BlockType::Stone;
    }
    if (y >= columnContext.surfaceHeight - columnContext.topsoilDepth)
    {
        return columnContext.subsurfaceBlockType;
    }
    if (underground::shouldCarveCave(worldX, y, worldZ, columnContext.surfaceHeight))
    {
        return underground::caveInteriorBlockType(worldX, y, worldZ, columnContext.surfaceHeight);
    }
    if (columnContext.usesSandStrata
        && y < columnContext.stratumTopExclusive
        && y >= columnContext.stratumBottomInclusive)
    {
        return BlockType::Sandstone;
    }

    const BlockType hostBlockType = undergroundBlockTypeAt(worldX, y, worldZ, worldSeed);
    if (const std::optional<BlockType> ore = underground::selectOreVeinBlock(
            worldX,
            y,
            worldZ,
            columnContext.surfaceHeight,
            hostBlockType,
            biomeOreProfileFromColumnBiome(columnContext.biome)))
    {
        return *ore;
    }
    return hostBlockType;
}
}  // namespace

std::uint32_t TerrainGenerator::worldSeed() const
{
    return worldSeed_;
}

const char* surfaceBiomeLabel(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::TemperateGrassland:
        return "glow forest fringe";
    case SurfaceBiome::Sandy:
        return "dry wastes";
    case SurfaceBiome::Snowy:
        return "crystal expanse";
    case SurfaceBiome::Jungle:
        return "glow forest heart";
    }

    return "unknown";
}

void TerrainGenerator::setWorldSeed(const std::uint32_t worldSeed)
{
    worldSeed_ = worldSeed;
}

std::optional<SurfaceBiome> TerrainGenerator::biomeOverride() const
{
    return biomeOverride_;
}

void TerrainGenerator::setBiomeOverride(const std::optional<SurfaceBiome> biomeOverride)
{
    biomeOverride_ = biomeOverride;
}

int TerrainGenerator::surfaceHeightAt(const int worldX, const int worldZ) const
{
    return surfaceHeightForCoordinates(worldX, worldZ, worldSeed_, biomeOverride_);
}

SurfaceBiome TerrainGenerator::surfaceBiomeAt(const int worldX, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    return toSurfaceBiome(columnBiomeAt(worldX, worldZ, surfaceHeight, worldSeed_, biomeOverride_));
}

BlockType TerrainGenerator::blockTypeAt(const int worldX, const int y, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    return blockTypeAtWithContext(
        worldX,
        y,
        worldZ,
        buildColumnContext(worldX, worldZ, surfaceHeight, worldSeed_, biomeOverride_),
        worldSeed_);
}

void TerrainGenerator::fillColumn(const int worldX, const int worldZ, BlockType* const outColumnBlocks) const
{
    if (outColumnBlocks == nullptr)
    {
        return;
    }

    std::fill(outColumnBlocks, outColumnBlocks + kWorldHeight, BlockType::Air);

    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    const ColumnContext columnContext = buildColumnContext(worldX, worldZ, surfaceHeight, worldSeed_, biomeOverride_);
    const int surfaceIndex = surfaceHeight - kWorldMinY;

    for (int y = kWorldMinY; y <= kBedrockFloorMaxY; ++y)
    {
        outColumnBlocks[y - kWorldMinY] = BlockType::Bedrock;
    }

    if (surfaceHeight < kWorldMinY)
    {
        const int waterTop = std::min(columnContext.columnWaterLevel, kWorldMaxY);
        for (int y = kWorldMinY; y <= waterTop; ++y)
        {
            if (y > kBedrockFloorMaxY)
            {
                outColumnBlocks[y - kWorldMinY] = BlockType::Water;
            }
        }
        return;
    }

    if (columnContext.columnWaterLevel > surfaceHeight)
    {
        const int waterStart = std::max(surfaceHeight + 1, kWorldMinY);
        const int waterEnd = std::min(columnContext.columnWaterLevel, kWorldMaxY);
        for (int y = waterStart; y <= waterEnd; ++y)
        {
            outColumnBlocks[y - kWorldMinY] = BlockType::Water;
        }
    }

    if (surfaceIndex >= 0 && surfaceIndex < kWorldHeight)
    {
        outColumnBlocks[surfaceIndex] = columnContext.surfaceBlockType;
    }

    const int topsoilStart = std::max(kUndergroundStartY, surfaceHeight - columnContext.topsoilDepth);
    for (int y = topsoilStart; y < surfaceHeight; ++y)
    {
        outColumnBlocks[y - kWorldMinY] = isMountainStoneCapLayer(y, surfaceHeight)
            ? BlockType::Stone
            : columnContext.subsurfaceBlockType;
    }

    const int undergroundEnd = std::min(surfaceHeight - columnContext.topsoilDepth - 1, kWorldMaxY);
    for (int y = kUndergroundStartY; y <= undergroundEnd; ++y)
    {
        outColumnBlocks[y - kWorldMinY] = blockTypeAtWithContext(worldX, y, worldZ, columnContext, worldSeed_);
    }
}
}  // namespace vibecraft::world
