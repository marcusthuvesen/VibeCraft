#include "vibecraft/game/CollisionHelpers.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::game
{
bool collidesWithSolidBlock(const world::World& worldState, const Aabb& aabb)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - kAabbEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - kAabbEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (world::isSolid(worldState.blockAt(x, y, z)))
                {
                    return true;
                }
            }
        }
    }
    return false;
}
}  // namespace vibecraft::game
