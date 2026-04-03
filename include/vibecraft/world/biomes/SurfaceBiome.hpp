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
};

[[nodiscard]] const char* surfaceBiomeLabel(SurfaceBiome biome);
}  // namespace vibecraft::world
