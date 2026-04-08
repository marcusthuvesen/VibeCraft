#include "vibecraft/world/TerrainGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"
#include "vibecraft/world/biomes/BiomeBlending.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/BiomeSelection.hpp"
#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeTerrainContribution.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/SurfaceVariantRules.hpp"
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
constexpr int kMountainStoneCapStartY = 110;
constexpr int kMountainStoneCapThickness = 2;
constexpr int kLowlandPondMaxHeightAboveSea = 1;
constexpr int kStarterForestRadius = 160;
constexpr std::uint32_t kPondNoiseSeed = 0xa53f210bU;
constexpr int kSandstoneStratumDepth = 6;
constexpr std::uint32_t kMoisturePocketNoiseSeed = 0xd47a38c1U;

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

struct ColumnContext
{
    int surfaceHeight = kSeaLevel;
    int topsoilDepth = kTopsoilDepth;
    int columnWaterLevel = kSeaLevel;
    int stratumTopExclusive = 0;
    int stratumBottomInclusive = 0;
    bool usesSandStrata = false;
    SurfaceBiome biome = SurfaceBiome::Forest;
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
    // Broad climate bands keep the world readable: plains, desert, snowy plains, and jungle.
    // Keep them large enough to feel like regions, but not so large that spawn-adjacent
    // exploration stays locked in forest for too long.
    const double baseTemperature =
        noise::fbmNoise2d(worldXd, worldZd, 1600.0, 4, mixedSeed(0x8b4d1e29U, worldSeed)) * 2.0 - 1.0;
    const double variation =
        noise::fbmNoise2d(worldXd + 73.0, worldZd - 59.0, 700.0, 3, mixedSeed(0x1c0f3aa7U, worldSeed)) * 2.0 - 1.0;
    const double altitudeCooling = std::clamp(
        static_cast<double>(surfaceHeight - kSeaLevel) / 120.0,
        0.0,
        0.55);
    return baseTemperature + variation * 0.34 - altitudeCooling;
}

[[nodiscard]] double biomeHumidityAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    return noise::fbmNoise2d(worldXd - 31.0, worldZd + 43.0, 1500.0, 4, mixedSeed(0x32a7f1c4U, worldSeed)) * 2.0
        - 1.0;
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

[[nodiscard]] double basinDepthFromContinents(double continents);
[[nodiscard]] double ridgeHeightContribution(double ridges, double continents, double uplands);

[[nodiscard]] double starterSpawnRegionUplift(const int worldX, const int worldZ)
{
    const double distanceFromOrigin = std::hypot(static_cast<double>(worldX), static_cast<double>(worldZ));
    if (distanceFromOrigin >= 720.0)
    {
        return 0.0;
    }

    const double blend = 1.0 - distanceFromOrigin / 720.0;
    return blend * blend * 4.0;
}

[[nodiscard]] double baseTerrainHeightAt(const int worldX, const int worldZ, const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    const double continents =
        noise::fbmNoise2d(worldXd, worldZd, 420.0, 4, mixedSeed(0x1234abcdU, worldSeed)) * 2.0 - 1.0;
    const double uplands =
        noise::fbmNoise2d(worldXd + 91.0, worldZd - 137.0, 180.0, 3, mixedSeed(0x31f4bc9dU, worldSeed)) * 2.0 - 1.0;
    const double ridges = noise::ridgeNoise2d(worldXd, worldZd, 90.0, mixedSeed(0x4422aa11U, worldSeed));
    const double hills =
        noise::fbmNoise2d(worldXd, worldZd, 72.0, 4, mixedSeed(0x90f0c55aU, worldSeed)) * 2.0 - 1.0;
    const double rolling =
        noise::fbmNoise2d(worldXd - 43.0, worldZd + 27.0, 40.0, 3, mixedSeed(0x5b7e2c41U, worldSeed)) * 2.0 - 1.0;
    const double detail =
        noise::fbmNoise2d(worldXd, worldZd, 24.0, 2, mixedSeed(0x7a0f3e19U, worldSeed)) * 2.0 - 1.0;
    return static_cast<double>(kSeaLevel)
        - 1.0
        + continents * 16.0
        + uplands * 6.0
        + ridgeHeightContribution(ridges, continents, uplands)
        + hills * 5.6
        + rolling * 4.2
        + detail * 1.4
        + starterSpawnRegionUplift(worldX, worldZ)
        - basinDepthFromContinents(continents);
}

