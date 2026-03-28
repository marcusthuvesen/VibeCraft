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
constexpr int kDeepslateStartY = 18;

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

[[nodiscard]] BlockType undergroundBlockTypeAt(const int y)
{
    return y < kDeepslateStartY ? BlockType::Deepslate : BlockType::Stone;
}

[[nodiscard]] bool shouldPlaceCoalOre(const int worldX, const int y, const int worldZ, const int surfaceHeight)
{
    if (y < kCoalMinY || y > surfaceHeight - kCoalSurfaceBuffer)
    {
        return false;
    }

    const double oreDensity = std::sin(static_cast<double>(worldX) * 0.23 + static_cast<double>(worldZ) * 0.11) +
        std::cos(static_cast<double>(worldZ) * 0.19 - static_cast<double>(y) * 0.07) +
        std::sin(static_cast<double>(y) * 0.31 + static_cast<double>(worldX) * 0.05);

    return oreDensity > 2.35;
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

    if (y > surfaceHeight)
    {
        return BlockType::Air;
    }
    if (y == surfaceHeight)
    {
        return BlockType::Grass;
    }
    if (y >= surfaceHeight - kTopsoilDepth)
    {
        return BlockType::Dirt;
    }
    if (shouldCarveCave(worldX, y, worldZ, surfaceHeight))
    {
        return BlockType::Air;
    }
    if (shouldPlaceCoalOre(worldX, y, worldZ, surfaceHeight))
    {
        return BlockType::CoalOre;
    }
    return undergroundBlockTypeAt(y);
}
}  // namespace vibecraft::world
