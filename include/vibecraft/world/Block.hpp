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
    DarkOakLeaves,
    SculkBlock,
    DripstoneBlock,
    FurnaceNorth,
    FurnaceEast,
    FurnaceWest,
    Ladder,
    OakStairs,
    OakStairsNorth,
    OakStairsEast,
    OakStairsSouth,
    OakStairsWest,
    CobblestoneStairs,
    CobblestoneStairsNorth,
    CobblestoneStairsEast,
    CobblestoneStairsSouth,
    CobblestoneStairsWest,
    StoneStairs,
    StoneStairsNorth,
    StoneStairsEast,
    StoneStairsSouth,
    StoneStairsWest,
    BrickStairs,
    BrickStairsNorth,
    BrickStairsEast,
    BrickStairsSouth,
    BrickStairsWest,
    SandstoneStairs,
    SandstoneStairsNorth,
    SandstoneStairsEast,
    SandstoneStairsSouth,
    SandstoneStairsWest,
    JungleStairs,
    JungleStairsNorth,
    JungleStairsEast,
    JungleStairsSouth,
    JungleStairsWest,
    TorchNorth,
    TorchEast,
    TorchSouth,
    TorchWest,
    OakDoor,
    JungleDoor,
    IronDoor,
    OakDoorLowerNorth,
    OakDoorLowerEast,
    OakDoorLowerSouth,
    OakDoorLowerWest,
    OakDoorLowerNorthOpen,
    OakDoorLowerEastOpen,
    OakDoorLowerSouthOpen,
    OakDoorLowerWestOpen,
    OakDoorUpperNorth,
    OakDoorUpperEast,
    OakDoorUpperSouth,
    OakDoorUpperWest,
    OakDoorUpperNorthOpen,
    OakDoorUpperEastOpen,
    OakDoorUpperSouthOpen,
    OakDoorUpperWestOpen,
    JungleDoorLowerNorth,
    JungleDoorLowerEast,
    JungleDoorLowerSouth,
    JungleDoorLowerWest,
    JungleDoorLowerNorthOpen,
    JungleDoorLowerEastOpen,
    JungleDoorLowerSouthOpen,
    JungleDoorLowerWestOpen,
    JungleDoorUpperNorth,
    JungleDoorUpperEast,
    JungleDoorUpperSouth,
    JungleDoorUpperWest,
    JungleDoorUpperNorthOpen,
    JungleDoorUpperEastOpen,
    JungleDoorUpperSouthOpen,
    JungleDoorUpperWestOpen,
    IronDoorLowerNorth,
    IronDoorLowerEast,
    IronDoorLowerSouth,
    IronDoorLowerWest,
    IronDoorLowerNorthOpen,
    IronDoorLowerEastOpen,
    IronDoorLowerSouthOpen,
    IronDoorLowerWestOpen,
    IronDoorUpperNorth,
    IronDoorUpperEast,
    IronDoorUpperSouth,
    IronDoorUpperWest,
    IronDoorUpperNorthOpen,
    IronDoorUpperEastOpen,
    IronDoorUpperSouthOpen,
    IronDoorUpperWestOpen,
    LargeFernBottom,
    LargeFernTop
};

enum class DoorFamily : std::uint8_t
{
    None = 0,
    Oak,
    Jungle,
    Iron
};

enum class DoorFacing : std::uint8_t
{
    North = 0,
    East,
    South,
    West
};

struct BlockCollisionBox
{
    float minX = 0.0f;
    float maxX = 1.0f;
    float minY = 0.0f;
    float maxY = 1.0f;
    float minZ = 0.0f;
    float maxZ = 1.0f;
};

[[nodiscard]] constexpr bool isDoorItemBlock(const BlockType blockType)
{
    return blockType == BlockType::OakDoor || blockType == BlockType::JungleDoor
        || blockType == BlockType::IronDoor;
}

[[nodiscard]] constexpr bool isDoorVariantBlock(const BlockType blockType)
{
    return blockType >= BlockType::OakDoorLowerNorth
        && blockType <= BlockType::IronDoorUpperWestOpen;
}

[[nodiscard]] constexpr DoorFamily doorFamilyForBlockType(const BlockType blockType)
{
    if (blockType == BlockType::OakDoor
        || (blockType >= BlockType::OakDoorLowerNorth
            && blockType <= BlockType::OakDoorUpperWestOpen))
    {
        return DoorFamily::Oak;
    }
    if (blockType == BlockType::JungleDoor
        || (blockType >= BlockType::JungleDoorLowerNorth
            && blockType <= BlockType::JungleDoorUpperWestOpen))
    {
        return DoorFamily::Jungle;
    }
    if (blockType == BlockType::IronDoor
        || (blockType >= BlockType::IronDoorLowerNorth
            && blockType <= BlockType::IronDoorUpperWestOpen))
    {
        return DoorFamily::Iron;
    }
    return DoorFamily::None;
}

