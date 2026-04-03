#pragma once

#include <cstdint>

namespace vibecraft::world
{
enum class SurfaceBiome : std::uint8_t
{
    Plains,
    Forest,
    BirchForest,
    DarkForest,
    Taiga,
    SnowyPlains,
    SnowyTaiga,
    Desert,
    Jungle,
    SparseJungle,
    BambooJungle,
    SunflowerPlains,
    IcePlains,
    FlowerForest,
    Meadow,
    WindsweptHills,
    Swamp,
    Savanna,
    OldGrowthBirchForest,
    OldGrowthSpruceTaiga,
    OldGrowthPineTaiga,
    SnowySlopes,
    FrozenPeaks,
    JaggedPeaks,
    StonyPeaks,
    MushroomField,
    SavannaPlateau,
    WindsweptSavanna,
    IceSpikePlains,
};

[[nodiscard]] const char* surfaceBiomeLabel(SurfaceBiome biome);
}  // namespace vibecraft::world
