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
    TreeCrown,
    OakPlanks,
    CraftingTable,
    Cobblestone,
    Sandstone,
    Oven,
    Chest,
    SnowGrass,
    JungleGrass,
    Torch,
    TNT,
    Glass,
    Bricks,
    Bookshelf,
    Glowstone,
    Obsidian,
    Gravel,
    Cactus,
    Dandelion,
    Poppy,
    BlueOrchid,
    Allium,
    OxeyeDaisy,
    BrownMushroom,
    RedMushroom,
    JungleTreeTrunk,
    JungleTreeCrown,
    SnowTreeTrunk,
    SnowTreeCrown,
    DeadBush,
    Vines,
    CocoaPod,
    Melon,
    Bamboo,
    JunglePlanks,
    MossBlock,
    MossyCobblestone
};

[[nodiscard]] constexpr bool isFluid(const BlockType blockType)
{
    return blockType == BlockType::Water || blockType == BlockType::Lava;
}

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && blockType != BlockType::TreeCrown
        && blockType != BlockType::JungleTreeCrown
        && blockType != BlockType::SnowTreeCrown
        && blockType != BlockType::Torch && blockType != BlockType::Dandelion
        && blockType != BlockType::Poppy && blockType != BlockType::BlueOrchid
        && blockType != BlockType::Allium && blockType != BlockType::OxeyeDaisy
        && blockType != BlockType::BrownMushroom && blockType != BlockType::RedMushroom
        && blockType != BlockType::DeadBush && blockType != BlockType::Vines
        && blockType != BlockType::CocoaPod && blockType != BlockType::Bamboo;
}

[[nodiscard]] constexpr bool isRenderable(const BlockType blockType)
{
    return blockType != BlockType::Air;
}

/// Blocks the player can directly target with the crosshair for mining/interaction.
/// Includes non-solid decorative blocks (e.g. leaves, torches), excludes fluids and air.
[[nodiscard]] constexpr bool isRaycastTarget(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType);
}

[[nodiscard]] constexpr bool occludesFaces(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && blockType != BlockType::TreeCrown
        && blockType != BlockType::JungleTreeCrown
        && blockType != BlockType::SnowTreeCrown
        && blockType != BlockType::Torch && blockType != BlockType::Glass
        && blockType != BlockType::Dandelion && blockType != BlockType::Poppy
        && blockType != BlockType::BlueOrchid && blockType != BlockType::Allium
        && blockType != BlockType::OxeyeDaisy && blockType != BlockType::BrownMushroom
        && blockType != BlockType::RedMushroom && blockType != BlockType::DeadBush
        && blockType != BlockType::Vines && blockType != BlockType::CocoaPod
        && blockType != BlockType::Bamboo;
}

/// Blocks terrain generation may place directly into the base world columns.
[[nodiscard]] constexpr bool isNaturalTerrainBlock(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::Air:
    case BlockType::Grass:
    case BlockType::Dirt:
    case BlockType::Stone:
    case BlockType::Deepslate:
    case BlockType::CoalOre:
    case BlockType::Sand:
    case BlockType::Bedrock:
    case BlockType::Water:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Lava:
    case BlockType::TreeTrunk:
    case BlockType::TreeCrown:
    case BlockType::JungleTreeTrunk:
    case BlockType::JungleTreeCrown:
    case BlockType::SnowTreeTrunk:
    case BlockType::SnowTreeCrown:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Sandstone:
        return true;
    default:
        return false;
    }
}

/// Blocks that can appear from biome flora decoration (after base terrain generation).
[[nodiscard]] constexpr bool isNaturalDecorationBlock(const BlockType blockType)
{
    return blockType == BlockType::Cactus || blockType == BlockType::Dandelion
        || blockType == BlockType::Poppy || blockType == BlockType::BlueOrchid
        || blockType == BlockType::Allium || blockType == BlockType::OxeyeDaisy
        || blockType == BlockType::BrownMushroom || blockType == BlockType::RedMushroom
        || blockType == BlockType::DeadBush || blockType == BlockType::Vines
        || blockType == BlockType::CocoaPod || blockType == BlockType::Melon
        || blockType == BlockType::Bamboo;
}
}  // namespace vibecraft::world
