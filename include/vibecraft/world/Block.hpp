#pragma once

#include <cstdint>

namespace vibecraft::world
{
// Keep enum values stable for save files and multiplayer payloads.
// The content pivot reinterprets these legacy ids through labels, recipes, and world generation.
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
    OakLog,
    OakLeaves,
    OakPlanks,
    CraftingTable,
    Cobblestone,
    Sandstone,
    Furnace,
    Chest,
    OxygenGenerator,
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
    JungleLog,
    JungleLeaves,
    SpruceLog,
    SpruceLeaves,
    DeadBush,
    Vines,
    CocoaPod,
    Melon,
    Bamboo,
    JunglePlanks,
    MossBlock,
    MossyCobblestone,
    HabitatPanel,
    HabitatFloor,
    HabitatFrame,
    AirlockPanel,
    PowerConduit,
    GreenhouseGlass,
    PlanterTray,
    FiberSapling,
    FiberSprout,
    GrassTuft,
    FlowerTuft,
    DryTuft,
    LushTuft,
    FrostTuft,
    SparseTuft,
    CloverTuft,
    SproutTuft,
    BirchLog,
    BirchLeaves,
    Fern,
    Podzol,
    CoarseDirt,
    DarkOakLog,
    DarkOakLeaves
};

[[nodiscard]] constexpr bool isFluid(const BlockType blockType)
{
    return blockType == BlockType::Water || blockType == BlockType::Lava;
}

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && blockType != BlockType::OakLeaves
        && blockType != BlockType::JungleLeaves
        && blockType != BlockType::SpruceLeaves
        && blockType != BlockType::BirchLeaves
        && blockType != BlockType::DarkOakLeaves
        && blockType != BlockType::Torch && blockType != BlockType::Dandelion
        && blockType != BlockType::Poppy && blockType != BlockType::BlueOrchid
        && blockType != BlockType::Allium && blockType != BlockType::OxeyeDaisy
        && blockType != BlockType::BrownMushroom && blockType != BlockType::RedMushroom
        && blockType != BlockType::DeadBush && blockType != BlockType::Vines
        && blockType != BlockType::FiberSapling
        && blockType != BlockType::FiberSprout
        && blockType != BlockType::GrassTuft
        && blockType != BlockType::FlowerTuft
        && blockType != BlockType::DryTuft
        && blockType != BlockType::LushTuft
        && blockType != BlockType::FrostTuft
        && blockType != BlockType::SparseTuft
        && blockType != BlockType::CloverTuft
        && blockType != BlockType::SproutTuft
        && blockType != BlockType::Fern
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
    return blockType != BlockType::Air && !isFluid(blockType) && blockType != BlockType::OakLeaves
        && blockType != BlockType::JungleLeaves
        && blockType != BlockType::SpruceLeaves
        && blockType != BlockType::BirchLeaves
        && blockType != BlockType::DarkOakLeaves
        && blockType != BlockType::Torch && blockType != BlockType::Glass
        && blockType != BlockType::GreenhouseGlass
        && blockType != BlockType::FiberSapling
        && blockType != BlockType::FiberSprout
        && blockType != BlockType::GrassTuft
        && blockType != BlockType::FlowerTuft
        && blockType != BlockType::DryTuft
        && blockType != BlockType::LushTuft
        && blockType != BlockType::FrostTuft
        && blockType != BlockType::SparseTuft
        && blockType != BlockType::CloverTuft
        && blockType != BlockType::SproutTuft
        && blockType != BlockType::Fern
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
    case BlockType::OakLog:
    case BlockType::OakLeaves:
    case BlockType::BirchLog:
    case BlockType::BirchLeaves:
    case BlockType::Podzol:
    case BlockType::CoarseDirt:
    case BlockType::JungleLog:
    case BlockType::JungleLeaves:
    case BlockType::SpruceLog:
    case BlockType::SpruceLeaves:
    case BlockType::DarkOakLog:
    case BlockType::DarkOakLeaves:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Sandstone:
    case BlockType::Gravel:
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
        || blockType == BlockType::Bamboo || blockType == BlockType::GrassTuft
        || blockType == BlockType::FlowerTuft || blockType == BlockType::DryTuft
        || blockType == BlockType::LushTuft || blockType == BlockType::FrostTuft
        || blockType == BlockType::SparseTuft || blockType == BlockType::CloverTuft
        || blockType == BlockType::SproutTuft || blockType == BlockType::Fern;
}
}  // namespace vibecraft::world
