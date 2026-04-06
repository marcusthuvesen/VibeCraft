#include "vibecraft/app/Inventory.hpp"

#include <algorithm>
#include <array>

namespace vibecraft::app
{
namespace
{
void maybeSelectHotbarSlot(
    std::size_t* const selectedHotbarIndex,
    const InventorySelectionBehavior selectionBehavior,
    const std::size_t slotIndex)
{
    if (selectedHotbarIndex != nullptr
        && selectionBehavior == InventorySelectionBehavior::SelectAffectedHotbarSlot)
    {
        *selectedHotbarIndex = slotIndex;
    }
}

template <std::size_t SlotCount>
bool addToMatchingOrEmptySlots(
    std::array<InventorySlot, SlotCount>& slots,
    const vibecraft::world::BlockType blockType,
    std::size_t* const selectedHotbarIndex,
    const InventorySelectionBehavior selectionBehavior)
{
    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.blockType == blockType && slot.equippedItem == EquippedItem::None
            && slot.count < inventorySlotStackLimit(slot))
        {
            ++slot.count;
            maybeSelectHotbarSlot(selectedHotbarIndex, selectionBehavior, slotIndex);
            return true;
        }
    }

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.count == 0)
        {
            if (slot.equippedItem != EquippedItem::None)
            {
                continue;
            }
            slot.blockType = blockType;
            slot.count = 1;
            slot.durabilityRemaining = 0;
            maybeSelectHotbarSlot(selectedHotbarIndex, selectionBehavior, slotIndex);
            return true;
        }
    }

    return false;
}

template <std::size_t SlotCount>
bool addEquippedItemToSlots(
    std::array<InventorySlot, SlotCount>& slots,
    const EquippedItem equippedItem,
    std::size_t* const selectedHotbarIndex,
    const InventorySelectionBehavior selectionBehavior)
{
    if (!isDamageableEquippedItem(equippedItem))
    {
        for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
        {
            InventorySlot& slot = slots[slotIndex];
            if (slot.equippedItem == equippedItem && slot.count < inventorySlotStackLimit(slot))
            {
                ++slot.count;
                maybeSelectHotbarSlot(selectedHotbarIndex, selectionBehavior, slotIndex);
                return true;
            }
        }
    }

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.count == 0)
        {
            slot.blockType = vibecraft::world::BlockType::Air;
            slot.equippedItem = equippedItem;
            slot.count = 1;
            slot.durabilityRemaining = maxDurabilityForEquippedItem(equippedItem);
            maybeSelectHotbarSlot(selectedHotbarIndex, selectionBehavior, slotIndex);
            return true;
        }
    }

    return false;
}

void refillHotbarSlotFromBag(HotbarSlots& hotbarSlots, BagSlots& bagSlots, const std::size_t hotbarIndex)
{
    InventorySlot& hotbarSlot = hotbarSlots[hotbarIndex];
    if (hotbarSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    for (InventorySlot& bagSlot : bagSlots)
    {
        if (bagSlot.count > 0 && bagSlot.blockType == hotbarSlot.blockType)
        {
            const std::uint32_t transferCount = std::min(kMaxStackSize, bagSlot.count);
            hotbarSlot.count = transferCount;
            hotbarSlot.equippedItem = EquippedItem::None;
            hotbarSlot.durabilityRemaining = 0;
            bagSlot.count -= transferCount;
            if (bagSlot.count == 0)
            {
                bagSlot.blockType = vibecraft::world::BlockType::Air;
            }
            return;
        }
    }

    for (InventorySlot& bagSlot : bagSlots)
    {
        if (bagSlot.count > 0)
        {
            const std::uint32_t transferCount = std::min(kMaxStackSize, bagSlot.count);
            hotbarSlot.blockType = bagSlot.blockType;
            hotbarSlot.count = transferCount;
            hotbarSlot.equippedItem = EquippedItem::None;
            hotbarSlot.durabilityRemaining = 0;
            bagSlot.count -= transferCount;
            if (bagSlot.count == 0)
            {
                bagSlot.blockType = vibecraft::world::BlockType::Air;
            }
            return;
        }
    }
}

bool inventorySlotIsEmpty(const InventorySlot& slot)
{
    return slot.count == 0
        || (slot.blockType == vibecraft::world::BlockType::Air && slot.equippedItem == EquippedItem::None);
}
}  // namespace

