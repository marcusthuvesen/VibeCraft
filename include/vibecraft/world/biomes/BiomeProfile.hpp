#pragma once

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/biomes/SurfaceBiome.hpp"

namespace vibecraft::world::biomes
{
enum class TreeGenerationFamily : std::uint8_t
{
    None,
    Plains,
    Forest,
    BirchForest,
    DarkForest,
    Taiga,
    SnowyTaiga,
    Jungle,
    SparseJungle,
    BambooJungle,
};

enum class FloraGenerationFamily : std::uint8_t
{
    Plains,
    SunflowerPlains,
    Meadow,
    Savanna,
    Forest,
    FlowerForest,
    BirchForest,
    DarkForest,
    Taiga,
    WindsweptHills,
    MushroomField,
    SnowyPlains,
    IcePlains,
    SnowyTaiga,
    Desert,
    Jungle,
    SparseJungle,
    BambooJungle,
    Swamp,
};

struct BiomeProfile
{
    SurfaceBiome biome = SurfaceBiome::Forest;
    const char* label = "forest";
    BlockType canonicalSurfaceBlock = BlockType::Grass;
    BlockType canonicalSubsurfaceBlock = BlockType::Dirt;
    TreeGenerationFamily treeFamily = TreeGenerationFamily::Forest;
    FloraGenerationFamily floraFamily = FloraGenerationFamily::Forest;
    bool usesSandStrata = false;
    bool forested = false;
    bool snowy = false;
    bool taiga = false;
    bool jungle = false;
    bool starterFriendly = false;
};

[[nodiscard]] const BiomeProfile& biomeProfile(SurfaceBiome biome);
[[nodiscard]] bool isForestSurfaceBiome(SurfaceBiome biome);
[[nodiscard]] bool isTaigaSurfaceBiome(SurfaceBiome biome);
[[nodiscard]] bool isSnowySurfaceBiome(SurfaceBiome biome);
[[nodiscard]] bool isSandySurfaceBiome(SurfaceBiome biome);
[[nodiscard]] bool isJungleSurfaceBiome(SurfaceBiome biome);
[[nodiscard]] bool isTemperateGrassSurfaceBiome(SurfaceBiome biome);
}  // namespace vibecraft::world::biomes
