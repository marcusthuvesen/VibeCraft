#pragma once

#include <cstdint>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world
{
enum class BlockFace : std::uint8_t
{
    Top = 0,
    Bottom,
    Side
};

struct BlockTextureTiles
{
    std::uint8_t top = 0;
    std::uint8_t bottom = 0;
    std::uint8_t side = 0;
};

struct BlockMetadata
{
    std::uint32_t debugColor = 0xffffffff;
    BlockTextureTiles textureTiles{};
    float hardness = 0.0f;
    bool breakable = false;
};

[[nodiscard]] constexpr BlockMetadata blockMetadata(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::Grass:
        return {
            .debugColor = 0xff4caf50,
            .textureTiles = {.top = 0, .bottom = 1, .side = 0},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::Dirt:
        return {
            .debugColor = 0xff795548,
            .textureTiles = {.top = 1, .bottom = 1, .side = 1},
            .hardness = 0.5f,
            .breakable = true,
        };
    case BlockType::Stone:
        return {
            .debugColor = 0xff9e9e9e,
            .textureTiles = {.top = 2, .bottom = 2, .side = 2},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::Deepslate:
        return {
            .debugColor = 0xff455a64,
            .textureTiles = {.top = 2, .bottom = 2, .side = 2},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::CoalOre:
        return {
            .debugColor = 0xff607d8b,
            .textureTiles = {.top = 2, .bottom = 2, .side = 2},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::Sand:
        return {
            .debugColor = 0xffd7b985,
            .textureTiles = {.top = 3, .bottom = 3, .side = 3},
            .hardness = 0.5f,
            .breakable = true,
        };
    case BlockType::Bedrock:
        return {
            .debugColor = 0xff202124,
            .textureTiles = {.top = 2, .bottom = 2, .side = 2},
            .hardness = 999.0f,
            .breakable = false,
        };
    case BlockType::Air:
    default:
        return {};
    }
}

[[nodiscard]] constexpr std::uint8_t textureTileIndex(const BlockType blockType, const BlockFace face)
{
    const BlockTextureTiles tiles = blockMetadata(blockType).textureTiles;

    switch (face)
    {
    case BlockFace::Top:
        return tiles.top;
    case BlockFace::Bottom:
        return tiles.bottom;
    case BlockFace::Side:
    default:
        return tiles.side;
    }
}
}  // namespace vibecraft::world
