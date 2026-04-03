#pragma once

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/SurfaceBiome.hpp"

#include <array>
#include <cstddef>

namespace vibecraft::world::biomes
{
/// Flower / mushroom patch noise thresholds and per-cell try chances (see `tryPopulateFlowersAndMushrooms`).
struct FloraPatchTuning
{
    double flowerPatchMin = 1.0;
    double flowerSpotChance = 0.0;
    double mushroomPatchMin = 1.0;
    double mushroomSpotChance = 0.0;
};

[[nodiscard]] FloraPatchTuning floraPatchTuning(FloraGenerationFamily family);

/// Short grass / clover / sparse tuft scatter on grass-like surfaces.
struct GrassTuftTuning
{
    double baseChance = 0.0;
    BlockType primaryTuft = BlockType::GrassTuft;
    BlockType secondaryTuft = BlockType::GrassTuft;
    /// Variant roll in [0, 1) compares to this; values >= 1.0 always pick `primaryTuft`.
    double primaryFraction = 1.0;
};

[[nodiscard]] GrassTuftTuning grassTuftTuning(FloraGenerationFamily family);

/// Temperate woodland floor: moss, ferns, mushrooms, podzol strips — all tunables in one place.
struct TemperateForestDecorProfile
{
    double patchEnterThreshold = 0.5;
    double denseForestThreshold = 0.72;

    double taigaPodzolGroundRollMax = 0.15;
    double taigaPodzolWetnessPreferPodzol = 0.54;
    double taigaPodzolForestPatchBoost = 0.88;
    double taigaFernOnPodzolChance = 0.26;

    double mossWetnessMin = 0.70;
    double mossGroundRollDark = 0.13;
    double mossGroundRollOther = 0.11;
    double mossFernChanceDark = 0.18;
    double mossFernChanceOther = 0.36;

    double mushroomWetnessMin = 0.48;
    double mushroomRollDark = 0.22;
    double mushroomRollOther = 0.14;

    double fernForestPatchMin = 0.58;
    double fernChanceDense = 0.36;
    double fernChanceSparse = 0.24;

    bool woodlandGroundPatchEnabled = false;
    double woodlandGroundPatchForestMin = 0.52;
    double woodlandGroundPatchRollMax = 0.035;
    double woodlandGroundPatchWetnessPodzol = 0.55;

    double mossyCobbleWetnessMin = 0.58;
    double mossyCobbleGroundRollMax = 0.025;
};

[[nodiscard]] TemperateForestDecorProfile temperateForestDecorProfile(SurfaceBiome biome);

inline constexpr std::size_t kTemperateWetFlowerPoolSize = 6;
inline constexpr std::size_t kTemperateDryFlowerPoolSize = 5;

[[nodiscard]] const std::array<BlockType, kTemperateWetFlowerPoolSize>& temperateWetFlowerPool();
[[nodiscard]] const std::array<BlockType, kTemperateDryFlowerPoolSize>& temperateDryFlowerPool();
}  // namespace vibecraft::world::biomes