bool isDamageableEquippedItem(const EquippedItem equippedItem)
{
    return maxDurabilityForEquippedItem(equippedItem) > 0;
}

std::uint16_t maxDurabilityForEquippedItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::WoodPickaxe:
    case EquippedItem::WoodAxe:
        return 59;
    case EquippedItem::StoneSword:
    case EquippedItem::StonePickaxe:
    case EquippedItem::StoneAxe:
        return 131;
    case EquippedItem::IronSword:
    case EquippedItem::IronPickaxe:
    case EquippedItem::IronAxe:
        return 250;
    case EquippedItem::DiamondSword:
    case EquippedItem::DiamondPickaxe:
    case EquippedItem::DiamondAxe:
        return 1561;
    case EquippedItem::GoldSword:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::GoldAxe:
        return 32;
    case EquippedItem::Bow:
        return 384;
    case EquippedItem::None:
    default:
        return 0;
    }
}

std::uint16_t durabilityUseAmountForEquippedItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::DiamondSword:
        return 1;
    case EquippedItem::IronSword:
        return 2;
    case EquippedItem::StoneSword:
        return 3;
    case EquippedItem::WoodSword:
        return 4;
    case EquippedItem::GoldSword:
        return 5;
    case EquippedItem::Bow:
        return 1;
    case EquippedItem::None:
    default:
        return 1;
    }
}

std::uint32_t inventorySlotStackLimit(const InventorySlot& slot)
{
    return isDamageableEquippedItem(slot.equippedItem) ? 1U : kMaxStackSize;
}

bool consumeEquippedItemDurability(InventorySlot& slot, const std::uint16_t amount)
{
    if (amount == 0 || slot.count == 0 || !isDamageableEquippedItem(slot.equippedItem))
    {
        return false;
    }
    const std::uint16_t maxDurability = maxDurabilityForEquippedItem(slot.equippedItem);
    if (maxDurability == 0)
    {
        return false;
    }
    if (slot.durabilityRemaining == 0 || slot.durabilityRemaining > maxDurability)
    {
        slot.durabilityRemaining = maxDurability;
    }
    if (amount >= slot.durabilityRemaining)
    {
        slot.blockType = vibecraft::world::BlockType::Air;
        slot.equippedItem = EquippedItem::None;
        slot.count = 0;
        slot.durabilityRemaining = 0;
        return true;
    }
    slot.durabilityRemaining =
        static_cast<std::uint16_t>(slot.durabilityRemaining - amount);
    return false;
}

void compactBagSlots(BagSlots& bagSlots)
{
    BagSlots original = bagSlots;
    std::size_t writeIndex = 0;
    for (const InventorySlot& slot : original)
    {
        if (!inventorySlotIsEmpty(slot))
        {
            bagSlots[writeIndex++] = slot;
        }
    }
    for (; writeIndex < bagSlots.size(); ++writeIndex)
    {
        bagSlots[writeIndex] = InventorySlot{};
    }
}

