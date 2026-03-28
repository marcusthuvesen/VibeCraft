#include "vibecraft/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace vibecraft::world
{
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
    if (y >= surfaceHeight - 3)
    {
        return BlockType::Dirt;
    }
    return BlockType::Stone;
}
}  // namespace vibecraft::world
