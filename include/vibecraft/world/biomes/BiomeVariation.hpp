#pragma once

#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <cstdint>

namespace vibecraft::world::biomes
{
enum class WoodlandVariant : std::uint8_t
{
    None,
    WoodlandCore,
    FernGrove,
    MossyHollow,
    DryClearing,
    BirchPocket,
    RockyRise,
};

struct BiomeVariationSample
{
    WoodlandVariant primaryVariant = WoodlandVariant::None;
    double lushness = 0.5;
    double dryness = 0.5;
    double roughness = 0.5;
    double canopyDensity = 0.5;
};

[[nodiscard]] bool supportsWoodlandVariation(SurfaceBiome biome);

[[nodiscard]] BiomeVariationSample sampleBiomeVariation(
    SurfaceBiome biome,
    int worldX,
    int worldZ,
    std::uint32_t worldSeed);
}  // namespace vibecraft::world::biomes
