#include "vibecraft/world/biomes/FloraGenerationTuning.hpp"

namespace vibecraft::world::biomes
{
namespace
{
constexpr std::array<BlockType, kTemperateWetFlowerPoolSize> kWetFlowers{
    BlockType::Dandelion,
    BlockType::BlueOrchid,
    BlockType::BlueOrchid,
    BlockType::Allium,
    BlockType::OxeyeDaisy,
    BlockType::RedMushroom,
};

constexpr std::array<BlockType, kTemperateDryFlowerPoolSize> kDryFlowers{
    BlockType::Dandelion,
    BlockType::Dandelion,
    BlockType::Poppy,
    BlockType::Allium,
    BlockType::OxeyeDaisy,
};
}  // namespace

FloraPatchTuning floraPatchTuning(const FloraGenerationFamily family)
{
    switch (family)
    {
    case FloraGenerationFamily::Plains:
        return FloraPatchTuning{
            .flowerPatchMin = 0.62,
            .flowerSpotChance = 0.028,
            .mushroomPatchMin = 0.88,
            .mushroomSpotChance = 0.012,
        };
    case FloraGenerationFamily::SunflowerPlains:
        return FloraPatchTuning{
            .flowerPatchMin = 0.44,
            .flowerSpotChance = 0.060,
            .mushroomPatchMin = 0.96,
            .mushroomSpotChance = 0.004,
        };
    case FloraGenerationFamily::Meadow:
        return FloraPatchTuning{
            .flowerPatchMin = 0.42,
            .flowerSpotChance = 0.070,
            .mushroomPatchMin = 0.92,
            .mushroomSpotChance = 0.006,
        };
    case FloraGenerationFamily::Savanna:
        return FloraPatchTuning{
            .flowerPatchMin = 0.84,
            .flowerSpotChance = 0.012,
            .mushroomPatchMin = 0.97,
            .mushroomSpotChance = 0.002,
        };
    case FloraGenerationFamily::Forest:
        return FloraPatchTuning{
            .flowerPatchMin = 0.54,
            .flowerSpotChance = 0.033,
            .mushroomPatchMin = 0.64,
            .mushroomSpotChance = 0.036,
        };
    case FloraGenerationFamily::FlowerForest:
        return FloraPatchTuning{
            .flowerPatchMin = 0.32,
            .flowerSpotChance = 0.095,
            .mushroomPatchMin = 0.68,
            .mushroomSpotChance = 0.028,
        };
    case FloraGenerationFamily::BirchForest:
        return FloraPatchTuning{
            .flowerPatchMin = 0.50,
            .flowerSpotChance = 0.032,
            .mushroomPatchMin = 0.72,
            .mushroomSpotChance = 0.024,
        };
    case FloraGenerationFamily::DarkForest:
        return FloraPatchTuning{
            .flowerPatchMin = 0.94,
            .flowerSpotChance = 0.003,
            .mushroomPatchMin = 0.64,
            .mushroomSpotChance = 0.050,
        };
    case FloraGenerationFamily::Taiga:
        return FloraPatchTuning{
            .flowerPatchMin = 0.94,
            .flowerSpotChance = 0.002,
            .mushroomPatchMin = 0.72,
            .mushroomSpotChance = 0.028,
        };
    case FloraGenerationFamily::WindsweptHills:
        return FloraPatchTuning{
            .flowerPatchMin = 0.88,
            .flowerSpotChance = 0.008,
            .mushroomPatchMin = 0.95,
            .mushroomSpotChance = 0.004,
        };
    case FloraGenerationFamily::MushroomField:
        return FloraPatchTuning{
            .flowerPatchMin = 0.99,
            .flowerSpotChance = 0.001,
            .mushroomPatchMin = 0.34,
            .mushroomSpotChance = 0.11,
        };
    case FloraGenerationFamily::Jungle:
    case FloraGenerationFamily::SparseJungle:
    case FloraGenerationFamily::BambooJungle:
        return FloraPatchTuning{
            .flowerPatchMin = 0.52,
            .flowerSpotChance = 0.10,
            .mushroomPatchMin = 0.60,
            .mushroomSpotChance = 0.18,
        };
    case FloraGenerationFamily::SnowyPlains:
    case FloraGenerationFamily::IcePlains:
    case FloraGenerationFamily::SnowyTaiga:
    case FloraGenerationFamily::Desert:
    case FloraGenerationFamily::Swamp:
    default:
        return FloraPatchTuning{};
    }
}

GrassTuftTuning grassTuftTuning(const FloraGenerationFamily family)
{
    switch (family)
    {
    case FloraGenerationFamily::Plains:
        return GrassTuftTuning{
            .baseChance = 0.013,
            .primaryTuft = BlockType::GrassTuft,
            .secondaryTuft = BlockType::FlowerTuft,
            .primaryFraction = 0.94,
        };
    case FloraGenerationFamily::SunflowerPlains:
        return GrassTuftTuning{
            .baseChance = 0.011,
            .primaryTuft = BlockType::FlowerTuft,
            .secondaryTuft = BlockType::GrassTuft,
            .primaryFraction = 0.72,
        };
    case FloraGenerationFamily::Meadow:
        return GrassTuftTuning{
            .baseChance = 0.014,
            .primaryTuft = BlockType::FlowerTuft,
            .secondaryTuft = BlockType::CloverTuft,
            .primaryFraction = 0.68,
        };
    case FloraGenerationFamily::Savanna:
        return GrassTuftTuning{
            .baseChance = 0.012,
            .primaryTuft = BlockType::DryTuft,
            .secondaryTuft = BlockType::SparseTuft,
            .primaryFraction = 0.74,
        };
    case FloraGenerationFamily::Forest:
        return GrassTuftTuning{
            .baseChance = 0.021,
            .primaryTuft = BlockType::GrassTuft,
            .secondaryTuft = BlockType::CloverTuft,
            .primaryFraction = 0.78,
        };
    case FloraGenerationFamily::FlowerForest:
        return GrassTuftTuning{
            .baseChance = 0.020,
            .primaryTuft = BlockType::FlowerTuft,
            .secondaryTuft = BlockType::CloverTuft,
            .primaryFraction = 0.64,
        };
    case FloraGenerationFamily::BirchForest:
        return GrassTuftTuning{
            .baseChance = 0.023,
            .primaryTuft = BlockType::GrassTuft,
            .secondaryTuft = BlockType::CloverTuft,
            .primaryFraction = 0.76,
        };
    case FloraGenerationFamily::DarkForest:
        return GrassTuftTuning{
            .baseChance = 0.004,
            .primaryTuft = BlockType::SparseTuft,
            .secondaryTuft = BlockType::SparseTuft,
            .primaryFraction = 1.0,
        };
    case FloraGenerationFamily::Taiga:
        return GrassTuftTuning{
            .baseChance = 0.008,
            .primaryTuft = BlockType::SparseTuft,
            .secondaryTuft = BlockType::SparseTuft,
            .primaryFraction = 1.0,
        };
    case FloraGenerationFamily::WindsweptHills:
        return GrassTuftTuning{
            .baseChance = 0.007,
            .primaryTuft = BlockType::SparseTuft,
            .secondaryTuft = BlockType::DryTuft,
            .primaryFraction = 0.70,
        };
    case FloraGenerationFamily::Jungle:
    case FloraGenerationFamily::SparseJungle:
    case FloraGenerationFamily::BambooJungle:
        return GrassTuftTuning{
            .baseChance = 0.012,
            .primaryTuft = BlockType::LushTuft,
            .secondaryTuft = BlockType::LushTuft,
            .primaryFraction = 1.0,
        };
    case FloraGenerationFamily::SnowyPlains:
    case FloraGenerationFamily::IcePlains:
    case FloraGenerationFamily::SnowyTaiga:
        return GrassTuftTuning{
            .baseChance = 0.004,
            .primaryTuft = BlockType::FrostTuft,
            .secondaryTuft = BlockType::FrostTuft,
            .primaryFraction = 1.0,
        };
    case FloraGenerationFamily::Swamp:
        return GrassTuftTuning{
            .baseChance = 0.016,
            .primaryTuft = BlockType::LushTuft,
            .secondaryTuft = BlockType::SproutTuft,
            .primaryFraction = 0.70,
        };
    case FloraGenerationFamily::MushroomField:
        return GrassTuftTuning{
            .baseChance = 0.003,
            .primaryTuft = BlockType::SparseTuft,
            .secondaryTuft = BlockType::SparseTuft,
            .primaryFraction = 1.0,
        };
    case FloraGenerationFamily::Desert:
    default:
        return GrassTuftTuning{};
    }
}

TemperateForestDecorProfile temperateForestDecorProfile(const SurfaceBiome biome)
{
    TemperateForestDecorProfile profile{};
    switch (biome)
    {
    case SurfaceBiome::FlowerForest:
        profile.patchEnterThreshold = 0.34;
        profile.denseForestThreshold = 0.56;
        profile.mossGroundRollDark = 0.07;
        profile.mossGroundRollOther = 0.08;
        profile.fernForestPatchMin = 0.48;
        profile.fernChanceDense = 0.44;
        profile.fernChanceSparse = 0.31;
        profile.woodlandGroundPatchEnabled = true;
        profile.woodlandGroundPatchForestMin = 0.40;
        profile.woodlandGroundPatchRollMax = 0.026;
        break;
    case SurfaceBiome::DarkForest:
        profile.patchEnterThreshold = 0.44;
        profile.denseForestThreshold = 0.62;
        profile.woodlandGroundPatchEnabled = false;
        break;
    case SurfaceBiome::Forest:
        profile.patchEnterThreshold = 0.40;
        profile.denseForestThreshold = 0.65;
        profile.fernForestPatchMin = 0.55;
        profile.fernChanceDense = 0.38;
        profile.fernChanceSparse = 0.25;
        profile.woodlandGroundPatchEnabled = true;
        profile.woodlandGroundPatchForestMin = 0.50;
        profile.woodlandGroundPatchRollMax = 0.042;
        break;
    case SurfaceBiome::BirchForest:
        profile.patchEnterThreshold = 0.38;
        profile.denseForestThreshold = 0.63;
        profile.fernForestPatchMin = 0.52;
        profile.fernChanceDense = 0.40;
        profile.fernChanceSparse = 0.27;
        profile.woodlandGroundPatchEnabled = true;
        profile.woodlandGroundPatchForestMin = 0.48;
        profile.woodlandGroundPatchRollMax = 0.045;
        break;
    case SurfaceBiome::Taiga:
    case SurfaceBiome::SnowyTaiga:
    default:
        profile.patchEnterThreshold = 0.50;
        profile.denseForestThreshold = 0.72;
        profile.woodlandGroundPatchEnabled = false;
        break;
    }
    return profile;
}

const std::array<BlockType, kTemperateWetFlowerPoolSize>& temperateWetFlowerPool()
{
    return kWetFlowers;
}

const std::array<BlockType, kTemperateDryFlowerPoolSize>& temperateDryFlowerPool()
{
    return kDryFlowers;
}
}  // namespace vibecraft::world::biomes