const char* blockTypeLabel(const vibecraft::world::BlockType blockType)
{
    switch (vibecraft::world::doorFamilyForBlockType(blockType))
    {
    case vibecraft::world::DoorFamily::Oak:
        return "Oak Door";
    case vibecraft::world::DoorFamily::Jungle:
        return "Jungle Door";
    case vibecraft::world::DoorFamily::Iron:
        return "Iron Door";
    case vibecraft::world::DoorFamily::None:
    default:
        break;
    }

    switch (blockType)
    {
    case vibecraft::world::BlockType::Grass:
        return "Grass Block";
    case vibecraft::world::BlockType::SnowGrass:
        return "Snowy Grass Block";
    case vibecraft::world::BlockType::JungleGrass:
        return "Mossy Grass Block";
    case vibecraft::world::BlockType::Dirt:
        return "Dirt";
    case vibecraft::world::BlockType::Stone:
        return "Stone";
    case vibecraft::world::BlockType::Deepslate:
        return "Deepslate";
    case vibecraft::world::BlockType::CoalOre:
        return "Coal Ore";
    case vibecraft::world::BlockType::Sand:
        return "Sand";
    case vibecraft::world::BlockType::Bedrock:
        return "Bedrock";
    case vibecraft::world::BlockType::Water:
        return "Water";
    case vibecraft::world::BlockType::IronOre:
        return "Iron Ore";
    case vibecraft::world::BlockType::GoldOre:
        return "Gold Ore";
    case vibecraft::world::BlockType::DiamondOre:
        return "Diamond Ore";
    case vibecraft::world::BlockType::EmeraldOre:
        return "Emerald Ore";
    case vibecraft::world::BlockType::Lava:
        return "Lava";
    case vibecraft::world::BlockType::OakLog:
        return "Oak Log";
    case vibecraft::world::BlockType::OakLeaves:
        return "Oak Leaves";
    case vibecraft::world::BlockType::BirchLog:
        return "Birch Log";
    case vibecraft::world::BlockType::BirchLeaves:
        return "Birch Leaves";
    case vibecraft::world::BlockType::DarkOakLog:
        return "Dark Oak Log";
    case vibecraft::world::BlockType::DarkOakLeaves:
        return "Dark Oak Leaves";
    case vibecraft::world::BlockType::SculkBlock:
        return "Sculk Block";
    case vibecraft::world::BlockType::DripstoneBlock:
        return "Dripstone Block";
    case vibecraft::world::BlockType::JungleLog:
        return "Jungle Log";
    case vibecraft::world::BlockType::JungleLeaves:
        return "Jungle Leaves";
    case vibecraft::world::BlockType::SpruceLog:
        return "Spruce Log";
    case vibecraft::world::BlockType::SpruceLeaves:
        return "Spruce Leaves";
    case vibecraft::world::BlockType::OakPlanks:
        return "Oak Planks";
    case vibecraft::world::BlockType::CraftingTable:
        return "Crafting Table";
    case vibecraft::world::BlockType::Cobblestone:
        return "Cobblestone";
    case vibecraft::world::BlockType::Sandstone:
        return "Sandstone";
    case vibecraft::world::BlockType::Furnace:
    case vibecraft::world::BlockType::FurnaceNorth:
    case vibecraft::world::BlockType::FurnaceEast:
    case vibecraft::world::BlockType::FurnaceWest:
        return "Furnace";
    case vibecraft::world::BlockType::Chest:
        return "Chest";
    case vibecraft::world::BlockType::OxygenGenerator:
        return "Industrial Relay";
    case vibecraft::world::BlockType::Torch:
    case vibecraft::world::BlockType::TorchNorth:
    case vibecraft::world::BlockType::TorchEast:
    case vibecraft::world::BlockType::TorchSouth:
    case vibecraft::world::BlockType::TorchWest:
        return "Torch";
    case vibecraft::world::BlockType::TNT:
        return "TNT";
    case vibecraft::world::BlockType::Glass:
        return "Glass";
    case vibecraft::world::BlockType::Bricks:
        return "Bricks";
    case vibecraft::world::BlockType::Bookshelf:
        return "Bookshelf";
    case vibecraft::world::BlockType::Glowstone:
        return "Glowstone";
    case vibecraft::world::BlockType::Obsidian:
        return "Obsidian";
    case vibecraft::world::BlockType::Gravel:
        return "Gravel";
    case vibecraft::world::BlockType::Cactus:
        return "Cactus";
    case vibecraft::world::BlockType::Dandelion:
        return "Dandelion";
    case vibecraft::world::BlockType::Poppy:
        return "Poppy";
    case vibecraft::world::BlockType::BlueOrchid:
        return "Blue Orchid";
    case vibecraft::world::BlockType::Allium:
        return "Allium";
    case vibecraft::world::BlockType::OxeyeDaisy:
        return "Oxeye Daisy";
    case vibecraft::world::BlockType::BrownMushroom:
        return "Brown Mushroom";
    case vibecraft::world::BlockType::RedMushroom:
        return "Red Mushroom";
    case vibecraft::world::BlockType::DeadBush:
        return "Dead Bush";
    case vibecraft::world::BlockType::Fern:
        return "Fern";
    case vibecraft::world::BlockType::Podzol:
        return "Podzol";
    case vibecraft::world::BlockType::CoarseDirt:
        return "Coarse Dirt";
    case vibecraft::world::BlockType::GrassTuft:
        return "Grass";
    case vibecraft::world::BlockType::SparseTuft:
        return "Sparse Tuft";
    case vibecraft::world::BlockType::FlowerTuft:
        return "Flower Tuft";
    case vibecraft::world::BlockType::DryTuft:
        return "Dry Tuft";
    case vibecraft::world::BlockType::LushTuft:
        return "Lush Tuft";
    case vibecraft::world::BlockType::FrostTuft:
        return "Frost Tuft";
    case vibecraft::world::BlockType::CloverTuft:
        return "Clover Tuft";
    case vibecraft::world::BlockType::SproutTuft:
        return "Sprout Tuft";
    case vibecraft::world::BlockType::Vines:
        return "Vines";
    case vibecraft::world::BlockType::Ladder:
        return "Ladder";
    case vibecraft::world::BlockType::OakStairs:
    case vibecraft::world::BlockType::OakStairsNorth:
    case vibecraft::world::BlockType::OakStairsEast:
    case vibecraft::world::BlockType::OakStairsSouth:
    case vibecraft::world::BlockType::OakStairsWest:
        return "Oak Stairs";
    case vibecraft::world::BlockType::JungleStairs:
    case vibecraft::world::BlockType::JungleStairsNorth:
    case vibecraft::world::BlockType::JungleStairsEast:
    case vibecraft::world::BlockType::JungleStairsSouth:
    case vibecraft::world::BlockType::JungleStairsWest:
        return "Jungle Stairs";
    case vibecraft::world::BlockType::CobblestoneStairs:
    case vibecraft::world::BlockType::CobblestoneStairsNorth:
    case vibecraft::world::BlockType::CobblestoneStairsEast:
    case vibecraft::world::BlockType::CobblestoneStairsSouth:
    case vibecraft::world::BlockType::CobblestoneStairsWest:
        return "Cobblestone Stairs";
    case vibecraft::world::BlockType::StoneStairs:
    case vibecraft::world::BlockType::StoneStairsNorth:
    case vibecraft::world::BlockType::StoneStairsEast:
    case vibecraft::world::BlockType::StoneStairsSouth:
    case vibecraft::world::BlockType::StoneStairsWest:
        return "Stone Stairs";
    case vibecraft::world::BlockType::BrickStairs:
    case vibecraft::world::BlockType::BrickStairsNorth:
    case vibecraft::world::BlockType::BrickStairsEast:
    case vibecraft::world::BlockType::BrickStairsSouth:
    case vibecraft::world::BlockType::BrickStairsWest:
        return "Brick Stairs";
    case vibecraft::world::BlockType::SandstoneStairs:
    case vibecraft::world::BlockType::SandstoneStairsNorth:
    case vibecraft::world::BlockType::SandstoneStairsEast:
    case vibecraft::world::BlockType::SandstoneStairsSouth:
    case vibecraft::world::BlockType::SandstoneStairsWest:
        return "Sandstone Stairs";
    case vibecraft::world::BlockType::CocoaPod:
        return "Cocoa Pod";
    case vibecraft::world::BlockType::Melon:
        return "Melon";
    case vibecraft::world::BlockType::Bamboo:
        return "Bamboo";
    case vibecraft::world::BlockType::JunglePlanks:
        return "Jungle Planks";
    case vibecraft::world::BlockType::MossBlock:
        return "Moss Block";
    case vibecraft::world::BlockType::MossyCobblestone:
        return "Mossy Cobblestone";
    case vibecraft::world::BlockType::HabitatPanel:
        return "Habitat Panel";
    case vibecraft::world::BlockType::HabitatFloor:
        return "Habitat Floor";
    case vibecraft::world::BlockType::HabitatFrame:
        return "Habitat Frame";
    case vibecraft::world::BlockType::AirlockPanel:
        return "Airlock Panel";
    case vibecraft::world::BlockType::PowerConduit:
        return "Power Conduit";
    case vibecraft::world::BlockType::GreenhouseGlass:
        return "Greenhouse Glass";
    case vibecraft::world::BlockType::PlanterTray:
        return "Planter Tray";
    case vibecraft::world::BlockType::FiberSapling:
        return "Fiber Sapling";
    case vibecraft::world::BlockType::FiberSprout:
        return "Fiber Sprout";
    case vibecraft::world::BlockType::Air:
    default:
        return "Empty";
    }
}

