#include "vibecraft/app/Mining.hpp"

#include <algorithm>
#include <limits>

#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::app
{
namespace
{
enum class MiningToolKind : std::uint8_t
{
    None = 0,
    Pickaxe,
    Axe,
    Sword,
};

struct MiningToolProfile
{
    MiningToolKind kind = MiningToolKind::None;
    float effectiveSpeed = 1.0f;
};

[[nodiscard]] bool isEarthyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::Grass || blockType == world::BlockType::Dirt
        || blockType == world::BlockType::Sand || blockType == world::BlockType::SnowGrass
        || blockType == world::BlockType::JungleGrass || blockType == world::BlockType::Gravel
        || blockType == world::BlockType::MossBlock
        || blockType == world::BlockType::Cactus || blockType == world::BlockType::Dandelion
        || blockType == world::BlockType::Poppy || blockType == world::BlockType::BlueOrchid
        || blockType == world::BlockType::Allium || blockType == world::BlockType::OxeyeDaisy
        || blockType == world::BlockType::BrownMushroom || blockType == world::BlockType::RedMushroom
        || blockType == world::BlockType::DeadBush || blockType == world::BlockType::Vines
        || blockType == world::BlockType::CocoaPod || blockType == world::BlockType::Melon
        || blockType == world::BlockType::Bamboo || blockType == world::BlockType::GrassTuft
        || blockType == world::BlockType::FlowerTuft || blockType == world::BlockType::DryTuft
        || blockType == world::BlockType::LushTuft || blockType == world::BlockType::FrostTuft
        || blockType == world::BlockType::SparseTuft || blockType == world::BlockType::CloverTuft
        || blockType == world::BlockType::SproutTuft;
}

[[nodiscard]] bool isStoneOrOreBlockType(const world::BlockType blockType)
{
    if (world::isMetalDoorBlock(blockType))
    {
        return true;
    }

    switch (blockType)
    {
    case world::BlockType::Stone:
    case world::BlockType::Deepslate:
    case world::BlockType::CoalOre:
    case world::BlockType::IronOre:
    case world::BlockType::GoldOre:
    case world::BlockType::DiamondOre:
    case world::BlockType::EmeraldOre:
    case world::BlockType::Cobblestone:
    case world::BlockType::CobblestoneStairs:
    case world::BlockType::CobblestoneStairsNorth:
    case world::BlockType::CobblestoneStairsEast:
    case world::BlockType::CobblestoneStairsSouth:
    case world::BlockType::CobblestoneStairsWest:
    case world::BlockType::MossyCobblestone:
    case world::BlockType::Sandstone:
    case world::BlockType::SandstoneStairs:
    case world::BlockType::SandstoneStairsNorth:
    case world::BlockType::SandstoneStairsEast:
    case world::BlockType::SandstoneStairsSouth:
    case world::BlockType::SandstoneStairsWest:
    case world::BlockType::StoneStairs:
    case world::BlockType::StoneStairsNorth:
    case world::BlockType::StoneStairsEast:
    case world::BlockType::StoneStairsSouth:
    case world::BlockType::StoneStairsWest:
    case world::BlockType::Furnace:
    case world::BlockType::FurnaceNorth:
    case world::BlockType::FurnaceEast:
    case world::BlockType::FurnaceWest:
    case world::BlockType::Bricks:
    case world::BlockType::BrickStairs:
    case world::BlockType::BrickStairsNorth:
    case world::BlockType::BrickStairsEast:
    case world::BlockType::BrickStairsSouth:
    case world::BlockType::BrickStairsWest:
    case world::BlockType::Glowstone:
    case world::BlockType::Obsidian:
    case world::BlockType::Glass:
    case world::BlockType::DripstoneBlock:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isWoodFamilyBlockType(const world::BlockType blockType)
{
    if (world::isWoodDoorBlock(blockType))
    {
        return true;
    }

    return blockType == world::BlockType::OakLog || blockType == world::BlockType::OakLeaves
        || blockType == world::BlockType::JungleLog || blockType == world::BlockType::JungleLeaves
        || blockType == world::BlockType::SpruceLog || blockType == world::BlockType::SpruceLeaves
        || blockType == world::BlockType::Bookshelf || blockType == world::BlockType::JunglePlanks
        || blockType == world::BlockType::OakStairs || blockType == world::BlockType::OakStairsNorth
        || blockType == world::BlockType::OakStairsEast || blockType == world::BlockType::OakStairsSouth
        || blockType == world::BlockType::OakStairsWest
        || blockType == world::BlockType::JungleStairs || blockType == world::BlockType::JungleStairsNorth
        || blockType == world::BlockType::JungleStairsEast || blockType == world::BlockType::JungleStairsSouth
        || blockType == world::BlockType::JungleStairsWest;
}

[[nodiscard]] bool isPickaxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isStoneOrOreBlockType(targetBlockType);
}

[[nodiscard]] bool isAxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isWoodFamilyBlockType(targetBlockType) || targetBlockType == world::BlockType::OakPlanks
        || targetBlockType == world::BlockType::JunglePlanks
        || targetBlockType == world::BlockType::CraftingTable || targetBlockType == world::BlockType::Chest
        || targetBlockType == world::BlockType::Bookshelf;
}

[[nodiscard]] MiningToolProfile miningToolProfile(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodPickaxe:
        return {.kind = MiningToolKind::Pickaxe, .effectiveSpeed = 2.0f};
    case EquippedItem::StonePickaxe:
        return {.kind = MiningToolKind::Pickaxe, .effectiveSpeed = 4.0f};
    case EquippedItem::IronPickaxe:
        return {.kind = MiningToolKind::Pickaxe, .effectiveSpeed = 6.0f};
    case EquippedItem::DiamondPickaxe:
        return {.kind = MiningToolKind::Pickaxe, .effectiveSpeed = 8.0f};
    case EquippedItem::GoldPickaxe:
        return {.kind = MiningToolKind::Pickaxe, .effectiveSpeed = 12.0f};
    case EquippedItem::WoodAxe:
        return {.kind = MiningToolKind::Axe, .effectiveSpeed = 2.0f};
    case EquippedItem::StoneAxe:
        return {.kind = MiningToolKind::Axe, .effectiveSpeed = 4.0f};
    case EquippedItem::IronAxe:
        return {.kind = MiningToolKind::Axe, .effectiveSpeed = 6.0f};
    case EquippedItem::DiamondAxe:
        return {.kind = MiningToolKind::Axe, .effectiveSpeed = 8.0f};
    case EquippedItem::GoldAxe:
        return {.kind = MiningToolKind::Axe, .effectiveSpeed = 12.0f};
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
    case EquippedItem::DiamondSword:
        return {.kind = MiningToolKind::Sword, .effectiveSpeed = 1.5f};
    case EquippedItem::None:
    default:
        return {};
    }
}

