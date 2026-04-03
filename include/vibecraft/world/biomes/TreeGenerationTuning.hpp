#pragma once

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::world::biomes
{
struct TreeBiomeSettings
{
    enum class CanopyStyle : std::uint8_t
    {
        Temperate,
        Snowy,
        Jungle,
    };

    float spawnChance = 0.0f;
    int minTrunkHeight = 4;
    int maxTrunkHeight = 6;
    int crownRadius = 2;
    BlockType trunkBlock = BlockType::OakLog;
    BlockType crownBlock = BlockType::OakLeaves;
    CanopyStyle canopyStyle = CanopyStyle::Temperate;
};

[[nodiscard]] TreeBiomeSettings treeBiomeSettingsForTreeFamily(TreeGenerationFamily family);

[[nodiscard]] inline TreeBiomeSettings treeBiomeSettingsForSurfaceBiome(const SurfaceBiome biome)
{
    return treeBiomeSettingsForTreeFamily(biomeProfile(biome).treeFamily);
}
}  // namespace vibecraft::world::biomes