const char* equippedItemLabel(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::DiamondSword:
        return "Diamond Sword";
    case EquippedItem::Stick:
        return "Stick";
    case EquippedItem::RottenFlesh:
        return "Rotten Flesh";
    case EquippedItem::Leather:
        return "Leather";
    case EquippedItem::RawPorkchop:
        return "Raw Pork";
    case EquippedItem::Mutton:
        return "Raw Mutton";
    case EquippedItem::Feather:
        return "Feather";
    case EquippedItem::WoodSword:
        return "Wooden Sword";
    case EquippedItem::StoneSword:
        return "Stone Sword";
    case EquippedItem::IronSword:
        return "Iron Sword";
    case EquippedItem::GoldSword:
        return "Golden Sword";
    case EquippedItem::WoodPickaxe:
        return "Wooden Pickaxe";
    case EquippedItem::StonePickaxe:
        return "Stone Pickaxe";
    case EquippedItem::IronPickaxe:
        return "Iron Pickaxe";
    case EquippedItem::GoldPickaxe:
        return "Golden Pickaxe";
    case EquippedItem::DiamondPickaxe:
        return "Diamond Pickaxe";
    case EquippedItem::WoodAxe:
        return "Wooden Axe";
    case EquippedItem::StoneAxe:
        return "Stone Axe";
    case EquippedItem::IronAxe:
        return "Iron Axe";
    case EquippedItem::GoldAxe:
        return "Golden Axe";
    case EquippedItem::DiamondAxe:
        return "Diamond Axe";
    case EquippedItem::Coal:
        return "Coal";
    case EquippedItem::Charcoal:
        return "Charcoal";
    case EquippedItem::ScoutHelmet:
        return "Scout Helmet";
    case EquippedItem::ScoutChestRig:
        return "Scout Chest Rig";
    case EquippedItem::ScoutGreaves:
        return "Scout Greaves";
    case EquippedItem::ScoutBoots:
        return "Scout Boots";
    case EquippedItem::IronIngot:
        return "Iron Ingot";
    case EquippedItem::GoldIngot:
        return "Gold Ingot";
    case EquippedItem::Arrow:
        return "Arrow";
    case EquippedItem::Bow:
        return "Bow";
    case EquippedItem::String:
        return "String";
    case EquippedItem::Gunpowder:
        return "Gunpowder";
    case EquippedItem::None:
    default:
        return "Empty";
    }
}

