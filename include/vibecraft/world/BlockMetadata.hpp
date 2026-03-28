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

// Vertex color multiplies the atlas sample; white shows materials as authored in assets/textures/materials.
// Tile indices follow scripts/build_chunk_atlas.sh row-major order on a 5x4 atlas.
[[nodiscard]] constexpr BlockMetadata blockMetadata(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::Grass:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 0, .bottom = 2, .side = 1},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::Dirt:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 2, .bottom = 2, .side = 2},
            .hardness = 0.5f,
            .breakable = true,
        };
    case BlockType::Stone:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 3, .bottom = 3, .side = 3},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::Deepslate:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 4, .bottom = 4, .side = 4},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::CoalOre:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 5, .bottom = 5, .side = 5},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::Sand:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 7, .bottom = 7, .side = 7},
            .hardness = 0.5f,
            .breakable = true,
        };
    case BlockType::Bedrock:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 8, .bottom = 8, .side = 8},
            .hardness = 999.0f,
            .breakable = false,
        };
    case BlockType::Water:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 6, .bottom = 6, .side = 6},
            .hardness = 1000.0f,
            .breakable = false,
        };
    case BlockType::IronOre:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 9, .bottom = 9, .side = 9},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::GoldOre:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 10, .bottom = 10, .side = 10},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::DiamondOre:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 11, .bottom = 11, .side = 11},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::EmeraldOre:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 12, .bottom = 12, .side = 12},
            .hardness = 3.0f,
            .breakable = true,
        };
    case BlockType::Lava:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 13, .bottom = 13, .side = 13},
            .hardness = 1000.0f,
            .breakable = false,
        };
    case BlockType::TreeTrunk:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 14, .bottom = 14, .side = 15},
            .hardness = 1.6f,
            .breakable = true,
        };
    case BlockType::TreeCrown:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 16, .bottom = 16, .side = 16},
            .hardness = 0.2f,
            .breakable = true,
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
