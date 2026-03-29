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
// Tile indices follow scripts/build_chunk_atlas.sh row-major order on the chunk atlas grid.
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
        // Tile 6 = first frame of assets/textures/materials/water_still.png (see build_chunk_atlas.sh).
        return {
            .debugColor = 0xb8ffcf7a,
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
            // Apply a temperate foliage tint (ABGR) so grayscale leaf textures render green.
            .debugColor = 0xc84eb978,
            .textureTiles = {.top = 16, .bottom = 16, .side = 16},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::JungleTreeTrunk:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 34, .bottom = 34, .side = 35},
            .hardness = 1.7f,
            .breakable = true,
        };
    case BlockType::JungleTreeCrown:
        return {
            // Lusher jungle foliage tint (ABGR), closer to Minecraft jungle biome look.
            .debugColor = 0xc85aac38,
            .textureTiles = {.top = 60, .bottom = 60, .side = 60},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::SnowTreeTrunk:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 61, .bottom = 61, .side = 62},
            .hardness = 1.7f,
            .breakable = true,
        };
    case BlockType::SnowTreeCrown:
        return {
            // Cooler spruce-like tint for snowy canopies (ABGR).
            .debugColor = 0xc8548461,
            .textureTiles = {.top = 63, .bottom = 63, .side = 63},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::OakPlanks:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 17, .bottom = 17, .side = 17},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::CraftingTable:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 20, .bottom = 17, .side = 21},
            .hardness = 2.5f,
            .breakable = true,
        };
    case BlockType::Cobblestone:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 18, .bottom = 18, .side = 18},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::Sandstone:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 19, .bottom = 19, .side = 19},
            .hardness = 0.8f,
            .breakable = true,
        };
    case BlockType::Oven:
        // Tiles 24–26 from build_chunk_atlas.sh row 5 (furnace_*); bottom reuses cobblestone (18).
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 24, .bottom = 18, .side = 25},
            .hardness = 3.5f,
            .breakable = true,
        };
    case BlockType::Chest:
        // Tiles 27–29 from build_chunk_atlas.sh row 5 (chest_*).
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 27, .bottom = 27, .side = 28},
            .hardness = 2.5f,
            .breakable = true,
        };
    case BlockType::SnowGrass:
        // Tiles 30–31 from build_chunk_atlas.sh row 6 (powder_snow + grass_block_snow); bottom uses dirt (2).
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 30, .bottom = 2, .side = 31},
            .hardness = 0.65f,
            .breakable = true,
        };
    case BlockType::JungleGrass:
        // Tiles 32–33 from build_chunk_atlas.sh row 6 (lush jungle-tinted grass top/side); bottom uses dirt (2).
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 32, .bottom = 2, .side = 33},
            .hardness = 0.7f,
            .breakable = true,
        };
    case BlockType::Torch:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 36, .bottom = 36, .side = 36},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::TNT:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 37, .bottom = 38, .side = 39},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Glass:
        return {
            .debugColor = 0xccffffff,
            .textureTiles = {.top = 40, .bottom = 40, .side = 40},
            .hardness = 0.3f,
            .breakable = true,
        };
    case BlockType::Bricks:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 41, .bottom = 41, .side = 41},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::Bookshelf:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 17, .bottom = 17, .side = 42},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::Glowstone:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 43, .bottom = 43, .side = 43},
            .hardness = 0.3f,
            .breakable = true,
        };
    case BlockType::Obsidian:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 44, .bottom = 44, .side = 44},
            .hardness = 50.0f,
            .breakable = true,
        };
    case BlockType::Gravel:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 45, .bottom = 45, .side = 45},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::Cactus:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 46, .bottom = 47, .side = 48},
            .hardness = 0.4f,
            .breakable = true,
        };
    case BlockType::Dandelion:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 49, .bottom = 49, .side = 49},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Poppy:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 50, .bottom = 50, .side = 50},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::BlueOrchid:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 51, .bottom = 51, .side = 51},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Allium:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 52, .bottom = 52, .side = 52},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::OxeyeDaisy:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 53, .bottom = 53, .side = 53},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::BrownMushroom:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 54, .bottom = 54, .side = 54},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::RedMushroom:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 55, .bottom = 55, .side = 55},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::DeadBush:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 56, .bottom = 56, .side = 56},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Vines:
        return {
            // Match jungle canopy tint so hanging vines read as lush green instead of gray.
            .debugColor = 0xc85aac38,
            .textureTiles = {.top = 57, .bottom = 57, .side = 57},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::CocoaPod:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 58, .bottom = 58, .side = 58},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::Melon:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 59, .bottom = 59, .side = 59},
            .hardness = 1.0f,
            .breakable = true,
        };
    case BlockType::Bamboo:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 64, .bottom = 64, .side = 64},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::JunglePlanks:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 65, .bottom = 65, .side = 65},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::MossBlock:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 66, .bottom = 66, .side = 66},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::MossyCobblestone:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 67, .bottom = 67, .side = 67},
            .hardness = 2.0f,
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