const char* equipmentSlotLabel(const EquipmentSlotKind slotKind)
{
    switch (slotKind)
    {
    case EquipmentSlotKind::Helmet:
        return "Helmet";
    case EquipmentSlotKind::Chestplate:
        return "Chest";
    case EquipmentSlotKind::Leggings:
        return "Legs";
    case EquipmentSlotKind::Boots:
        return "Boots";
    }

    return "Slot";
}

std::string inventorySlotLabel(const InventorySlot& slot)
{
    if (slot.count == 0)
    {
        return "Empty";
    }
    if (slot.equippedItem != EquippedItem::None)
    {
        const std::string label = equippedItemLabel(slot.equippedItem);
        if (!isDamageableEquippedItem(slot.equippedItem))
        {
            return label;
        }
        const std::uint16_t maxDurability = maxDurabilityForEquippedItem(slot.equippedItem);
        const std::uint16_t durability =
            slot.durabilityRemaining == 0 ? maxDurability : slot.durabilityRemaining;
        return std::string(label)
            + " (" + std::to_string(durability) + "/" + std::to_string(maxDurability) + ")";
    }
    return blockTypeLabel(slot.blockType);
}

float armorProtectionFractionForEquippedItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::ScoutHelmet:
        return 0.04f;
    case EquippedItem::ScoutChestRig:
        return 0.12f;
    case EquippedItem::ScoutGreaves:
        return 0.08f;
    case EquippedItem::ScoutBoots:
        return 0.04f;
    case EquippedItem::None:
    default:
        return 0.0f;
    }
}