[[nodiscard]] constexpr bool isDoorBlock(const BlockType blockType)
{
    return doorFamilyForBlockType(blockType) != DoorFamily::None;
}

[[nodiscard]] constexpr bool isWoodDoorBlock(const BlockType blockType)
{
    const DoorFamily family = doorFamilyForBlockType(blockType);
    return family == DoorFamily::Oak || family == DoorFamily::Jungle;
}

[[nodiscard]] constexpr bool isMetalDoorBlock(const BlockType blockType)
{
    return doorFamilyForBlockType(blockType) == DoorFamily::Iron;
}

[[nodiscard]] constexpr BlockType baseDoorBlockType(const DoorFamily family)
{
    switch (family)
    {
    case DoorFamily::Oak:
        return BlockType::OakDoor;
    case DoorFamily::Jungle:
        return BlockType::JungleDoor;
    case DoorFamily::Iron:
        return BlockType::IronDoor;
    case DoorFamily::None:
    default:
        return BlockType::Air;
    }
}

[[nodiscard]] constexpr BlockType firstDoorVariantBlockType(const DoorFamily family)
{
    switch (family)
    {
    case DoorFamily::Oak:
        return BlockType::OakDoorLowerNorth;
    case DoorFamily::Jungle:
        return BlockType::JungleDoorLowerNorth;
    case DoorFamily::Iron:
        return BlockType::IronDoorLowerNorth;
    case DoorFamily::None:
    default:
        return BlockType::Air;
    }
}

[[nodiscard]] constexpr DoorFacing doorFacingForBlockType(const BlockType blockType)
{
    if (!isDoorVariantBlock(blockType))
    {
        return DoorFacing::South;
    }

    const std::uint8_t offset = static_cast<std::uint8_t>(blockType)
        - static_cast<std::uint8_t>(firstDoorVariantBlockType(doorFamilyForBlockType(blockType)));
    return static_cast<DoorFacing>(offset % 4U);
}

[[nodiscard]] constexpr bool doorUsesXAxisPlane(const DoorFacing facing)
{
    return facing == DoorFacing::East || facing == DoorFacing::West;
}

[[nodiscard]] constexpr DoorFacing oppositeDoorFacingWithinAxis(const DoorFacing facing)
{
    switch (facing)
    {
    case DoorFacing::North:
        return DoorFacing::South;
    case DoorFacing::South:
        return DoorFacing::North;
    case DoorFacing::East:
        return DoorFacing::West;
    case DoorFacing::West:
    default:
        return DoorFacing::East;
    }
}

[[nodiscard]] constexpr bool isDoorUpperHalf(const BlockType blockType)
{
    if (!isDoorVariantBlock(blockType))
    {
        return false;
    }

    const std::uint8_t offset = static_cast<std::uint8_t>(blockType)
        - static_cast<std::uint8_t>(firstDoorVariantBlockType(doorFamilyForBlockType(blockType)));
    return offset >= 8U;
}

[[nodiscard]] constexpr bool isDoorOpen(const BlockType blockType)
{
    if (!isDoorVariantBlock(blockType))
    {
        return false;
    }

    const std::uint8_t offset = static_cast<std::uint8_t>(blockType)
        - static_cast<std::uint8_t>(firstDoorVariantBlockType(doorFamilyForBlockType(blockType)));
    return (offset % 8U) >= 4U;
}

[[nodiscard]] constexpr BlockType doorVariantBlockType(
    const DoorFamily family,
    const DoorFacing facing,
    const bool open,
    const bool upperHalf)
{
    if (family == DoorFamily::None)
    {
        return BlockType::Air;
    }

    return static_cast<BlockType>(
        static_cast<std::uint8_t>(firstDoorVariantBlockType(family))
        + (upperHalf ? 8U : 0U)
        + (open ? 4U : 0U)
        + static_cast<std::uint8_t>(facing));
}

[[nodiscard]] constexpr BlockType placedDoorLowerBlockType(
    const BlockType doorItemBlock,
    const DoorFacing facing)
{
    return doorVariantBlockType(doorFamilyForBlockType(doorItemBlock), facing, false, false);
}

[[nodiscard]] constexpr BlockType placedDoorUpperBlockType(
    const BlockType doorItemBlock,
    const DoorFacing facing)
{
    return doorVariantBlockType(doorFamilyForBlockType(doorItemBlock), facing, false, true);
}

