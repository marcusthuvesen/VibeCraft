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

constexpr BiomeProfile kSunflowerPlainsProfile{
    .biome = SurfaceBiome::SunflowerPlains,
    .label = "sunflower plains",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::SunflowerPlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kMeadowProfile{
    .biome = SurfaceBiome::Meadow,
    .label = "meadow",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::Meadow,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kSavannaProfile{
    .biome = SurfaceBiome::Savanna,
    .label = "savanna",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::Savanna,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kSavannaPlateauProfile{
    .biome = SurfaceBiome::SavannaPlateau,
    .label = "savanna plateau",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::Savanna,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kWindsweptSavannaProfile{
    .biome = SurfaceBiome::WindsweptSavanna,
    .label = "windswept savanna",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::Savanna,
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

constexpr BiomeProfile kFlowerForestProfile{
    .biome = SurfaceBiome::FlowerForest,
    .label = "flower forest",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Forest,
    .floraFamily = FloraGenerationFamily::FlowerForest,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kOldGrowthBirchForestProfile{
    .biome = SurfaceBiome::OldGrowthBirchForest,
    .label = "old growth birch forest",
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

constexpr BiomeProfile kOldGrowthSpruceTaigaProfile{
    .biome = SurfaceBiome::OldGrowthSpruceTaiga,
    .label = "old growth spruce taiga",
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

constexpr BiomeProfile kOldGrowthPineTaigaProfile{
    .biome = SurfaceBiome::OldGrowthPineTaiga,
    .label = "old growth pine taiga",
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

constexpr BiomeProfile kWindsweptHillsProfile{
    .biome = SurfaceBiome::WindsweptHills,
    .label = "windswept hills",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Plains,
    .floraFamily = FloraGenerationFamily::WindsweptHills,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kSnowySlopesProfile{
    .biome = SurfaceBiome::SnowySlopes,
    .label = "snowy slopes",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Stone,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::SnowyPlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = true,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kFrozenPeaksProfile{
    .biome = SurfaceBiome::FrozenPeaks,
    .label = "frozen peaks",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Stone,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::IcePlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = true,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kJaggedPeaksProfile{
    .biome = SurfaceBiome::JaggedPeaks,
    .label = "jagged peaks",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Stone,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::IcePlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = true,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kStonyPeaksProfile{
    .biome = SurfaceBiome::StonyPeaks,
    .label = "stony peaks",
    .canonicalSurfaceBlock = BlockType::Stone,
    .canonicalSubsurfaceBlock = BlockType::Stone,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::WindsweptHills,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
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

constexpr BiomeProfile kIcePlainsProfile{
    .biome = SurfaceBiome::IcePlains,
    .label = "ice plains",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::IcePlains,
    .usesSandStrata = false,
    .forested = false,
    .snowy = true,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kIceSpikePlainsProfile{
    .biome = SurfaceBiome::IceSpikePlains,
    .label = "ice spike plains",
    .canonicalSurfaceBlock = BlockType::SnowGrass,
    .canonicalSubsurfaceBlock = BlockType::Stone,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::IcePlains,
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

constexpr BiomeProfile kSwampProfile{
    .biome = SurfaceBiome::Swamp,
    .label = "swamp",
    .canonicalSurfaceBlock = BlockType::Grass,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::Forest,
    .floraFamily = FloraGenerationFamily::Swamp,
    .usesSandStrata = false,
    .forested = true,
    .snowy = false,
    .taiga = false,
    .jungle = false,
    .starterFriendly = false,
};

constexpr BiomeProfile kMushroomFieldProfile{
    .biome = SurfaceBiome::MushroomField,
    .label = "mushroom field",
    .canonicalSurfaceBlock = BlockType::MossBlock,
    .canonicalSubsurfaceBlock = BlockType::Dirt,
    .treeFamily = TreeGenerationFamily::None,
    .floraFamily = FloraGenerationFamily::MushroomField,
    .usesSandStrata = false,
    .forested = false,
    .snowy = false,
    .taiga = false,
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
    case SurfaceBiome::SunflowerPlains:
        return kSunflowerPlainsProfile;
    case SurfaceBiome::Meadow:
        return kMeadowProfile;
    case SurfaceBiome::Savanna:
        return kSavannaProfile;
    case SurfaceBiome::SavannaPlateau:
        return kSavannaPlateauProfile;
    case SurfaceBiome::WindsweptSavanna:
        return kWindsweptSavannaProfile;
    case SurfaceBiome::Forest:
        return kForestProfile;
    case SurfaceBiome::FlowerForest:
        return kFlowerForestProfile;
    case SurfaceBiome::OldGrowthBirchForest:
        return kOldGrowthBirchForestProfile;
    case SurfaceBiome::BirchForest:
        return kBirchForestProfile;
    case SurfaceBiome::DarkForest:
        return kDarkForestProfile;
    case SurfaceBiome::Taiga:
        return kTaigaProfile;
    case SurfaceBiome::OldGrowthSpruceTaiga:
        return kOldGrowthSpruceTaigaProfile;
    case SurfaceBiome::OldGrowthPineTaiga:
        return kOldGrowthPineTaigaProfile;
    case SurfaceBiome::WindsweptHills:
        return kWindsweptHillsProfile;
    case SurfaceBiome::SnowySlopes:
        return kSnowySlopesProfile;
    case SurfaceBiome::FrozenPeaks:
        return kFrozenPeaksProfile;
    case SurfaceBiome::JaggedPeaks:
        return kJaggedPeaksProfile;
    case SurfaceBiome::StonyPeaks:
        return kStonyPeaksProfile;
    case SurfaceBiome::SnowyPlains:
        return kSnowyPlainsProfile;
    case SurfaceBiome::IcePlains:
        return kIcePlainsProfile;
    case SurfaceBiome::IceSpikePlains:
        return kIceSpikePlainsProfile;
    case SurfaceBiome::SnowyTaiga:
        return kSnowyTaigaProfile;
    case SurfaceBiome::Swamp:
        return kSwampProfile;
    case SurfaceBiome::MushroomField:
        return kMushroomFieldProfile;
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
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::Swamp:
    case SurfaceBiome::Taiga:
    case SurfaceBiome::OldGrowthSpruceTaiga:
    case SurfaceBiome::OldGrowthPineTaiga:
    case SurfaceBiome::SnowyTaiga:
        return true;
    default:
        return false;
    }
}

bool isTaigaSurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::Taiga || biome == SurfaceBiome::SnowyTaiga
        || biome == SurfaceBiome::OldGrowthSpruceTaiga
        || biome == SurfaceBiome::OldGrowthPineTaiga;
}

bool isSnowySurfaceBiome(const SurfaceBiome biome)
{
    return biome == SurfaceBiome::SnowyPlains || biome == SurfaceBiome::SnowyTaiga
        || biome == SurfaceBiome::IcePlains
        || biome == SurfaceBiome::IceSpikePlains
        || biome == SurfaceBiome::SnowySlopes
        || biome == SurfaceBiome::FrozenPeaks
        || biome == SurfaceBiome::JaggedPeaks;
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
    case SurfaceBiome::SunflowerPlains:
    case SurfaceBiome::Meadow:
    case SurfaceBiome::Savanna:
    case SurfaceBiome::SavannaPlateau:
    case SurfaceBiome::WindsweptSavanna:
    case SurfaceBiome::Forest:
    case SurfaceBiome::FlowerForest:
    case SurfaceBiome::BirchForest:
    case SurfaceBiome::OldGrowthBirchForest:
    case SurfaceBiome::DarkForest:
    case SurfaceBiome::Taiga:
    case SurfaceBiome::OldGrowthSpruceTaiga:
    case SurfaceBiome::OldGrowthPineTaiga:
    case SurfaceBiome::WindsweptHills:
    case SurfaceBiome::Swamp:
    case SurfaceBiome::MushroomField:
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