bool canPlaceIntoEquipmentSlot(const InventorySlot& slot, const EquipmentSlotKind slotKind)
{
    if (slot.count == 0)
    {
        return true;
    }

    if (slot.blockType != vibecraft::world::BlockType::Air || slot.equippedItem == EquippedItem::None)
    {
        return false;
    }

    switch (slotKind)
    {
    case EquipmentSlotKind::Helmet:
        return slot.equippedItem == EquippedItem::ScoutHelmet;
    case EquipmentSlotKind::Chestplate:
        return slot.equippedItem == EquippedItem::ScoutChestRig;
    case EquipmentSlotKind::Leggings:
        return slot.equippedItem == EquippedItem::ScoutGreaves;
    case EquipmentSlotKind::Boots:
        return slot.equippedItem == EquippedItem::ScoutBoots;
    }

    return false;
}

bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex,
    const InventorySelectionBehavior selectionBehavior)
{
    if (blockType == vibecraft::world::BlockType::Air || blockType == vibecraft::world::BlockType::Water
        || blockType == vibecraft::world::BlockType::Lava)
    {
        return false;
    }

    if (addToMatchingOrEmptySlots(hotbarSlots, blockType, &selectedHotbarIndex, selectionBehavior))
    {
        return true;
    }

    return addToMatchingOrEmptySlots(
        bagSlots,
        blockType,
        nullptr,
        InventorySelectionBehavior::PreserveCurrent);
}

bool addEquippedItemToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const EquippedItem equippedItem,
    std::size_t& selectedHotbarIndex,
    const InventorySelectionBehavior selectionBehavior)
{
    if (equippedItem == EquippedItem::None)
    {
        return false;
    }

    if (addEquippedItemToSlots(hotbarSlots, equippedItem, &selectedHotbarIndex, selectionBehavior))
    {
        return true;
    }

    return addEquippedItemToSlots(
        bagSlots,
        equippedItem,
        nullptr,
        InventorySelectionBehavior::PreserveCurrent);
}

void applyCreativeInventoryLoadout(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    std::size_t& selectedHotbarIndex)
{
    hotbarSlots.fill({});
    bagSlots.fill({});

    constexpr std::size_t kLastBlockType = static_cast<std::size_t>(world::BlockType::IronDoorUpperWestOpen);
    std::array<bool, kLastBlockType + 1> seenBlockTypes{};
    std::size_t nextSlotIndex = 0;

    const auto assignSlot = [](InventorySlot& slot, const world::BlockType blockType)
    {
        slot.blockType = blockType;
        slot.count = kMaxStackSize;
        slot.equippedItem = EquippedItem::None;
        slot.durabilityRemaining = 0;
    };

    for (std::size_t rawBlockIndex = 1; rawBlockIndex <= kLastBlockType; ++rawBlockIndex)
    {
        const world::BlockType rawBlockType = static_cast<world::BlockType>(rawBlockIndex);
        const world::BlockType blockType = world::normalizePlaceVariantBlockType(rawBlockType);
        if (blockType == world::BlockType::Air)
        {
            continue;
        }

        const std::size_t normalizedIndex = static_cast<std::size_t>(blockType);
        if (seenBlockTypes[normalizedIndex])
        {
            continue;
        }
        seenBlockTypes[normalizedIndex] = true;

        if (nextSlotIndex < hotbarSlots.size())
        {
            assignSlot(hotbarSlots[nextSlotIndex], blockType);
        }
        else
        {
            const std::size_t bagIndex = nextSlotIndex - hotbarSlots.size();
            if (bagIndex >= bagSlots.size())
            {
                break;
            }
            assignSlot(bagSlots[bagIndex], blockType);
        }

        ++nextSlotIndex;
    }

    selectedHotbarIndex = 0;
}

void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, const std::size_t selectedHotbarIndex)
{
    InventorySlot& selectedSlot = hotbarSlots[selectedHotbarIndex];
    if (selectedSlot.count == 0)
    {
        selectedSlot.blockType = vibecraft::world::BlockType::Air;
        selectedSlot.equippedItem = EquippedItem::None;
        selectedSlot.durabilityRemaining = 0;
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
        return;
    }

    --selectedSlot.count;
    if (selectedSlot.count == 0)
    {
        selectedSlot.blockType = vibecraft::world::BlockType::Air;
        selectedSlot.equippedItem = EquippedItem::None;
        selectedSlot.durabilityRemaining = 0;
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
    }
}

}  // namespace vibecraft::app
