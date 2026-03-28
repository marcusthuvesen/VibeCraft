#pragma once

#include <cstdint>

namespace vibecraft::world
{
enum class BlockType : std::uint8_t
{
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Deepslate,
    CoalOre,
    Sand,
    Bedrock,
    Water,
    IronOre,
    GoldOre,
    DiamondOre,
    EmeraldOre,
    Lava,
    TreeTrunk,
    TreeCrown
};

[[nodiscard]] constexpr bool isFluid(const BlockType blockType)
{
    return blockType == BlockType::Water || blockType == BlockType::Lava;
}

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType);
}

[[nodiscard]] constexpr bool isRenderable(const BlockType blockType)
{
    return blockType != BlockType::Air;
}

[[nodiscard]] constexpr bool occludesFaces(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && blockType != BlockType::TreeCrown;
}
}  // namespace vibecraft::world
