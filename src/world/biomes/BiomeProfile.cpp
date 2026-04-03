#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::world::biomes
{
namespace
{
constexpr BiomeProfile kPlainsProfile{
    .biome = SurfaceBiome::Plains,
    .label = "plains",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::Plains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kForestProfile{
    .biome = SurfaceBiome::Forest,
    .label = "forest",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Forest,
    .floraFamily = FloraGenerationFamily::Forest,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = true,
};

constexpr BiomeProfile kBirchForestProfile{
    .biome = SurfaceBiome::BirchForest,
    .label = "birch forest",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::BirchForest,
    .floraFamily = FloraGenerationFamily::BirchForest,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kDarkForestProfile{
    .biome = SurfaceBiome::DarkForest,
    .label = "dark forest",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::DarkForest,
    .floraFamily = FloraGenerationFamily::DarkForest,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kTaigaProfile{
    .biome = SurfaceBiome::Taiga,
    .label = "taiga",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Taiga,
    .floraFamily = FloraGenerationFamily::Taiga,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = true,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kSnowyPlainsProfile{
    .biome = SurfaceBiome::SnowyPlains,
    .label = "snowy plains",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::SnowyPlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = true,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kSnowyTaigaProfile{
    .biome = SurfaceBiome::SnowyTaiga,
    .label = "snowy taiga",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::SnowyTaiga,
    .floraFamily = FloraGenerationFamily::SnowyTaiga,
    .usesSandStrata = false,
    .forested = true,
    .snowy = true,
    .taiga = true,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kDesertProfile{
    .biome = SurfaceBiome::Desert,
    .label = "desert",
    .canonicalSurfaceBlock = BlockType::Sand,
    .canonicalSubsurfaceBlock = BlockType::Sand,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::Desert,
    .usesSandStrata = true,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kJungleProfile{
    .biome = SurfaceBiome::Jungle,
    .label = "jungle",
    .canonicalSurfaceBlock = BlockType::JungleGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Jungle,
    .floraFamily = FloraGenerationFamily::Jungle,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = true,
    .starterFriendly = false,
};

constexpr BiomeProfile kSparseJungleProfile{
    .biome = SurfaceBiome::SparseJungle,
    .label = "sparse jungle",
    .canonicalSurfaceBlock = BlockType::JungleGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::SparseJungle,
    .floraFamily = FloraGenerationFamily::SparseJungle,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = true,
    .starterFriendly = false,
};

constexpr BiomeProfile kBambooJungleProfile{
    .biome = SurfaceBiome::BambooJungle,
    .label = "bamboo jungle",
    .canonicalSurfaceBlock = BlockType::JungleGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::BambooJungle,
    .floraFamily = FloraGenerationFamily::BambooJungle,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = true,
    .starterFriendly = false,
};
}  // namespace

const BiomeProfile& biomeProfile(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Plains:
        return kPlainsProfile;
    case SurfaceBiome::Forest:
        return kForestProfile;
    case SurfaceBiome::BirchForest:
        return kBirchForestProfile;
    case SurfaceBiome::DarkForest:
        return kDarkForestProfile;
    case SurfaceBiome::Taiga:
        return kTaigaProfile;
    case SurfaceBiome::SnowyPlains:
        return kSnowyPlainsProfile;
    case SurfaceBiome::SnowyTaiga:
        return kSnowyTaigaProfile;
    case SurfaceBiome::Desert:
        return kDesertProfile;
    case SurfaceBiome::Jungle:
        return kJungleProfile;
    case SurfaceBiome::SparseJungle:
        return kSparseJungleProfile;
    case SurfaceBiome::BambooJungle:
        return kBambooJungleProfile;
    }

    return kForestProfile;
}

bool isForestSurfaceBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Forest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::Taiga:
    case SurfaceBiome::SnowyTaiga:
        return true;
    default:
        return false;
    }
}

bool isTaigaSurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::Taiga || biome == SurfaceBiome::SnowyTaiga;
}

bool isSnowySurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::SnowyPlains || biome == SurfaceBiome::SnowyTaiga;
}

bool isSandySurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::Desert;
}

bool isJungleSurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::Jungle || biome == SurfaceBiome::SparseJungle || biome == SurfaceBiome::BambooJungle;
}

bool isTemperateGrassSurfaceBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Plains:
    case SurfaceBiome::Forest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::Taiga:
        return true;
    default:
        return false;
    }
}
}  // namespace vibecraft::world::biomes

namespace vibecraft::world
{
const char* surfaceBiomeLabel(const SurfaceBiome biome)
{
    return biomes::biomeProfile(biome).label;
}
}  // namespace vibecraft::world
