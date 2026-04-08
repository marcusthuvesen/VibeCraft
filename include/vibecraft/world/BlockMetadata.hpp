#pragma once

#include <cstdint>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world
{
enum class BlockFace : std::uint8_t
{
    Top = 0,
    Bottom,
    Side,
    East,
    West,
    South,
    North
};

constexpr std::uint8_t kUseSharedSideTile = 0xff;

struct BlockTextureTiles
{
    std::uint8_t top = 0;
    std::uint8_t bottom = 0;
    std::uint8_t side = 0;
    std::uint8_t east = kUseSharedSideTile;
    std::uint8_t west = kUseSharedSideTile;
    std::uint8_t south = kUseSharedSideTile;
    std::uint8_t north = kUseSharedSideTile;
};

struct BlockMetadata
{
    std::uint32_t debugColor = 0xffffffff;
    BlockTextureTiles textureTiles{};
    float hardness = 0.0f;
    bool breakable = false;
};

// Vertex color multiplies the atlas sample; white shows materials as authored in the source textures.
// Tile indices follow scripts/build_chunk_atlas.sh row-major order on the chunk atlas grid.
// Standard tiles are interpreted with Minecraft-style material semantics.
[[nodiscard]] constexpr BlockMetadata blockMetadata(const BlockType blockType)
{
    if (const DoorFamily doorFamily = doorFamilyForBlockType(blockType);
        doorFamily != DoorFamily::None)
    {
        std::uint8_t tileIndex = 108;
        float hardness = 3.0f;
        switch (doorFamily)
        {
        case DoorFamily::Oak:
            tileIndex = isDoorUpperHalf(blockType) ? 109 : 108;
            hardness = 3.0f;
            break;
        case DoorFamily::Jungle:
            tileIndex = isDoorUpperHalf(blockType) ? 111 : 110;
            hardness = 3.0f;
            break;
        case DoorFamily::Iron:
            tileIndex = isDoorUpperHalf(blockType) ? 113 : 112;
            hardness = 5.0f;
            break;
        case DoorFamily::None:
        default:
            break;
        }

        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = tileIndex, .bottom = tileIndex, .side = tileIndex},
            .hardness = hardness,
            .breakable = true,
        };
    }

    switch (blockType)
    {
    case BlockType::Grass:
        // Grass block: standard grassy top with dirt fringe on the side, dirt on the bottom.
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
            .textureTiles = {.top = 88, .bottom = 88, .side = 4},
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
        // Tile 6 = first frame of the still-water strip (see build_chunk_atlas.sh).
        return {
            // Stronger blue tint matches the classic Minecraft-style surface better.
            .debugColor = 0xc8d2874a,
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
    case BlockType::OakLog:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 14, .bottom = 14, .side = 15},
            .hardness = 1.6f,
            .breakable = true,
        };
    case BlockType::OakLeaves:
        return {
            .debugColor = 0xff70bf7d,
            .textureTiles = {.top = 16, .bottom = 16, .side = 16},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::BirchLog:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 92, .bottom = 92, .side = 93},
            .hardness = 1.6f,
            .breakable = true,
        };
    case BlockType::BirchLeaves:
        return {
            .debugColor = 0xff7ece9a,
            .textureTiles = {.top = 94, .bottom = 94, .side = 94},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::DarkOakLog:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 99, .bottom = 99, .side = 100},
            .hardness = 1.7f,
            .breakable = true,
        };
    case BlockType::DarkOakLeaves:
        return {
            .debugColor = 0xff466e58,
            .textureTiles = {.top = 101, .bottom = 101, .side = 101},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::JungleLog:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 34, .bottom = 34, .side = 35},
            .hardness = 1.7f,
            .breakable = true,
        };
    case BlockType::JungleLeaves:
        return {
            .debugColor = 0xff5cae72,
            .textureTiles = {.top = 60, .bottom = 60, .side = 60},
            .hardness = 0.2f,
            .breakable = true,
        };
    case BlockType::SpruceLog:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 61, .bottom = 61, .side = 62},
            .hardness = 1.7f,
            .breakable = true,
        };
    case BlockType::SpruceLeaves:
        return {
            .debugColor = 0xff4c875f,
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
            .textureTiles = {.top = 20, .bottom = 23, .side = 21, .south = 22},
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
            .textureTiles = {.top = 89, .bottom = 90, .side = 19},
            .hardness = 0.8f,
            .breakable = true,
        };
    case BlockType::Furnace:
        // Tiles 24-26 from build_chunk_atlas.sh row 5 (furnace_*); vanilla reuses the top texture on the bottom.
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 24, .bottom = 24, .side = 25, .south = 26},
            .hardness = 3.5f,
            .breakable = true,
        };
    case BlockType::FurnaceNorth:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 24, .bottom = 24, .side = 25, .north = 26},
            .hardness = 3.5f,
            .breakable = true,
        };
    case BlockType::FurnaceEast:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 24, .bottom = 24, .side = 25, .east = 26},
            .hardness = 3.5f,
            .breakable = true,
        };
    case BlockType::FurnaceWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 24, .bottom = 24, .side = 25, .west = 26},
            .hardness = 3.5f,
            .breakable = true,
        };
    case BlockType::Chest:
        // Tiles 27-29 from build_chunk_atlas.sh row 5 (chest_top/front/side).
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 27, .bottom = 27, .side = 29, .south = 28},
            .hardness = 2.5f,
            .breakable = true,
        };
    case BlockType::OxygenGenerator:
        // Dedicated atlas tiles 77–79 (relay bottom / side / top); avoids clobbering bricks, glowstone, moss.
        return {
            .debugColor = 0xffb6ffe1,
            .textureTiles = {.top = 79, .bottom = 77, .side = 78},
            .hardness = 2.5f,
            .breakable = true,
        };
    case BlockType::SnowGrass:
        // Snowy grass block: snowy top and frosted grass side over dirt.
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 30, .bottom = 2, .side = 31},
            .hardness = 0.65f,
            .breakable = true,
        };
    case BlockType::JungleGrass:
        // Mossy grass block: mossy top and overgrown grass side over dirt.
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 32, .bottom = 2, .side = 33},
            .hardness = 0.7f,
            .breakable = true,
        };
    case BlockType::Torch:
    case BlockType::TorchNorth:
    case BlockType::TorchEast:
    case BlockType::TorchSouth:
    case BlockType::TorchWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 36, .bottom = 36, .side = 36},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Ladder:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 107, .bottom = 107, .side = 107},
            .hardness = 0.4f,
            .breakable = true,
        };
    case BlockType::OakStairs:
    case BlockType::OakStairsNorth:
    case BlockType::OakStairsEast:
    case BlockType::OakStairsSouth:
    case BlockType::OakStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 17, .bottom = 17, .side = 17},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::JungleStairs:
    case BlockType::JungleStairsNorth:
    case BlockType::JungleStairsEast:
    case BlockType::JungleStairsSouth:
    case BlockType::JungleStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 65, .bottom = 65, .side = 65},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::CobblestoneStairs:
    case BlockType::CobblestoneStairsNorth:
    case BlockType::CobblestoneStairsEast:
    case BlockType::CobblestoneStairsSouth:
    case BlockType::CobblestoneStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 18, .bottom = 18, .side = 18},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::StoneStairs:
    case BlockType::StoneStairsNorth:
    case BlockType::StoneStairsEast:
    case BlockType::StoneStairsSouth:
    case BlockType::StoneStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 3, .bottom = 3, .side = 3},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::BrickStairs:
    case BlockType::BrickStairsNorth:
    case BlockType::BrickStairsEast:
    case BlockType::BrickStairsSouth:
    case BlockType::BrickStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 41, .bottom = 41, .side = 41},
            .hardness = 2.0f,
            .breakable = true,
        };
    case BlockType::SandstoneStairs:
    case BlockType::SandstoneStairsNorth:
    case BlockType::SandstoneStairsEast:
    case BlockType::SandstoneStairsSouth:
    case BlockType::SandstoneStairsWest:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 89, .bottom = 90, .side = 19},
            .hardness = 0.8f,
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
            .hardness = 0.9f,
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
            .debugColor = 0xffd0b4a0,
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
    case BlockType::Fern:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 95, .bottom = 95, .side = 95},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::Podzol:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 96, .bottom = 2, .side = 97},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::CoarseDirt:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 98, .bottom = 98, .side = 98},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::Vines:
        return {
            .debugColor = 0xffffffff,
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
            .textureTiles = {.top = 91, .bottom = 91, .side = 59},
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
    case BlockType::HabitatPanel:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 68, .bottom = 68, .side = 68},
            .hardness = 2.3f,
            .breakable = true,
        };
    case BlockType::HabitatFloor:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 69, .bottom = 69, .side = 69},
            .hardness = 2.1f,
            .breakable = true,
        };
    case BlockType::HabitatFrame:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 70, .bottom = 70, .side = 70},
            .hardness = 2.6f,
            .breakable = true,
        };
    case BlockType::AirlockPanel:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 75, .bottom = 75, .side = 75},
            .hardness = 2.8f,
            .breakable = true,
        };
    case BlockType::PowerConduit:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 76, .bottom = 76, .side = 76},
            .hardness = 1.4f,
            .breakable = true,
        };
    case BlockType::GreenhouseGlass:
        return {
            .debugColor = 0xbfe9fff5,
            .textureTiles = {.top = 71, .bottom = 71, .side = 71},
            .hardness = 0.4f,
            .breakable = true,
        };
    case BlockType::PlanterTray:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 72, .bottom = 72, .side = 72},
            .hardness = 1.8f,
            .breakable = true,
        };
    case BlockType::FiberSapling:
        return {
            .debugColor = 0xd8bff3d8,
            .textureTiles = {.top = 73, .bottom = 73, .side = 73},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::FiberSprout:
        return {
            .debugColor = 0xe0d8ffd8,
            .textureTiles = {.top = 74, .bottom = 74, .side = 74},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::GrassTuft:
        // Stylized grass tuft billboard for denser plains ground cover.
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 80, .bottom = 80, .side = 80},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::SparseTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 81, .bottom = 81, .side = 81},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::FlowerTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 82, .bottom = 82, .side = 82},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::DryTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 83, .bottom = 83, .side = 83},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::LushTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 84, .bottom = 84, .side = 84},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::FrostTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 85, .bottom = 85, .side = 85},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::CloverTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 86, .bottom = 86, .side = 86},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::SproutTuft:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 87, .bottom = 87, .side = 87},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::SculkBlock:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 104, .bottom = 104, .side = 104},
            .hardness = 0.6f,
            .breakable = true,
        };
    case BlockType::DripstoneBlock:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 105, .bottom = 105, .side = 105},
            .hardness = 1.5f,
            .breakable = true,
        };
    case BlockType::LargeFernBottom:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 120, .bottom = 120, .side = 120},
            .hardness = 0.0f,
            .breakable = true,
        };
    case BlockType::LargeFernTop:
        return {
            .debugColor = 0xffffffff,
            .textureTiles = {.top = 121, .bottom = 121, .side = 121},
            .hardness = 0.0f,
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
    case BlockFace::East:
        return tiles.east == kUseSharedSideTile ? tiles.side : tiles.east;
    case BlockFace::West:
        return tiles.west == kUseSharedSideTile ? tiles.side : tiles.west;
    case BlockFace::South:
        return tiles.south == kUseSharedSideTile ? tiles.side : tiles.south;
    case BlockFace::North:
        return tiles.north == kUseSharedSideTile ? tiles.side : tiles.north;
    case BlockFace::Side:
    default:
        return tiles.side;
    }
}
}  // namespace vibecraft::world