[[nodiscard]] float toolMiningSpeedMultiplier(
    const EquippedItem equippedItem,
    const world::BlockType targetBlockType)
{
    const MiningToolProfile tool = miningToolProfile(equippedItem);
    switch (tool.kind)
    {
    case MiningToolKind::Pickaxe:
        return isPickaxeEffectiveTarget(targetBlockType) ? tool.effectiveSpeed : 1.0f;
    case MiningToolKind::Axe:
        return isAxeEffectiveTarget(targetBlockType) ? tool.effectiveSpeed : 1.0f;
    case MiningToolKind::Sword:
        return (isPickaxeEffectiveTarget(targetBlockType) || isAxeEffectiveTarget(targetBlockType)) ? 1.0f
                                                                                                     : tool.effectiveSpeed;
    case MiningToolKind::None:
    default:
        return 1.0f;
    }
}

[[nodiscard]] float heldBlockMiningSpeedMultiplier(
    const world::BlockType equippedBlockType,
    const world::BlockType targetBlockType)
{
    if (equippedBlockType == world::BlockType::Air)
    {
        return 1.0f;
    }
    if (equippedBlockType == targetBlockType)
    {
        return 2.0f;
    }
    if (isStoneOrOreBlockType(targetBlockType) && isStoneOrOreBlockType(equippedBlockType))
    {
        return 2.5f;
    }
    if (isEarthyBlockType(targetBlockType) && isEarthyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    if (isWoodFamilyBlockType(targetBlockType) && isWoodFamilyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    return 1.15f;
}
}  // namespace

float miningDurationSeconds(
    const world::BlockType targetBlockType,
    const world::BlockType equippedBlockType,
    const EquippedItem equippedItem)
{
    constexpr float kHardnessToSeconds = 0.65f;
    constexpr float kMinimumBreakDurationSeconds = 0.06f;
    const world::BlockMetadata metadata = world::blockMetadata(targetBlockType);
    if (!metadata.breakable)
    {
        return std::numeric_limits<float>::max();
    }

    const float blockMultiplier = heldBlockMiningSpeedMultiplier(equippedBlockType, targetBlockType);
    const float toolMultiplier = toolMiningSpeedMultiplier(equippedItem, targetBlockType);
    const float speedMultiplier = std::max(blockMultiplier, toolMultiplier);
    const float rawDurationSeconds = metadata.hardness * kHardnessToSeconds / speedMultiplier;
    return std::max(kMinimumBreakDurationSeconds, rawDurationSeconds);
}
}  // namespace vibecraft::app
