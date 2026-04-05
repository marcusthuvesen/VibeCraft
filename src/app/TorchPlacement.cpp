#include "vibecraft/app/TorchPlacement.hpp"

namespace vibecraft::app
{
world::BlockType torchBlockForPlacement(
    const world::BlockType torchBaseBlock,
    const glm::ivec3& solidBlock,
    const glm::ivec3& buildTarget)
{
    if (!world::isTorchBlock(torchBaseBlock))
    {
        return torchBaseBlock;
    }

    const glm::ivec3 delta = buildTarget - solidBlock;
    if (delta.y != 0)
    {
        // Top/bottom face placement uses standing torch.
        return world::BlockType::Torch;
    }
    if (delta.x > 0)
    {
        return world::BlockType::TorchEast;
    }
    if (delta.x < 0)
    {
        return world::BlockType::TorchWest;
    }
    if (delta.z > 0)
    {
        return world::BlockType::TorchSouth;
    }
    if (delta.z < 0)
    {
        return world::BlockType::TorchNorth;
    }
    return world::BlockType::Torch;
}

bool isValidTorchPlacementFace(const glm::ivec3& solidBlock, const glm::ivec3& buildTarget)
{
    const glm::ivec3 delta = buildTarget - solidBlock;
    // Minecraft-style: torches place on top or on vertical sides, not on undersides.
    return delta.y >= 0;
}
}  // namespace vibecraft::app
