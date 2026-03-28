#include "vibecraft/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace vibecraft::world
{
namespace
{
constexpr int kTopsoilDepth = 3;
constexpr int kMinCaveY = 4;
constexpr int kCaveRoofBuffer = 5;
constexpr int kCoalMinY = 8;
constexpr int kCoalSurfaceBuffer = 6;
constexpr int kDeepslateFullStartY = 14;
constexpr int kDeepslateTransitionEndY = 22;
constexpr int kSandSurfaceMaxHeight = 26;
constexpr int kBedrockNoiseMaxY = 2;

[[nodiscard]] bool shouldPlaceBedrock(const int worldX, const int y, const int worldZ)
{
    if (y <= 0)
    {
        return true;
    }

    if (y > kBedrockNoiseMaxY)
    {
        return false;
    }

    const double bedrockNoise = std::sin(static_cast<double>(worldX) * 0.47) +
        std::cos(static_cast<double>(worldZ) * 0.41) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.19);

    if (y == 1)
    {
        return bedrockNoise > 1.05;
    }

    return bedrockNoise > 1.6;
}

[[nodiscard]] bool shouldCarveCave(const int worldX, const int y, const int worldZ, const int surfaceHeight)
{
    if (y < kMinCaveY || y > surfaceHeight - kCaveRoofBuffer)
    {
        return false;
    }

    const double caveDensity = std::sin(static_cast<double>(worldX) * 0.11) +
        std::cos(static_cast<double>(worldZ) * 0.13) +
        std::sin(static_cast<double>(worldX + worldZ) * 0.05) +
        std::cos(static_cast<double>(y) * 0.21 + static_cast<double>(worldX) * 0.04) +
        std::sin(static_cast<double>(y) * 0.17 - static_cast<double>(worldZ) * 0.03) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.09 + static_cast<double>(y) * 0.12);

    return caveDensity > 2.2;
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

[[nodiscard]] BlockType surfaceBlockTypeAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    return isSandyBiome(worldX, worldZ, surfaceHeight) ? BlockType::Sand : BlockType::Grass;
}

[[nodiscard]] BlockType subsurfaceBlockTypeAt(const int worldX, const int worldZ, const int surfaceHeight)
{
    return isSandyBiome(worldX, worldZ, surfaceHeight) ? BlockType::Sand : BlockType::Dirt;
}

[[nodiscard]] bool shouldPlaceCoalOre(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType)
{
    if (y < kCoalMinY || y > surfaceHeight - kCoalSurfaceBuffer)
    {
        return false;
    }

    if (hostBlockType != BlockType::Stone && hostBlockType != BlockType::Deepslate)
    {
        return false;
    }

    const double veinRegionNoise = std::sin(static_cast<double>(worldX) * 0.041) +
        std::cos(static_cast<double>(worldZ) * 0.037) +
        std::sin(static_cast<double>(worldX + worldZ) * 0.024) +
        std::cos(static_cast<double>(worldX - worldZ) * 0.029);

    if (veinRegionNoise <= 2.15)
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.23 + static_cast<double>(y) * 0.11) +
        std::cos(static_cast<double>(worldZ) * 0.19 - static_cast<double>(y) * 0.08) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.17 + static_cast<double>(y) * 0.05);
    const double pocketNoise = std::sin(static_cast<double>(worldX) * 0.53) +
        std::cos(static_cast<double>(y) * 0.47) +
        std::sin(static_cast<double>(worldZ) * 0.43);

    return veinNoise + pocketNoise * 0.35 > 2.3;
}
}  // namespace

int TerrainGenerator::surfaceHeightAt(const int worldX, const int worldZ) const
{
    const double hills = std::sin(static_cast<double>(worldX) * 0.18) * 4.0;
    const double ridges = std::cos(static_cast<double>(worldZ) * 0.13) * 3.0;
    const double variation = std::sin(static_cast<double>(worldX + worldZ) * 0.07) * 2.0;
    const int surfaceHeight = static_cast<int>(24.0 + hills + ridges + variation);
    return std::clamp(surfaceHeight, 6, 48);
}

BlockType TerrainGenerator::blockTypeAt(const int worldX, const int y, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);

    if (shouldPlaceBedrock(worldX, y, worldZ))
    {
        return BlockType::Bedrock;
    }
    if (y > surfaceHeight)
    {
        return BlockType::Air;
    }
    if (y == surfaceHeight)
    {
        return surfaceBlockTypeAt(worldX, worldZ, surfaceHeight);
    }
    if (y >= surfaceHeight - kTopsoilDepth)
    {
        return subsurfaceBlockTypeAt(worldX, worldZ, surfaceHeight);
    }
    if (shouldCarveCave(worldX, y, worldZ, surfaceHeight))
    {
        return BlockType::Air;
    }

    const BlockType hostBlockType = undergroundBlockTypeAt(worldX, y, worldZ);
    if (shouldPlaceCoalOre(worldX, y, worldZ, surfaceHeight, hostBlockType))
    {
        return BlockType::CoalOre;
    }
    return hostBlockType;
}
}  // namespace vibecraft::world