[[nodiscard]] constexpr BlockType normalizeDoorBlockType(const BlockType blockType)
{
    return isDoorBlock(blockType) ? baseDoorBlockType(doorFamilyForBlockType(blockType))
                                  : blockType;
}

[[nodiscard]] constexpr BlockType toggleDoorBlockType(const BlockType blockType)
{
    if (!isDoorVariantBlock(blockType))
    {
        return blockType;
    }

    return doorVariantBlockType(
        doorFamilyForBlockType(blockType),
        doorFacingForBlockType(blockType),
        !isDoorOpen(blockType),
        isDoorUpperHalf(blockType));
}

[[nodiscard]] constexpr BlockType pairedDoorBlockType(const BlockType blockType)
{
    if (!isDoorVariantBlock(blockType))
    {
        return blockType;
    }

    return doorVariantBlockType(
        doorFamilyForBlockType(blockType),
        doorFacingForBlockType(blockType),
        isDoorOpen(blockType),
        !isDoorUpperHalf(blockType));
}

[[nodiscard]] constexpr bool hasCustomCollisionBox(const BlockType blockType)
{
    return isDoorVariantBlock(blockType);
}

[[nodiscard]] constexpr BlockCollisionBox collisionBoxForBlockType(const BlockType blockType)
{
    constexpr float thickness = 3.0f / 16.0f;
    constexpr float minThin = (1.0f - thickness) * 0.5f;
    constexpr float maxThin = minThin + thickness;

    if (isDoorVariantBlock(blockType))
    {
        const DoorFacing facing = doorFacingForBlockType(blockType);
        const bool closedAlongX = doorUsesXAxisPlane(facing);
        const bool alongX = isDoorOpen(blockType) ? !closedAlongX : closedAlongX;
        return alongX ? BlockCollisionBox{.minX = minThin, .maxX = maxThin}
                      : BlockCollisionBox{.minZ = minThin, .maxZ = maxThin};
    }

    return {};
}

[[nodiscard]] constexpr bool isFurnaceBlock(const BlockType blockType)
{
    return blockType == BlockType::Furnace || blockType == BlockType::FurnaceNorth
        || blockType == BlockType::FurnaceEast || blockType == BlockType::FurnaceWest;
}

[[nodiscard]] constexpr BlockType normalizeFurnaceBlockType(const BlockType blockType)
{
    return isFurnaceBlock(blockType) ? BlockType::Furnace : blockType;
}

[[nodiscard]] constexpr bool isStairBlock(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::OakStairs:
    case BlockType::OakStairsNorth:
    case BlockType::OakStairsEast:
    case BlockType::OakStairsSouth:
    case BlockType::OakStairsWest:
    case BlockType::CobblestoneStairs:
    case BlockType::CobblestoneStairsNorth:
    case BlockType::CobblestoneStairsEast:
    case BlockType::CobblestoneStairsSouth:
    case BlockType::CobblestoneStairsWest:
    case BlockType::StoneStairs:
    case BlockType::StoneStairsNorth:
    case BlockType::StoneStairsEast:
    case BlockType::StoneStairsSouth:
    case BlockType::StoneStairsWest:
    case BlockType::BrickStairs:
    case BlockType::BrickStairsNorth:
    case BlockType::BrickStairsEast:
    case BlockType::BrickStairsSouth:
    case BlockType::BrickStairsWest:
    case BlockType::SandstoneStairs:
    case BlockType::SandstoneStairsNorth:
    case BlockType::SandstoneStairsEast:
    case BlockType::SandstoneStairsSouth:
    case BlockType::SandstoneStairsWest:
    case BlockType::JungleStairs:
    case BlockType::JungleStairsNorth:
    case BlockType::JungleStairsEast:
    case BlockType::JungleStairsSouth:
    case BlockType::JungleStairsWest:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool isTorchBlock(const BlockType blockType)
{
    return blockType == BlockType::Torch || blockType == BlockType::TorchNorth
        || blockType == BlockType::TorchEast || blockType == BlockType::TorchSouth
        || blockType == BlockType::TorchWest;
}

[[nodiscard]] constexpr BlockType normalizeTorchBlockType(const BlockType blockType)
{
    return isTorchBlock(blockType) ? BlockType::Torch : blockType;
}

[[nodiscard]] constexpr BlockType normalizeStairBlockType(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::OakStairsNorth:
    case BlockType::OakStairsEast:
    case BlockType::OakStairsSouth:
    case BlockType::OakStairsWest:
        return BlockType::OakStairs;
    case BlockType::CobblestoneStairsNorth:
    case BlockType::CobblestoneStairsEast:
    case BlockType::CobblestoneStairsSouth:
    case BlockType::CobblestoneStairsWest:
        return BlockType::CobblestoneStairs;
    case BlockType::StoneStairsNorth:
    case BlockType::StoneStairsEast:
    case BlockType::StoneStairsSouth:
    case BlockType::StoneStairsWest:
        return BlockType::StoneStairs;
    case BlockType::BrickStairsNorth:
    case BlockType::BrickStairsEast:
    case BlockType::BrickStairsSouth:
    case BlockType::BrickStairsWest:
        return BlockType::BrickStairs;
    case BlockType::SandstoneStairsNorth:
    case BlockType::SandstoneStairsEast:
    case BlockType::SandstoneStairsSouth:
    case BlockType::SandstoneStairsWest:
        return BlockType::SandstoneStairs;
    case BlockType::JungleStairsNorth:
    case BlockType::JungleStairsEast:
    case BlockType::JungleStairsSouth:
    case BlockType::JungleStairsWest:
        return BlockType::JungleStairs;
    default:
        return blockType;
    }
}

