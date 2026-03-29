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
constexpr int kSandSurfaceMaxHeight = 68;
constexpr int kBeachMaxHeightAboveSea = 1;
constexpr int kMountainStoneCapStartY = 110;
constexpr int kMountainStoneCapThickness = 2;
constexpr int kLowlandPondMaxHeightAboveSea = 3;
constexpr std::uint32_t kPondNoiseSeed = 0xa53f210bU;
constexpr int kSandstoneStratumDepth = 6;

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

[[nodiscard]] int topsoilDepthAt(const int worldX, const int worldZ)
{
    const double depthNoise = noise::valueNoise2d(
        static_cast<double>(worldX),
        static_cast<double>(worldZ),
        48.0,
        0x4f1bbcdcU);
    return kTopsoilDepth + static_cast<int>(depthNoise * 2.0);
}

[[nodiscard]] double transitionBandNoise(const int worldX, const int worldZ)
{
    return 0.5 + 0.25 * std::sin(static_cast<double>(worldX) * 0.07 + static_cast<double>(worldZ) * 0.05) +
        0.25 * std::cos(static_cast<double>(worldX - worldZ) * 0.04);
}

[[nodiscard]] BlockType undergroundBlockTypeAt(const int worldX, const int y, const int worldZ)
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
    return transitionBandNoise(worldX, worldZ) < transitionBias ? BlockType::Deepslate : BlockType::Stone;
}

[[nodiscard]] double biomeTemperatureAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    // Broader climate scales create larger contiguous biome regions (closer to Minecraft feel).
    const double baseTemperature = noise::fbmNoise2d(worldXd, worldZd, 360.0, 4, 0x8b4d1e29U) * 2.0 - 1.0;
    const double variation = noise::fbmNoise2d(worldXd + 73.0, worldZd - 59.0, 180.0, 3, 0x1c0f3aa7U) * 2.0 - 1.0;
    const double altitudeCooling = std::clamp(
        static_cast<double>(surfaceHeight - kSeaLevel) / 120.0,
        0.0,
        0.55);
    return baseTemperature + variation * 0.33 - altitudeCooling;
}

[[nodiscard]] double biomeHumidityAt(const int worldX, const int worldZ)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    return noise::fbmNoise2d(worldXd - 31.0, worldZd + 43.0, 340.0, 4, 0x32a7f1c4U) * 2.0 - 1.0;
}

[[nodiscard]] ColumnBiome columnBiomeAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight <= kSeaLevel + kBeachMaxHeightAboveSea)
    {
        return ColumnBiome::Sandy;
    }

    const double temperature = biomeTemperatureAt(worldX, worldZ, surfaceHeight);
    const double humidity = biomeHumidityAt(worldX, worldZ);
    if (surfaceHeight <= kSandSurfaceMaxHeight && temperature > 0.22 && humidity < -0.04)
    {
        return ColumnBiome::Sandy;
    }
    if (temperature < -0.12 && humidity > -0.05)
    {
        return ColumnBiome::Snowy;
    }
    if (temperature > 0.14 && humidity > 0.03)
    {
        return ColumnBiome::Jungle;
    }
    return ColumnBiome::TemperateGrassland;
}

/// Beaches and dry lowlands use sand on the surface; sandstone sits below that (uses existing atlas tile).
[[nodiscard]] bool columnUsesSandStrata(const ColumnBiome biome)
{
    return biome == ColumnBiome::Sandy;
}

[[nodiscard]] BlockType surfaceBlockTypeAt(const ColumnBiome biome, const int surfaceHeight)
{
    if (surfaceHeight >= kMountainStoneCapStartY)
    {
        return biome == ColumnBiome::Snowy ? BlockType::SnowGrass : BlockType::Stone;
    }
    if (biome == ColumnBiome::Sandy)
    {
        return BlockType::Sand;
    }
    if (biome == ColumnBiome::Snowy)
    {
        return BlockType::SnowGrass;
    }
    if (biome == ColumnBiome::Jungle)
    {
        return BlockType::JungleGrass;
    }
    return BlockType::Grass;
}

[[nodiscard]] BlockType subsurfaceBlockTypeAt(const ColumnBiome biome)
{
    return biome == ColumnBiome::Sandy ? BlockType::Sand : BlockType::Dirt;
}

