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
        return "Regolith Turf";
    case vibecraft::world::BlockType::SnowGrass:
        return "Frosted Regolith";
    case vibecraft::world::BlockType::JungleGrass:
        return "Oxygen Moss";
    case vibecraft::world::BlockType::Dirt:
        return "Packed Regolith";
    case vibecraft::world::BlockType::Stone:
        return "Basalt";
    case vibecraft::world::BlockType::Deepslate:
        return "Deep Basalt";
    case vibecraft::world::BlockType::CoalOre:
        return "Carbon Ore";
    case vibecraft::world::BlockType::Sand:
        return "Red Dust";
    case vibecraft::world::BlockType::Bedrock:
        return "Bedrock";
    case vibecraft::world::BlockType::Water:
        return "Ice Melt";
    case vibecraft::world::BlockType::IronOre:
        return "Ferrite Ore";
    case vibecraft::world::BlockType::GoldOre:
        return "Auric Ore";
    case vibecraft::world::BlockType::DiamondOre:
        return "Azure Crystal";
    case vibecraft::world::BlockType::EmeraldOre:
        return "Resonance Crystal";
    case vibecraft::world::BlockType::Lava:
        return "Thermal Vent";
    case vibecraft::world::BlockType::TreeTrunk:
        return "Fiber Stem";
    case vibecraft::world::BlockType::TreeCrown:
        return "Canopy Fronds";
    case vibecraft::world::BlockType::JungleTreeTrunk:
        return "Grove Stem";
    case vibecraft::world::BlockType::JungleTreeCrown:
        return "Grove Canopy";
    case vibecraft::world::BlockType::SnowTreeTrunk:
        return "Frost Stem";
    case vibecraft::world::BlockType::SnowTreeCrown:
        return "Frost Fronds";
    case vibecraft::world::BlockType::OakPlanks:
        return "Habitat Planks";
    case vibecraft::world::BlockType::CraftingTable:
        return "Field Fabricator";
    case vibecraft::world::BlockType::Cobblestone:
        return "Fractured Basalt";
    case vibecraft::world::BlockType::Sandstone:
        return "Duststone";
    case vibecraft::world::BlockType::Oven:
        return "Thermal Smelter";
    case vibecraft::world::BlockType::Chest:
        return "Cargo Crate";
    case vibecraft::world::BlockType::OxygenGenerator:
        return "Atmos Relay";
    case vibecraft::world::BlockType::Torch:
        return "Flare Lamp";
    case vibecraft::world::BlockType::TNT:
        return "Seismic Charge";
    case vibecraft::world::BlockType::Glass:
        return "Habitat Glass";
    case vibecraft::world::BlockType::Bricks:
        return "Composite Bricks";
    case vibecraft::world::BlockType::Bookshelf:
        return "Data Shelf";
    case vibecraft::world::BlockType::Glowstone:
        return "Lumen Crystal";
    case vibecraft::world::BlockType::Obsidian:
        return "Volcanic Glass";
    case vibecraft::world::BlockType::Gravel:
        return "Shale Gravel";
    case vibecraft::world::BlockType::Cactus:
        return "Spine Reed";
    case vibecraft::world::BlockType::Dandelion:
        return "Sunspike Bloom";
    case vibecraft::world::BlockType::Poppy:
        return "Crimson Bloom";
    case vibecraft::world::BlockType::BlueOrchid:
        return "Blue Spore Bloom";
    case vibecraft::world::BlockType::Allium:
        return "Violet Spore Bloom";
    case vibecraft::world::BlockType::OxeyeDaisy:
        return "White Signal Bloom";
    case vibecraft::world::BlockType::BrownMushroom:
        return "Amber Fungus";
    case vibecraft::world::BlockType::RedMushroom:
        return "Crimson Fungus";
    case vibecraft::world::BlockType::DeadBush:
        return "Dry Thicket";
    case vibecraft::world::BlockType::Vines:
        return "Tendrils";
    case vibecraft::world::BlockType::CocoaPod:
        return "Seed Pod";
    case vibecraft::world::BlockType::Melon:
        return "Hydromelon";
    case vibecraft::world::BlockType::Bamboo:
        return "Reed Spire";
    case vibecraft::world::BlockType::JunglePlanks:
        return "Grove Planks";
    case vibecraft::world::BlockType::MossBlock:
        return "Oxygen Mat";
    case vibecraft::world::BlockType::MossyCobblestone:
        return "Mossy Basalt";
    case vibecraft::world::BlockType::HabitatPanel:
        return "Hab Panel";
    case vibecraft::world::BlockType::HabitatFloor:
        return "Deck Plating";
    case vibecraft::world::BlockType::HabitatFrame:
        return "Support Frame";
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
        return "Azure Blade";
    case EquippedItem::Stick:
        return "Fiber Rod";
    case EquippedItem::RottenFlesh:
        return "Spoiled Meat";
    case EquippedItem::Leather:
        return "Cured Hide";
    case EquippedItem::RawPorkchop:
        return "Raw Pork";
    case EquippedItem::Mutton:
        return "Raw Mutton";
    case EquippedItem::Feather:
        return "Feather";
    case EquippedItem::WoodSword:
        return "Habitat Blade";
    case EquippedItem::StoneSword:
        return "Basalt Blade";
    case EquippedItem::IronSword:
        return "Ferrite Blade";
    case EquippedItem::GoldSword:
        return "Auric Blade";
    case EquippedItem::WoodPickaxe:
        return "Habitat Pickaxe";
    case EquippedItem::StonePickaxe:
        return "Basalt Pickaxe";
    case EquippedItem::IronPickaxe:
        return "Ferrite Pickaxe";
    case EquippedItem::GoldPickaxe:
        return "Auric Pickaxe";
    case EquippedItem::DiamondPickaxe:
        return "Azure Pickaxe";
    case EquippedItem::WoodAxe:
        return "Habitat Axe";
    case EquippedItem::StoneAxe:
        return "Basalt Axe";
    case EquippedItem::IronAxe:
        return "Ferrite Axe";
    case EquippedItem::GoldAxe:
        return "Auric Axe";
    case EquippedItem::DiamondAxe:
        return "Azure Axe";
    case EquippedItem::OxygenCanister:
        return "Oxygen Canister";
    case EquippedItem::FieldTank:
        return "Field Tank";
    case EquippedItem::ExpeditionTank:
        return "Expedition Tank";
    case EquippedItem::Coal:
        return "Carbon Charge";
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
