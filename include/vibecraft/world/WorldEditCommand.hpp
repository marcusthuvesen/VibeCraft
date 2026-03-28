#pragma once

#include <glm/vec3.hpp>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world
{
enum class WorldEditAction
{
    Place,
    Remove
};

struct WorldEditCommand
{
    WorldEditAction action = WorldEditAction::Place;
    glm::ivec3 position{0, 0, 0};
    BlockType blockType = BlockType::Dirt;
};
}  // namespace vibecraft::world
