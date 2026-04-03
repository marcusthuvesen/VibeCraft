#include "vibecraft/app/Inventory.hpp"

#include <algorithm>

namespace vibecraft::app
{
namespace
{
template <std::size_t SlotCount>
bool addToMatchingOrEmptySlots(
    std::array<InventorySlot, SlotCount>& slots,
    const vibecraft::world::BlockType blockType,
    std::size_t* const selectedHotbarIndex)
{
    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.blockType == blockType && slot.count < kMaxStackSize)
        {
            ++slot.count;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
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
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
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
        return "Furnace";
    case vibecraft::world::BlockType::Chest:
        return "Chest";
    case vibecraft::world::BlockType::OxygenGenerator:
        return "Oxygen Generator";
    case vibecraft::world::BlockType::Torch:
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
    case EquippedItem::OxygenCanister:
        return "Oxygen Canister";
    case EquippedItem::FieldTank:
        return "Field Tank";
    case EquippedItem::ExpeditionTank:
        return "Expedition Tank";
    case EquippedItem::Coal:
        return "Coal";
    case EquippedItem::StarterTank:
        return "Starter Tank";
    case EquippedItem::ScoutHelmet:
        return "Scout Helmet";
    case EquippedItem::ScoutChestRig:
        return "Scout Chest Rig";
    case EquippedItem::ScoutGreaves:
        return "Scout Greaves";
    case EquippedItem::ScoutBoots:
        return "Scout Boots";
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
    case EquipmentSlotKind::OxygenTank:
        return "Tank";
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
        return equippedItemLabel(slot.equippedItem);
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

    if (slotKind == EquipmentSlotKind::OxygenTank)
    {
        return slot.blockType == vibecraft::world::BlockType::Air
            && slot.count == 1
            && (slot.equippedItem == EquippedItem::StarterTank
                || slot.equippedItem == EquippedItem::FieldTank
                || slot.equippedItem == EquippedItem::ExpeditionTank);
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
    case EquipmentSlotKind::OxygenTank:
        return false;
    }

    return false;
}

bool addBlockToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const vibecraft::world::BlockType blockType,
    std::size_t& selectedHotbarIndex)
{
    if (blockType == vibecraft::world::BlockType::Air || blockType == vibecraft::world::BlockType::Water
        || blockType == vibecraft::world::BlockType::Lava)
    {
        return false;
    }

    if (addToMatchingOrEmptySlots(hotbarSlots, blockType, &selectedHotbarIndex))
    {
        return true;
    }

    return addToMatchingOrEmptySlots(bagSlots, blockType, nullptr);
}

void consumeSelectedHotbarSlot(HotbarSlots& hotbarSlots, BagSlots& bagSlots, const std::size_t selectedHotbarIndex)
{
    InventorySlot& selectedSlot = hotbarSlots[selectedHotbarIndex];
    if (selectedSlot.count == 0)
    {
        selectedSlot.blockType = vibecraft::world::BlockType::Air;
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
        return;
    }

    --selectedSlot.count;
    if (selectedSlot.count == 0)
    {
        refillHotbarSlotFromBag(hotbarSlots, bagSlots, selectedHotbarIndex);
    }
}

}  // namespace vibecraft::app
