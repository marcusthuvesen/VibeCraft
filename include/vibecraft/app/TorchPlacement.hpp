#pragma once

#include <glm/vec3.hpp>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
[[nodiscard]] world::BlockType torchBlockForPlacement(
    world::BlockType torchBaseBlock,
    const glm::ivec3& solidBlock,
    const glm::ivec3& buildTarget);

[[nodiscard]] bool isValidTorchPlacementFace(
    const glm::ivec3& solidBlock,
    const glm::ivec3& buildTarget);
}  // namespace vibecraft::app