[[nodiscard]] constexpr BlockType normalizePlaceVariantBlockType(const BlockType blockType)
{
    return normalizeDoorBlockType(
        normalizeTorchBlockType(normalizeStairBlockType(normalizeFurnaceBlockType(blockType))));
}

[[nodiscard]] constexpr bool isLogBlock(const BlockType blockType)
{
    return blockType == BlockType::OakLog || blockType == BlockType::JungleLog
        || blockType == BlockType::SpruceLog || blockType == BlockType::BirchLog
        || blockType == BlockType::DarkOakLog;
}

[[nodiscard]] constexpr bool isLeafBlock(const BlockType blockType)
{
    return blockType == BlockType::OakLeaves || blockType == BlockType::JungleLeaves
        || blockType == BlockType::SpruceLeaves || blockType == BlockType::BirchLeaves
        || blockType == BlockType::DarkOakLeaves;
}

[[nodiscard]] constexpr BlockType logBlockForLeaf(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::OakLeaves:
        return BlockType::OakLog;
    case BlockType::JungleLeaves:
        return BlockType::JungleLog;
    case BlockType::SpruceLeaves:
        return BlockType::SpruceLog;
    case BlockType::BirchLeaves:
        return BlockType::BirchLog;
    case BlockType::DarkOakLeaves:
        return BlockType::DarkOakLog;
    default:
        return BlockType::Air;
    }
}

[[nodiscard]] constexpr BlockType leafBlockForLog(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::OakLog:
        return BlockType::OakLeaves;
    case BlockType::JungleLog:
        return BlockType::JungleLeaves;
    case BlockType::SpruceLog:
        return BlockType::SpruceLeaves;
    case BlockType::BirchLog:
        return BlockType::BirchLeaves;
    case BlockType::DarkOakLog:
        return BlockType::DarkOakLeaves;
    default:
        return BlockType::Air;
    }
}

[[nodiscard]] constexpr bool isFluid(const BlockType blockType)
{
    return blockType == BlockType::Water || blockType == BlockType::Lava;
}

[[nodiscard]] constexpr bool isGravityBlock(const BlockType blockType)
{
    return blockType == BlockType::Sand || blockType == BlockType::Gravel;
}

[[nodiscard]] constexpr bool isSolid(const BlockType blockType)
{
    return blockType != BlockType::Air && !isFluid(blockType) && !isTorchBlock(blockType)
        && blockType != BlockType::Dandelion
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
        && blockType != BlockType::LargeFernBottom && blockType != BlockType::LargeFernTop
        && blockType != BlockType::CocoaPod && blockType != BlockType::Bamboo
        && blockType != BlockType::Ladder
        && !isDoorBlock(blockType);
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
        && !isTorchBlock(blockType) && blockType != BlockType::Glass
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
        && blockType != BlockType::LargeFernBottom && blockType != BlockType::LargeFernTop
        && blockType != BlockType::Dandelion && blockType != BlockType::Poppy
        && blockType != BlockType::BlueOrchid && blockType != BlockType::Allium
        && blockType != BlockType::OxeyeDaisy && blockType != BlockType::BrownMushroom
        && blockType != BlockType::RedMushroom && blockType != BlockType::DeadBush
        && blockType != BlockType::Vines && blockType != BlockType::CocoaPod
        && blockType != BlockType::Bamboo
        && blockType != BlockType::Ladder
        && !isStairBlock(blockType)
        && !isDoorBlock(blockType);
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
    case BlockType::SculkBlock:
    case BlockType::DripstoneBlock:
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
        || blockType == BlockType::SproutTuft || blockType == BlockType::Fern
        || blockType == BlockType::LargeFernBottom || blockType == BlockType::LargeFernTop;
}
}  // namespace vibecraft::world
