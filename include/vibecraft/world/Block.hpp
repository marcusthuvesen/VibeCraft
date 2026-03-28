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
    Bedrock
};

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air;
}
}  // namespace vibecraft::world
