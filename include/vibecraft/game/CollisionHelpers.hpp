#pragma once

#include <glm/vec3.hpp>

namespace vibecraft::world
{
class World;
}

namespace vibecraft::game
{
struct Aabb
{
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

inline constexpr float kAabbEpsilon = 0.0001f;

[[nodiscard]] inline Aabb aabbAtFeet(const glm::vec3& feetPosition, const float halfWidth, const float height)
{
    return Aabb{
        .min = {feetPosition.x - halfWidth, feetPosition.y, feetPosition.z - halfWidth},
        .max = {feetPosition.x + halfWidth, feetPosition.y + height, feetPosition.z + halfWidth},
    };
}

[[nodiscard]] bool collidesWithSolidBlock(const world::World& worldState, const Aabb& aabb);
}  // namespace vibecraft::game
