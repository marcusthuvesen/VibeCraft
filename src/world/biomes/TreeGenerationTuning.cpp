#include "vibecraft/world/biomes/TreeGenerationTuning.hpp"

namespace vibecraft::world::biomes
{
TreeBiomeSettings treeBiomeSettingsForTreeFamily(const TreeGenerationFamily family)
{
    switch (family)
    {
    case TreeGenerationFamily::Jungle:
        return TreeBiomeSettings{
            .spawnChance = 0.22f,
            .minTrunkHeight = 10,
            .maxTrunkHeight = 22,
            .crownRadius = 4,
            .trunkBlock = BlockType::JungleLog,
            .crownBlock = BlockType::JungleLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::Jungle,
            .largeTreeChance = 0.30f,
            .largeTreeHeightBonus = 6,
            .largeTreeCrownRadiusBonus = 1,
        };
    case TreeGenerationFamily::SparseJungle:
        return TreeBiomeSettings{
            .spawnChance = 0.14f,
            .minTrunkHeight = 8,
            .maxTrunkHeight = 16,
            .crownRadius = 3,
            .trunkBlock = BlockType::JungleLog,
            .crownBlock = BlockType::JungleLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::Jungle,
            .largeTreeChance = 0.15f,
            .largeTreeHeightBonus = 4,
            .largeTreeCrownRadiusBonus = 1,
        };
    case TreeGenerationFamily::BambooJungle:
        return TreeBiomeSettings{
            .spawnChance = 0.26f,
            .minTrunkHeight = 11,
            .maxTrunkHeight = 22,
            .crownRadius = 4,
            .trunkBlock = BlockType::JungleLog,
            .crownBlock = BlockType::JungleLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::Jungle,
            .largeTreeChance = 0.26f,
            .largeTreeHeightBonus = 5,
            .largeTreeCrownRadiusBonus = 1,
        };
    case TreeGenerationFamily::SnowyTaiga:
        return TreeBiomeSettings{
            .spawnChance = 0.22f,
            .minTrunkHeight = 6,
            .maxTrunkHeight = 8,
            .crownRadius = 2,
            .trunkBlock = BlockType::SpruceLog,
            .crownBlock = BlockType::SpruceLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::Snowy,
        };
    case TreeGenerationFamily::Taiga:
        return TreeBiomeSettings{
            .spawnChance = 0.26f,
            .minTrunkHeight = 6,
            .maxTrunkHeight = 8,
            .crownRadius = 2,
            .trunkBlock = BlockType::SpruceLog,
            .crownBlock = BlockType::SpruceLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::Snowy,
        };
    case TreeGenerationFamily::Forest:
        return TreeBiomeSettings{
            .spawnChance = 0.51f,
            .minTrunkHeight = 4,
            .maxTrunkHeight = 7,
            .crownRadius = 2,
        };
    case TreeGenerationFamily::BirchForest:
        return TreeBiomeSettings{
            .spawnChance = 0.31f,
            .minTrunkHeight = 5,
            .maxTrunkHeight = 8,
            .crownRadius = 2,
            .trunkBlock = BlockType::BirchLog,
            .crownBlock = BlockType::BirchLeaves,
        };
    case TreeGenerationFamily::DarkForest:
        return TreeBiomeSettings{
            .spawnChance = 0.40f,
            .minTrunkHeight = 5,
            .maxTrunkHeight = 8,
            .crownRadius = 3,
            .trunkWidth = 1,
            .trunkBlock = BlockType::DarkOakLog,
            .crownBlock = BlockType::DarkOakLeaves,
            .canopyStyle = TreeBiomeSettings::CanopyStyle::BroadTemperate,
            .largeTreeChance = 0.34f,
            .largeTreeHeightBonus = 2,
            .largeTreeCrownRadiusBonus = 1,
        };
    case TreeGenerationFamily::Plains:
        return TreeBiomeSettings{
            .spawnChance = 0.08f,
            .minTrunkHeight = 4,
            .maxTrunkHeight = 6,
            .crownRadius = 2,
        };
    case TreeGenerationFamily::None:
    default:
        return TreeBiomeSettings{};
    }
}
}  // namespace vibecraft::world::biomes
