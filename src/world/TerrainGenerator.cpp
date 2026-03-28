#include "vibecraft/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vibecraft::world
{
namespace
{
constexpr int kSeaLevel = 24;
constexpr int kTopsoilDepth = 3;
constexpr int kMinCaveY = 4;
constexpr int kCaveRoofBuffer = 5;
constexpr int kCoalMinY = 8;
constexpr int kCoalSurfaceBuffer = 6;
constexpr int kDeepslateFullStartY = 14;
constexpr int kDeepslateTransitionEndY = 22;
constexpr int kSandSurfaceMaxHeight = 26;
constexpr int kBeachMaxHeightAboveSea = 1;
constexpr int kMountainStoneCapStartY = 44;
constexpr int kMountainStoneCapThickness = 2;
constexpr int kBedrockNoiseMaxY = 2;
constexpr int kLowlandPondMaxHeightAboveSea = 3;
constexpr std::uint32_t kPondNoiseSeed = 0xa53f210bU;

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

[[nodiscard]] std::uint32_t hashCoordinates(const int x, const int z, const std::uint32_t seed)
{
    std::uint32_t hash = seed;
    hash ^= static_cast<std::uint32_t>(x) * 0x9e3779b9U;
    hash = (hash << 6U) ^ (hash >> 2U);
    hash ^= static_cast<std::uint32_t>(z) * 0x85ebca6bU;
    hash *= 0xc2b2ae35U;
    hash ^= hash >> 16U;
    return hash;
}

[[nodiscard]] double random01(const int x, const int z, const std::uint32_t seed)
{
    constexpr double kInvMax = 1.0 / static_cast<double>(0xffffffffU);
    return static_cast<double>(hashCoordinates(x, z, seed)) * kInvMax;
}

[[nodiscard]] double smoothstep(const double t)
{
    return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] double valueNoise2d(
    const double worldX,
    const double worldZ,
    const double scale,
    const std::uint32_t seed)
{
    const double scaledX = worldX / scale;
    const double scaledZ = worldZ / scale;
    const int x0 = static_cast<int>(std::floor(scaledX));
    const int z0 = static_cast<int>(std::floor(scaledZ));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const double tx = smoothstep(scaledX - static_cast<double>(x0));
    const double tz = smoothstep(scaledZ - static_cast<double>(z0));

    const double n00 = random01(x0, z0, seed);
    const double n10 = random01(x1, z0, seed);
    const double n01 = random01(x0, z1, seed);
    const double n11 = random01(x1, z1, seed);

    const double nx0 = std::lerp(n00, n10, tx);
    const double nx1 = std::lerp(n01, n11, tx);
    return std::lerp(nx0, nx1, tz);
}

[[nodiscard]] double fbmNoise2d(
    const double worldX,
    const double worldZ,
    const double baseScale,
    const int octaves,
    const std::uint32_t seed)
{
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    double scale = baseScale;

    for (int octave = 0; octave < octaves; ++octave)
    {
        value += valueNoise2d(worldX, worldZ, scale, seed + static_cast<std::uint32_t>(octave) * 101U)
            * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        scale *= 0.5;
    }

    return amplitudeSum > 0.0 ? value / amplitudeSum : 0.0;
}

[[nodiscard]] double ridgeNoise2d(
    const double worldX,
    const double worldZ,
    const double scale,
    const std::uint32_t seed)
{
    const double base = fbmNoise2d(worldX, worldZ, scale, 3, seed);
    return 1.0 - std::abs(base * 2.0 - 1.0);
}

[[nodiscard]] int topsoilDepthAt(const int worldX, const int worldZ)
{
    const double depthNoise = valueNoise2d(
        static_cast<double>(worldX),
        static_cast<double>(worldZ),
        48.0,
        0x4f1bbcdcU);
    return kTopsoilDepth + static_cast<int>(depthNoise * 2.0);
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
    const double combinedOreNoise = veinNoise + pocketNoise * 0.35;
    const double altitudeBias =
        std::clamp((static_cast<double>(y) - static_cast<double>(kSeaLevel)) / 24.0, -0.35, 0.35);
    const double hostBias = hostBlockType == BlockType::Deepslate ? 0.18 : 0.0;
    const double threshold = 2.3 - altitudeBias + hostBias;
    return combinedOreNoise > threshold;
}

[[nodiscard]] bool shouldFloodLowlandColumn(const int worldX, const int worldZ, const int surfaceHeight)
{
    if (surfaceHeight < kSeaLevel || surfaceHeight > kSeaLevel + kLowlandPondMaxHeightAboveSea)
    {
        return false;
    }

    const double floodNoise = fbmNoise2d(
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
    const double continents = fbmNoise2d(worldXd, worldZd, 220.0, 4, 0x1234abcdU) * 2.0 - 1.0;
    const double ridges = ridgeNoise2d(worldXd, worldZd, 110.0, 0x4422aa11U);
    const double hills = fbmNoise2d(worldXd, worldZd, 72.0, 4, 0x90f0c55aU) * 2.0 - 1.0;
    const double detail = fbmNoise2d(worldXd, worldZd, 28.0, 3, 0x7a0f3e19U) * 2.0 - 1.0;

    const double terrainHeight = static_cast<double>(kSeaLevel)
        - 5.0
        + continents * 14.0
        + ridges * 8.0
        + hills * 7.0
        + detail * 3.0;
    return std::clamp(static_cast<int>(std::round(terrainHeight)), 6, 56);
}

BlockType TerrainGenerator::blockTypeAt(const int worldX, const int y, const int worldZ) const
{
    const int surfaceHeight = surfaceHeightAt(worldX, worldZ);
    const int topsoilDepth = topsoilDepthAt(worldX, worldZ);
    const bool floodLowland = shouldFloodLowlandColumn(worldX, worldZ, surfaceHeight);
    const int columnWaterLevel = floodLowland ? surfaceHeight + 1 : kSeaLevel;

    if (shouldPlaceBedrock(worldX, y, worldZ))
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
    if (shouldCarveCave(worldX, y, worldZ, surfaceHeight))
    {
        return y <= kSeaLevel ? BlockType::Water : BlockType::Air;
    }

    const BlockType hostBlockType = undergroundBlockTypeAt(worldX, y, worldZ);
    if (shouldPlaceCoalOre(worldX, y, worldZ, surfaceHeight, hostBlockType))
    {
        return BlockType::CoalOre;
    }
    return hostBlockType;
}
}  // namespace vibecraft::world
