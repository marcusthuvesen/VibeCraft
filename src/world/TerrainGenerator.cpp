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

[[nodiscard]] bool isSandyBiome(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight > kSandSurfaceMaxHeight)
    {
        return false;
    }

    const double drynessNoise = std::sin(static_cast<double>(worldX) * 0.032) +
        std::cos(static_cast<double>(worldZ) * 0.027) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.015);
    return drynessNoise > 1.05;
}

/// Beaches and dry lowlands use sand on the surface; sandstone sits below that (uses existing atlas tile).
[[nodiscard]] bool columnUsesSandStrata(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight <= kSeaLevel + kBeachMaxHeightAboveSea)
    {
        return true;
    }
    return isSandyBiome(worldX, worldZ, surfaceHeight);
}

[[nodiscard]] BlockType surfaceBlockTypeAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight >= kMountainStoneCapStartY)
    {
        return BlockType::Stone;
    }
    if (surfaceHeight <= kSeaLevel + kBeachMaxHeightAboveSea)
    {
        return BlockType::Sand;
    }
    return isSandyBiome(worldX, worldZ, surfaceHeight) ? BlockType::Sand : BlockType::Grass;
}

[[nodiscard]] BlockType subsurfaceBlockTypeAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight <= kSeaLevel + kBeachMaxHeightAboveSea)
    {
        return BlockType::Sand;
    }
    return isSandyBiome(worldX, worldZ, surfaceHeight) ? BlockType::Sand : BlockType::Dirt;
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

BlockType TerrainGenerator::blockTypeAt(const int worldX, const int y, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    const int topsoilDepth = topsoilDepthAt(worldX, worldZ);
    const bool floodLowland = shouldFloodLowlandColumn(worldX, worldZ, surfaceHeight);
    const int columnWaterLevel = floodLowland ? surfaceHeight + 1 : kSeaLevel;

    if (shouldPlaceBedrock(y))
    {
        return BlockType::Bedrock;
    }
    if (y > surfaceHeight)
    {
        return y <= columnWaterLevel ? BlockType::Water : BlockType::Air;
    }
    if (y == surfaceHeight)
    {
        return surfaceBlockTypeAt(worldX, worldZ, surfaceHeight);
    }
    if (isMountainStoneCapLayer(y, surfaceHeight))
    {
        return BlockType::Stone;
    }
    if (y >= surfaceHeight - topsoilDepth)
    {
        return subsurfaceBlockTypeAt(worldX, worldZ, surfaceHeight);
    }
    if (underground::shouldCarveCave(worldX, y, worldZ, surfaceHeight))
    {
        return underground::caveInteriorBlockType(worldX, y, worldZ, surfaceHeight);
    }

    if (columnUsesSandStrata(worldX, worldZ, surfaceHeight))
    {
        const int stratumTopExclusive = surfaceHeight - topsoilDepth;
        const int stratumBottomInclusive = stratumTopExclusive - kSandstoneStratumDepth;
        if (y < stratumTopExclusive && y >= stratumBottomInclusive)
        {
            return BlockType::Sandstone;
        }
    }

    const BlockType hostBlockType = undergroundBlockTypeAt(worldX, y, worldZ);
    if (const std::optional<BlockType> ore = underground::selectOreVeinBlock(
            worldX, y, worldZ, surfaceHeight, hostBlockType))
    {
        return *ore;
    }
    return hostBlockType;
}
}  // namespace vibecraft::world
