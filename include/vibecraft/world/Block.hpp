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
    Water
};

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air && blockType != BlockType::Water;
}

[[nodiscard]] constexpr bool isRenderable(const BlockType blockType)
{
    return blockType != BlockType::Air;
}

[[nodiscard]] constexpr bool occludesFaces(const BlockType blockType)
{
    return blockType != BlockType::Air && blockType != BlockType::Water;
}
}  // namespace vibecraft::world