[[nodiscard]] SurfaceBiome columnBiomeAt(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    if (biomeOverride.has_value())
    {
        return *biomeOverride;
    }

    const long long starterRadiusSq = static_cast<long long>(kStarterForestRadius) * kStarterForestRadius;
    if (static_cast<long long>(worldX) * worldX + static_cast<long long>(worldZ) * worldZ <= starterRadiusSq)
    {
        return SurfaceBiome::Forest;
    }
    const auto rawBiomeAt = [&](const int sampleX, const int sampleZ, const int sampleSurfaceHeight) {
        const double sampleTemperature = biomeTemperatureAt(sampleX, sampleZ, sampleSurfaceHeight, worldSeed);
        const double sampleHumidity = biomeHumidityAt(sampleX, sampleZ, worldSeed);
        return biomes::selectRawSurfaceBiome(
            biomes::BiomeSelectionInputs{
                .worldX = sampleX,
                .worldZ = sampleZ,
                .surfaceHeight = sampleSurfaceHeight,
                .worldSeed = worldSeed,
                .temperature = sampleTemperature,
                .humidity = sampleHumidity,
                .starterRegion = static_cast<long long>(sampleX) * sampleX + static_cast<long long>(sampleZ) * sampleZ <= starterRadiusSq,
            },
            std::nullopt);
    };

    const SurfaceBiome centerBiome = rawBiomeAt(worldX, worldZ, surfaceHeight);
    std::array<SurfaceBiome, biomes::kBiomeBlendNeighborCount> nearbyBiomes{};
    const auto& offsets = biomes::biomeBlendOffsets();
    for (std::size_t i = 0; i < offsets.size(); ++i)
    {
        const int sampleX = worldX + offsets[i].dx;
        const int sampleZ = worldZ + offsets[i].dz;
        const int sampleSurfaceHeight = std::clamp(
            static_cast<int>(std::round(baseTerrainHeightAt(sampleX, sampleZ, worldSeed))),
            36,
            144);
        nearbyBiomes[i] = rawBiomeAt(sampleX, sampleZ, sampleSurfaceHeight);
    }

    return biomes::selectBlendedSurfaceBiome(centerBiome, nearbyBiomes);
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
        noise::fbmNoise2d(worldXd, worldZd, 420.0, 4, mixedSeed(0x1234abcdU, worldSeed)) * 2.0 - 1.0;
    const double uplands =
        noise::fbmNoise2d(worldXd + 91.0, worldZd - 137.0, 180.0, 3, mixedSeed(0x31f4bc9dU, worldSeed)) * 2.0 - 1.0;
    const double ridges = noise::ridgeNoise2d(worldXd, worldZd, 90.0, mixedSeed(0x4422aa11U, worldSeed));
    const auto buildHeightForBiome = [&](const SurfaceBiome biome) {
        const biomes::BiomeVariationSample variation = biomes::sampleBiomeVariation(biome, worldX, worldZ, worldSeed);
        return baseTerrainHeightAt(worldX, worldZ, worldSeed)
            + biomes::biomeTerrainContribution(biome, worldX, worldZ, continents, uplands, ridges, worldSeed)
            + biomes::localSurfaceHeightDelta(biome, variation, worldX, worldZ, worldSeed);
    };

    const int provisionalSurfaceHeight = std::clamp(
        static_cast<int>(std::round(baseTerrainHeightAt(worldX, worldZ, worldSeed))),
        36,
        144);
    const SurfaceBiome provisionalBiome =
        columnBiomeAt(worldX, worldZ, provisionalSurfaceHeight, worldSeed, biomeOverride);
    const int preliminarySurfaceHeight = std::clamp(
        static_cast<int>(std::round(buildHeightForBiome(provisionalBiome))),
        36,
        144);
    const SurfaceBiome resolvedBiome =
        columnBiomeAt(worldX, worldZ, preliminarySurfaceHeight, worldSeed, biomeOverride);
    const double terrainHeight = buildHeightForBiome(resolvedBiome);
    return std::clamp(static_cast<int>(std::round(terrainHeight)), 36, 144);
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

/// Desert columns use sand on the surface; sandstone acts as a compacted stratum below.
[[nodiscard]] bool columnUsesSandStrata(const SurfaceBiome biome)
{
    return biomes::biomeProfile(biome).usesSandStrata;
}

[[nodiscard]] BlockType surfaceBlockTypeAt(
    const SurfaceBiome biome,
    const int surfaceHeight,
    const int maxNeighborSurfaceDelta,
    const double moisturePocket)
{
    const biomes::BiomeProfile& profile = biomes::biomeProfile(biome);
    if (surfaceHeight >= kMountainStoneCapStartY)
    {
        return profile.snowy ? BlockType::SnowGrass : BlockType::Stone;
    }
    if (profile.usesSandStrata)
    {
        if (maxNeighborSurfaceDelta >= 18 || (surfaceHeight >= 112 && maxNeighborSurfaceDelta >= 12))
        {
            return BlockType::Sandstone;
        }
        static_cast<void>(moisturePocket);
        return BlockType::Sand;
    }
    if (profile.snowy)
    {
        if (maxNeighborSurfaceDelta >= 18 || (surfaceHeight >= 126 && maxNeighborSurfaceDelta >= 12))
        {
            return BlockType::Stone;
        }
        return BlockType::SnowGrass;
    }
    if (profile.jungle)
    {
        if (maxNeighborSurfaceDelta >= 18 || (surfaceHeight >= 120 && maxNeighborSurfaceDelta >= 11))
        {
            return BlockType::Stone;
        }
        return BlockType::JungleGrass;
    }
    if (biome == SurfaceBiome::MushroomField)
    {
        if (maxNeighborSurfaceDelta >= 14 || (surfaceHeight >= 108 && maxNeighborSurfaceDelta >= 9))
        {
            return BlockType::Stone;
        }
        return BlockType::MossBlock;
    }
    if (maxNeighborSurfaceDelta >= 20 || (surfaceHeight >= 120 && maxNeighborSurfaceDelta >= 12))
    {
        return BlockType::Stone;
    }
    return BlockType::Grass;
}

[[nodiscard]] BlockType subsurfaceBlockTypeAt(
    const SurfaceBiome biome,
    const int maxNeighborSurfaceDelta,
    const BlockType surfaceBlockType)
{
    if (biomes::biomeProfile(biome).usesSandStrata)
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
    return basinFactor * basinFactor * 9.0;
}

[[nodiscard]] double ridgeHeightContribution(
    const double ridges,
    const double continents,
    const double uplands)
{
    const double ridgeMask = std::clamp((continents + uplands * 0.24 + 0.20) / 1.20, 0.0, 1.0);
    const double shapedRidges = std::pow(std::clamp(ridges, 0.0, 1.0), 1.8);
    return shapedRidges * (2.2 + ridgeMask * 9.4);
}

[[nodiscard]] bool isMountainStoneCapLayer(const int y, const int surfaceHeight)
{
    return surfaceHeight >= kMountainStoneCapStartY && y >= surfaceHeight - kMountainStoneCapThickness;
}

[[nodiscard]] bool shouldFloodLowlandColumn(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const int maxNeighborSurfaceDelta,
    const std::uint32_t worldSeed,
    const SurfaceBiome biome)
{
    if (surfaceHeight < kSeaLevel || surfaceHeight > kSeaLevel + kLowlandPondMaxHeightAboveSea)
    {
        return false;
    }
    if (maxNeighborSurfaceDelta > 2)
    {
        return false;
    }
    if (starterSpawnRegionUplift(worldX, worldZ) > 0.0)
    {
        return false;
    }

    const double floodNoise = noise::fbmNoise2d(
        static_cast<double>(worldX),
        static_cast<double>(worldZ),
        88.0,
        3,
        mixedSeed(kPondNoiseSeed, worldSeed));
    // Forest and swamp biomes get more tiny surface pools (closer to vanilla wetlands behavior).
    const double threshold = biome == SurfaceBiome::Swamp
        ? 0.78
        : (biomes::isForestSurfaceBiome(biome) ? 0.875 : 0.90);
    return floodNoise > threshold;
}

[[nodiscard]] ColumnContext buildColumnContext(
    const int worldX,
    const int worldZ,
    const int surfaceHeight,
    const std::uint32_t worldSeed,
    const std::optional<SurfaceBiome> biomeOverride)
{
    const SurfaceBiome biome = columnBiomeAt(worldX, worldZ, surfaceHeight, worldSeed, biomeOverride);
    const int maxNeighborSurfaceDelta =
        maxNeighborSurfaceDeltaAt(worldX, worldZ, surfaceHeight, worldSeed, biomeOverride);
    const double moisturePocket = moisturePocketNoiseAt(worldX, worldZ, worldSeed);
    const bool floodLowland =
        shouldFloodLowlandColumn(worldX, worldZ, surfaceHeight, maxNeighborSurfaceDelta, worldSeed, biome);
    const bool lushPondPocket = !biomeOverride.has_value()
        && (biomes::isJungleSurfaceBiome(biome) || biomes::isTemperateGrassSurfaceBiome(biome) || biomes::isSnowySurfaceBiome(biome))
        && surfaceHeight >= kSeaLevel - 1
        && surfaceHeight <= kSeaLevel + 2
        && maxNeighborSurfaceDelta <= 1
        && moisturePocket > 0.94;
    const int columnWaterLevel = floodLowland || lushPondPocket ? surfaceHeight + 1 : kSeaLevel;
    const bool usesSandStrata = columnUsesSandStrata(biome);
    const bool forceCanonicalSurface = biomeOverride.has_value();
    const biomes::BiomeTransitionSample transition = forceCanonicalSurface
        ? biomes::BiomeTransitionSample{}
        : biomes::sampleBiomeTransition(
              biome,
              worldX,
              worldZ,
              [&](const int sampleX, const int sampleZ)
              {
                  // Avoid recursive full surface sampling here. This transition is only used to
                  // soften surface variants near biome edges, so a provisional terrain height is
                  // accurate enough and keeps spawn-time probe generation responsive.
                  const int sampleHeight = std::clamp(
                      static_cast<int>(std::round(baseTerrainHeightAt(sampleX, sampleZ, worldSeed))),
                      36,
                      144);
                  return columnBiomeAt(sampleX, sampleZ, sampleHeight, worldSeed, biomeOverride);
              });
    const biomes::BiomeVariationSample variation = biomes::sampleBiomeVariation(biome, worldX, worldZ, worldSeed);
    BlockType surfaceBlockType = forceCanonicalSurface
        ? surfaceBlockTypeAt(biome, surfaceHeight, 0, 0.0)
        : surfaceBlockTypeAt(biome, surfaceHeight, maxNeighborSurfaceDelta, moisturePocket);
    BlockType subsurfaceBlockType = forceCanonicalSurface
        ? subsurfaceBlockTypeAt(biome, 0, surfaceBlockType)
        : subsurfaceBlockTypeAt(biome, maxNeighborSurfaceDelta, surfaceBlockType);
    biomes::SurfaceVariantDecision surfaceVariant{};
    if (!forceCanonicalSurface)
    {
        surfaceVariant = biomes::evaluateSurfaceVariantRules(
            biome,
            variation,
            surfaceHeight,
            maxNeighborSurfaceDelta,
            moisturePocket,
            surfaceBlockType,
            subsurfaceBlockType);
        if (surfaceVariant.surfaceBlock.has_value())
        {
            surfaceBlockType = *surfaceVariant.surfaceBlock;
        }
        if (surfaceVariant.subsurfaceBlock.has_value())
        {
            subsurfaceBlockType = *surfaceVariant.subsurfaceBlock;
        }
        biomes::softenSurfaceVariantForBiomeEdge(
            biomes::biomeProfile(biome),
            transition,
            surfaceBlockType,
            subsurfaceBlockType,
            surfaceVariant);
    }
    int topsoilDepth = topsoilDepthAt(worldX, worldZ, worldSeed);
    if (!forceCanonicalSurface
        && (surfaceBlockType == BlockType::Stone || surfaceBlockType == BlockType::Sandstone)
        && topsoilDepth > 1)
    {
        topsoilDepth -= 1;
    }
    if (!forceCanonicalSurface)
    {
        topsoilDepth += surfaceVariant.topsoilDepthDelta;
    }
    topsoilDepth = std::clamp(topsoilDepth, 1, 6);

    // Underwater columns (ocean floor): override surface/subsurface to sand or gravel,
    // matching Minecraft behaviour. Sandy-strata biomes already place sand themselves.
    if (surfaceHeight < kSeaLevel && !usesSandStrata)
    {
        const biomes::BiomeProfile& underwaterProfile = biomes::biomeProfile(biome);
        if (underwaterProfile.snowy)
        {
            surfaceBlockType = BlockType::Gravel;
            subsurfaceBlockType = BlockType::Gravel;
        }
        else
        {
            surfaceBlockType = BlockType::Sand;
            subsurfaceBlockType = BlockType::Sand;
        }
    }

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

[[nodiscard]] underground::BiomeOreProfile biomeOreProfileFromSurfaceBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Desert:
    case SurfaceBiome::Savanna:
    case SurfaceBiome::SavannaPlateau:
    case SurfaceBiome::WindsweptSavanna:
        return underground::BiomeOreProfile::DustFlats;
    case SurfaceBiome::SnowyPlains:
    case SurfaceBiome::IcePlains:
    case SurfaceBiome::IceSpikePlains:
    case SurfaceBiome::SnowyTaiga:
    case SurfaceBiome::SnowySlopes:
    case SurfaceBiome::FrozenPeaks:
    case SurfaceBiome::JaggedPeaks:
        return underground::BiomeOreProfile::IceShelf;
    case SurfaceBiome::Jungle:
    case SurfaceBiome::SparseJungle:
    case SurfaceBiome::BambooJungle:
        return underground::BiomeOreProfile::VerdantGrove;
    case SurfaceBiome::Plains:
    case SurfaceBiome::SunflowerPlains:
    case SurfaceBiome::Meadow:
    case SurfaceBiome::WindsweptHills:
    case SurfaceBiome::StonyPeaks:
    case SurfaceBiome::Forest:
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::OldGrowthSpruceTaiga:
    case SurfaceBiome::OldGrowthPineTaiga:
    case SurfaceBiome::Swamp:
    case SurfaceBiome::MushroomField:
    case SurfaceBiome::Taiga:
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
            biomeOreProfileFromSurfaceBiome(columnContext.biome)))
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
    return columnBiomeAt(worldX, worldZ, surfaceHeight, worldSeed_, biomeOverride_);
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

    const int topsoilStart = std::max(kUndergroundStartY, surfaceHeight - columnContext.topsoilDepth);
    for (int y = topsoilStart; y <= surfaceHeight; ++y)
    {
        outColumnBlocks[y - kWorldMinY] =
            blockTypeAtWithContext(worldX, y, worldZ, columnContext, worldSeed_);
    }

    const int undergroundEnd = std::min(surfaceHeight - columnContext.topsoilDepth - 1, kWorldMaxY);
    for (int y = kUndergroundStartY; y <= undergroundEnd; ++y)
    {
        outColumnBlocks[y - kWorldMinY] = blockTypeAtWithContext(worldX, y, worldZ, columnContext, worldSeed_);
    }
}
}  // namespace vibecraft::world