[[nodiscard]] bool isMountainStoneCapLayer(const int y, const int surfaceHeight)
{
    return surfaceHeight >= kMountainStoneCapStartY && y >= surfaceHeight - kMountainStoneCapThickness;
}

[[nodiscard]] bool shouldFloodLowlandColumn(const int worldX, const int worldZ, const int surfaceHeight)
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
        kPondNoiseSeed);
    return floodNoise > 0.66;
}

[[nodiscard]] ColumnContext buildColumnContext(const int worldX, const int worldZ, const int surfaceHeight)
{
    const int topsoilDepth = topsoilDepthAt(worldX, worldZ);
    const bool floodLowland = shouldFloodLowlandColumn(worldX, worldZ, surfaceHeight);
    const int columnWaterLevel = floodLowland ? surfaceHeight + 1 : kSeaLevel;
    const ColumnBiome biome = columnBiomeAt(worldX, worldZ, surfaceHeight);
    const bool usesSandStrata = columnUsesSandStrata(biome);
    const int stratumTopExclusive = surfaceHeight - topsoilDepth;
    return ColumnContext{
        .surfaceHeight = surfaceHeight,
        .topsoilDepth = topsoilDepth,
        .columnWaterLevel = columnWaterLevel,
        .stratumTopExclusive = stratumTopExclusive,
        .stratumBottomInclusive = stratumTopExclusive - kSandstoneStratumDepth,
        .usesSandStrata = usesSandStrata,
        .biome = biome,
        .surfaceBlockType = surfaceBlockTypeAt(biome, surfaceHeight),
        .subsurfaceBlockType = subsurfaceBlockTypeAt(biome),
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

[[nodiscard]] BlockType blockTypeAtWithContext(
    const int worldX,
    const int y,
    const int worldZ,
    const ColumnContext& columnContext)
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

    const BlockType hostBlockType = undergroundBlockTypeAt(worldX, y, worldZ);
    if (const std::optional<BlockType> ore = underground::selectOreVeinBlock(
            worldX, y, worldZ, columnContext.surfaceHeight, hostBlockType))
    {
        return *ore;
    }
    return hostBlockType;
}
}  // namespace

int TerrainGenerator::surfaceHeightAt(const int worldX, const int worldZ) const
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    const double continents = noise::fbmNoise2d(worldXd, worldZd, 220.0, 4, 0x1234abcdU) * 2.0 - 1.0;
    const double ridges = noise::ridgeNoise2d(worldXd, worldZd, 110.0, 0x4422aa11U);
    const double hills = noise::fbmNoise2d(worldXd, worldZd, 72.0, 4, 0x90f0c55aU) * 2.0 - 1.0;
    const double detail = noise::fbmNoise2d(worldXd, worldZd, 28.0, 3, 0x7a0f3e19U) * 2.0 - 1.0;

    const double terrainHeight = static_cast<double>(kSeaLevel)
        - 8.0
        + continents * 34.0
        + ridges * 18.0
        + hills * 12.0
        + detail * 6.0;
    return std::clamp(static_cast<int>(std::round(terrainHeight)), -20, 190);
}

SurfaceBiome TerrainGenerator::surfaceBiomeAt(const int worldX, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    return toSurfaceBiome(columnBiomeAt(worldX, worldZ, surfaceHeight));
}

BlockType TerrainGenerator::blockTypeAt(const int worldX, const int y, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    return blockTypeAtWithContext(worldX, y, worldZ, buildColumnContext(worldX, worldZ, surfaceHeight));
}

void TerrainGenerator::fillColumn(const int worldX, const int worldZ, BlockType* const outColumnBlocks) const
{
    if (outColumnBlocks == nullptr)
    {
        return;
    }

    std::fill(outColumnBlocks, outColumnBlocks + kWorldHeight, BlockType::Air);

    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    const ColumnContext columnContext = buildColumnContext(worldX, worldZ, surfaceHeight);
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
        outColumnBlocks[y - kWorldMinY] = blockTypeAtWithContext(worldX, y, worldZ, columnContext);
    }
}
}  // namespace vibecraft::world
