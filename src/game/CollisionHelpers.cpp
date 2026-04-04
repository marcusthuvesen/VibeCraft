#include "vibecraft/game/CollisionHelpers.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool aabbIntersectsBlockBox(
    const Aabb& aabb,
    const int blockX,
    const int blockY,
    const int blockZ,
    const world::BlockCollisionBox& box)
{
    const float minX = static_cast<float>(blockX) + box.minX;
    const float maxX = static_cast<float>(blockX) + box.maxX;
    const float minY = static_cast<float>(blockY) + box.minY;
    const float maxY = static_cast<float>(blockY) + box.maxY;
    const float minZ = static_cast<float>(blockZ) + box.minZ;
    const float maxZ = static_cast<float>(blockZ) + box.maxZ;

    return aabb.max.x > minX + kAabbEpsilon && aabb.min.x < maxX - kAabbEpsilon
        && aabb.max.y > minY + kAabbEpsilon && aabb.min.y < maxY - kAabbEpsilon
        && aabb.max.z > minZ + kAabbEpsilon && aabb.min.z < maxZ - kAabbEpsilon;
}
}  // namespace

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
                const world::BlockType blockType = worldState.blockAt(x, y, z);
                if (world::isSolid(blockType))
                {
                    return true;
                }
                if (world::hasCustomCollisionBox(blockType)
                    && aabbIntersectsBlockBox(
                        aabb, x, y, z, world::collisionBoxForBlockType(blockType)))
                {
                    return true;
                }
            }
        }
    }
    return false;
}
}  // namespace vibecraft::game
