#include "vibecraft/world/underground/CaveRules.hpp"

#include <algorithm>
#include <cmath>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world::underground
{
namespace
{
/// Blocks below surface before 3D cave carve is allowed (keeps shallow regolith solid).
constexpr int kCaveRoofBuffer = 20;
constexpr int kSeaLevel = 63;
constexpr int kLavaPoolMinY = -54;
constexpr int kLavaPoolMaxY = -20;
constexpr std::uint32_t kLavaNoiseSeed = 0x8d3b2917U;
/// Base carve threshold tuned for sparse expedition caverns instead of swiss-cheese terrain.
constexpr double kCaveDensityThresholdDeep = 2.48;
/// Extra threshold added near the top of the cave band so shallow underground rarely carves.
constexpr double kCaveShallowThresholdBoost = 0.52;
}  // namespace

bool shouldCarveCave(const int worldX, const int y, const int worldZ, const int surfaceHeight)
{
    const int caveCeilingY = surfaceHeight - kCaveRoofBuffer;
    if (y < kUndergroundStartY || y > caveCeilingY)
    {
        return false;
    }

    const double caveDensity = std::sin(static_cast<double>(worldX) * 0.11) +
        std::cos(static_cast<double>(worldZ) * 0.13) +
        std::sin(static_cast<double>(worldX + worldZ) * 0.05) +
        std::cos(static_cast<double>(y) * 0.21 + static_cast<double>(worldX) * 0.04) +
        std::sin(static_cast<double>(y) * 0.17 - static_cast<double>(worldZ) * 0.03) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.09 + static_cast<double>(y) * 0.12);

    const int verticalSpan = std::max(1, caveCeilingY - kUndergroundStartY);
    const double shallow01 =
        static_cast<double>(y - kUndergroundStartY) / static_cast<double>(verticalSpan);
    const double carveThreshold = kCaveDensityThresholdDeep + kCaveShallowThresholdBoost * shallow01;

    return caveDensity > carveThreshold;
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
        // Thermal vents use chunk atlas tile 13; keep the pocket dry or hot instead of mixing with aquifers.
        if (spot > 0.88)
        {
            return BlockType::Lava;
        }
        return BlockType::Air;
    }

    if (y <= kSeaLevel)
    {
        return BlockType::Water;
    }

    static_cast<void>(surfaceHeight);
    return BlockType::Air;
}
}  // namespace vibecraft::world::underground
