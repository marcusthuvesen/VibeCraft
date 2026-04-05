#include "vibecraft/app/ApplicationCombat.hpp"

namespace vibecraft::app
{
float meleeDamageForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
        return 3.0f;
    case EquippedItem::StoneSword:
        return 4.0f;
    case EquippedItem::IronSword:
        return 5.0f;
    case EquippedItem::GoldSword:
        return 3.5f;
    case EquippedItem::DiamondSword:
        return 7.0f;
    case EquippedItem::WoodAxe:
        return 4.0f;
    case EquippedItem::StoneAxe:
        return 5.5f;
    case EquippedItem::IronAxe:
        return 6.5f;
    case EquippedItem::GoldAxe:
        return 4.25f;
    case EquippedItem::DiamondAxe:
        return 9.0f;
    case EquippedItem::WoodPickaxe:
        return 2.0f;
    case EquippedItem::StonePickaxe:
        return 2.75f;
    case EquippedItem::IronPickaxe:
        return 3.5f;
    case EquippedItem::GoldPickaxe:
        return 2.25f;
    case EquippedItem::DiamondPickaxe:
        return 5.0f;
    case EquippedItem::None:
    default:
        return 1.0f;
    }
}

float meleeReachForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return 3.05f;
    case EquippedItem::DiamondSword:
        return 3.45f;
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
    case EquippedItem::DiamondAxe:
        return 3.1f;
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return 2.85f;
    case EquippedItem::None:
    default:
        return 2.75f;
    }
}

float knockbackDistanceForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
        return 0.55f;
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return 0.65f;
    case EquippedItem::DiamondSword:
        return 0.9f;
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
        return 0.62f;
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
        return 0.72f;
    case EquippedItem::DiamondAxe:
        return 0.95f;
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
        return 0.48f;
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return 0.52f;
    case EquippedItem::None:
    default:
        return 0.45f;
    }
}

EquippedItem mobDropItemForKind(const game::MobKind mobKind)
{
    using MK = game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Skeleton:
    case MK::Creeper:
    case MK::Spider:
        return EquippedItem::RottenFlesh;
    case MK::Player:
        return EquippedItem::None;
    case MK::Cow:
        return EquippedItem::Leather;
    case MK::Pig:
        return EquippedItem::Leather;
    case MK::Sheep:
        return EquippedItem::Mutton;
    case MK::Chicken:
        return EquippedItem::Feather;
    }
    return EquippedItem::RottenFlesh;
}
}  // namespace vibecraft::app
