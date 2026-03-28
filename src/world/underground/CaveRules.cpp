#include "vibecraft/world/underground/CaveRules.hpp"

#include <cmath>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world::underground
{
namespace
{
constexpr int kCaveRoofBuffer = 5;
constexpr int kSeaLevel = 24;
constexpr int kLavaPoolMinY = 6;
constexpr int kLavaPoolMaxY = 14;
constexpr std::uint32_t kLavaNoiseSeed = 0x8d3b2917U;
}  // namespace

bool shouldCarveCave(const int worldX, const int y, const int worldZ, const int surfaceHeight)
{
    if (y < kUndergroundStartY || y > surfaceHeight - kCaveRoofBuffer)
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

BlockType caveInteriorBlockType(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight)
{
    const double lavaField = noise::fbmNoise2d(
        static_cast<double>(worldX) + static_cast<double>(y) * 0.08,
        static_cast<double>(worldZ) - static_cast<double>(y) * 0.06,
        24.0,
        2,
        kLavaNoiseSeed);

    const double lavaThreshold = 0.78;
    if (y >= kLavaPoolMinY && y <= kLavaPoolMaxY && lavaField > lavaThreshold)
    {
        const double spot = noise::random01(worldX, y * 31 + worldZ, 0x51ab12efU);
        if (spot > 0.94)
        {
            return BlockType::Lava;
        }
    }

    if (y <= kSeaLevel)
    {
        return BlockType::Water;
    }

    static_cast<void>(surfaceHeight);
    return BlockType::Air;
}
}  // namespace vibecraft::world::underground
