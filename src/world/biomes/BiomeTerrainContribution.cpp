#include "vibecraft/world/biomes/BiomeTerrainContribution.hpp"

#include "vibecraft/world/TerrainNoise.hpp"

#include <algorithm>

namespace vibecraft::world::biomes
{
namespace
{
constexpr std::uint32_t kSandyMesaNoiseSeed = 0xe58b49d2U;
constexpr std::uint32_t kSnowShelfNoiseSeed = 0x07ad6bf4U;
constexpr std::uint32_t kJungleShelfNoiseSeed = 0x29cf8d16U;
constexpr std::uint32_t kJungleValleyNoiseSeed = 0x3ae09e27U;
constexpr std::uint32_t kTemperateTerraceNoiseSeed = 0x4bf1af38U;
constexpr std::uint32_t kSandyDuneNoiseSeed = 0x5c02b049U;
constexpr std::uint32_t kSnowGlacierNoiseSeed = 0x6d13c15aU;
constexpr std::uint32_t kJungleButtressNoiseSeed = 0x7e24d26bU;
constexpr std::uint32_t kTemperateKnollNoiseSeed = 0x8f35e37cU;
constexpr std::uint32_t kTemperateClearingNoiseSeed = 0x9a46f48dU;
constexpr std::uint32_t kTemperateShelfNoiseSeed = 0xab57059eU;
constexpr std::uint32_t kSnowBasinNoiseSeed = 0xbc6816afU;
constexpr std::uint32_t kForestRiseNoiseSeed = 0xcd7927baU;
constexpr std::uint32_t kForestShelfNoiseSeed = 0xd98c31c7U;
constexpr std::uint32_t kTaigaShelfNoiseSeed = 0xde8a38cbU;
constexpr std::uint32_t kDarkForestNoiseSeed = 0xef9b49dcU;
constexpr std::uint32_t kWindsweptRidgeNoiseSeed = 0xf0ac5aedU;
constexpr std::uint32_t kSwampFlatNoiseSeed = 0xa1bd6bfeU;

[[nodiscard]] constexpr std::uint32_t mixedSeed(const std::uint32_t baseSeed, const std::uint32_t worldSeed)
{
    std::uint32_t mixed = baseSeed ^ (worldSeed + 0x9e3779b9U + (baseSeed << 6U) + (baseSeed >> 2U));
    mixed ^= mixed >> 16U;
    mixed *= 0x7feb352dU;
    mixed ^= mixed >> 15U;
    mixed *= 0x846ca68bU;
    mixed ^= mixed >> 16U;
    return mixed;
}

[[nodiscard]] double desertContribution(
    const double worldXd,
    const double worldZd,
    const double continents,
    const std::uint32_t worldSeed)
{
    const double duneNoise =
        noise::ridgeNoise2d(worldXd - 17.0, worldZd + 33.0, 84.0, mixedSeed(kSandyDuneNoiseSeed, worldSeed));
    const double duneField =
        noise::fbmNoise2d(worldXd + 121.0, worldZd - 77.0, 180.0, 2, mixedSeed(kSandyDuneNoiseSeed + 13U, worldSeed))
        * 2.0
        - 1.0;
    const double lowMesaNoise =
        noise::fbmNoise2d(worldXd - 181.0, worldZd + 129.0, 300.0, 2, mixedSeed(kSandyMesaNoiseSeed, worldSeed)) * 2.0
        - 1.0;
    const double dunes = std::max(0.0, duneNoise - 0.16) * (1.0 + std::max(0.0, duneField) * 1.4);
    const double lowMesas = std::max(0.0, lowMesaNoise - 0.60) * (0.8 + std::max(0.0, continents) * 1.0);
    return dunes + lowMesas;
}

[[nodiscard]] double snowyContribution(
    const double worldXd,
    const double worldZd,
    const double uplands,
    const std::uint32_t worldSeed,
    const double basinScale,
    const double ribScale)
{
    const double shelfNoise =
        noise::fbmNoise2d(worldXd + 143.0, worldZd - 39.0, 168.0, 3, mixedSeed(kSnowShelfNoiseSeed, worldSeed)) * 2.0
        - 1.0;
    const double glacierNoise =
        noise::fbmNoise2d(worldXd - 95.0, worldZd + 61.0, 88.0, 3, mixedSeed(kSnowGlacierNoiseSeed, worldSeed)) * 2.0
        - 1.0;
    const double basinNoise =
        noise::fbmNoise2d(worldXd + 57.0, worldZd + 117.0, 124.0, 2, mixedSeed(kSnowBasinNoiseSeed, worldSeed)) * 2.0
        - 1.0;
    const double rollingSnow = std::max(0.0, shelfNoise + 0.02) * (1.5 + std::max(0.0, uplands) * 1.2);
    const double frostRibs = std::max(0.0, glacierNoise - 0.12) * ribScale;
    const double frostBasins = std::clamp((0.08 - basinNoise) / 0.42, 0.0, 1.0) * basinScale;
    return rollingSnow + frostRibs - frostBasins;
}

[[nodiscard]] double jungleContribution(
    const double worldXd,
    const double worldZd,
    const double ridges,
    const std::uint32_t worldSeed,
    const double riseScale,
    const double valleyScale)
{
    const double shelfNoise =
        noise::ridgeNoise2d(worldXd - 141.0, worldZd + 63.0, 112.0, mixedSeed(kJungleShelfNoiseSeed, worldSeed));
    const double valleyNoise =
        noise::fbmNoise2d(worldXd + 95.0, worldZd - 173.0, 210.0, 3, mixedSeed(kJungleValleyNoiseSeed, worldSeed))
        * 2.0
        - 1.0;
    const double buttressNoise =
        noise::ridgeNoise2d(worldXd + 43.0, worldZd + 17.0, 84.0, mixedSeed(kJungleButtressNoiseSeed, worldSeed));
    const double jungleRise = std::max(0.0, shelfNoise - 0.22) * riseScale * (1.0 + std::max(0.0, ridges) * 0.62);
    const double shallowValleys = std::clamp((0.08 - valleyNoise) / 0.36, 0.0, 1.0) * valleyScale;
    const double buttresses = std::max(0.0, buttressNoise - 0.30) * 1.4;
    return jungleRise + buttresses - shallowValleys;
}

[[nodiscard]] double temperateContribution(
    const double worldXd,
    const double worldZd,
    const double uplands,
    const double ridges,
    const std::uint32_t worldSeed,
    const double knollScale,
    const double clearingScale,
    const double bluffScale)
{
    const double terraceNoise =
        noise::fbmNoise2d(worldXd + 11.0, worldZd + 157.0, 220.0, 3, mixedSeed(kTemperateTerraceNoiseSeed, worldSeed))
        * 2.0
        - 1.0;
    const double knollNoise =
        noise::fbmNoise2d(worldXd - 57.0, worldZd - 97.0, 128.0, 3, mixedSeed(kTemperateKnollNoiseSeed, worldSeed))
        * 2.0
        - 1.0;
    const double clearingNoise =
        noise::fbmNoise2d(worldXd - 71.0, worldZd + 93.0, 320.0, 2, mixedSeed(kTemperateClearingNoiseSeed, worldSeed))
        * 2.0
        - 1.0;
    const double bluffNoise =
        noise::ridgeNoise2d(worldXd + 37.0, worldZd - 141.0, 112.0, mixedSeed(kTemperateShelfNoiseSeed, worldSeed));
    const double valeNoise = noise::fbmNoise2d(
                                worldXd - 129.0,
                                worldZd + 63.0,
                                176.0,
                                2,
                                mixedSeed(kTemperateKnollNoiseSeed + 29U, worldSeed))
        * 2.0
        - 1.0;
    const double plainsLift =
        std::clamp((terraceNoise + 0.02) / 1.12, 0.0, 1.0) * (1.4 + std::max(0.0, uplands) * 1.2);
    const double knolls = std::max(0.0, knollNoise + 0.04) * knollScale * (1.1 + std::max(0.0, ridges) * 0.6);
    const double bluffs = std::max(0.0, bluffNoise - 0.66) * bluffScale * (0.3 + std::max(0.0, ridges) * 0.5);
    const double clearings = std::clamp((0.10 - clearingNoise) / 0.42, 0.0, 1.0) * clearingScale;
    const double vales = std::clamp((0.04 - valeNoise) / 0.40, 0.0, 1.0) * 0.25;
    return plainsLift + knolls + bluffs - vales - clearings;
}
}  // namespace

double biomeTerrainContribution(
    const SurfaceBiome biome,
    const int worldX,
    const int worldZ,
    const double continents,
    const double uplands,
    const double ridges,
    const std::uint32_t worldSeed)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);
    switch (biome)
    {
    case SurfaceBiome::Desert:
        return desertContribution(worldXd, worldZd, continents, worldSeed);
    case SurfaceBiome::Savanna:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.0, 0.22, 0.10)
            + desertContribution(worldXd, worldZd, continents, worldSeed) * 0.22;
    case SurfaceBiome::SavannaPlateau:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.05, 0.18, 0.36)
            + desertContribution(worldXd, worldZd, continents, worldSeed) * 0.16;
    case SurfaceBiome::WindsweptSavanna:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.08, 0.34, 0.68)
            + std::max(0.0, ridges - 0.20) * 3.0;
    case SurfaceBiome::SnowyPlains:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 1.6, 0.7);
    case SurfaceBiome::IcePlains:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 2.2, 0.40)
            + temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 0.7, 0.34, 0.05);
    case SurfaceBiome::IceSpikePlains:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 1.4, 1.30)
            + std::max(0.0, ridges - 0.08) * 2.8;
    case SurfaceBiome::SnowyTaiga:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 0.9, 0.55)
            + temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.0, 0.08, 0.18);
    case SurfaceBiome::SnowySlopes:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 1.8, 0.55)
            + temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 0.8, 0.42, 0.26);
    case SurfaceBiome::FrozenPeaks:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 1.6, 0.72)
            + std::max(0.0, ridges - 0.18) * 2.6;
    case SurfaceBiome::JaggedPeaks:
        return snowyContribution(worldXd, worldZd, uplands, worldSeed, 1.0, 1.05)
            + std::max(0.0, ridges - 0.12) * 4.2;
    case SurfaceBiome::StonyPeaks:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 0.8, 0.44, 0.72)
            + std::max(0.0, ridges - 0.10) * 2.1;
    case SurfaceBiome::Jungle:
        return jungleContribution(worldXd, worldZd, ridges, worldSeed, 2.6, 1.8);
    case SurfaceBiome::SparseJungle:
        return jungleContribution(worldXd, worldZd, ridges, worldSeed, 1.8, 1.4);
    case SurfaceBiome::BambooJungle:
        return jungleContribution(worldXd, worldZd, ridges, worldSeed, 3.0, 1.1);
    case SurfaceBiome::Forest:
        // Forests should read as rolling woodland with occasional shelves and dips, not a mostly flat carpet.
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 2.55, 0.07, 0.46)
            + std::max(
                   0.0,
                   noise::fbmNoise2d(worldXd - 39.0, worldZd + 81.0, 190.0, 3, mixedSeed(kForestRiseNoiseSeed, worldSeed))
                       * 2.0
                       - 1.0)
                * 0.74
            + std::max(
                   0.0,
                   noise::ridgeNoise2d(worldXd + 63.0, worldZd - 27.0, 118.0, mixedSeed(kForestShelfNoiseSeed, worldSeed))
                       - 0.52)
                * 0.90
            - std::clamp(
                  (0.12
                      - (noise::fbmNoise2d(
                              worldXd - 151.0,
                              worldZd + 47.0,
                              214.0,
                              2,
                              mixedSeed(kForestShelfNoiseSeed + 17U, worldSeed))
                          * 2.0
                          - 1.0))
                      / 0.34,
                  0.0,
                  1.0)
                * 0.38;
    case SurfaceBiome::FlowerForest:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.9, 0.06, 0.22);
    case SurfaceBiome::OldGrowthBirchForest:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 2.0, 0.09, 0.24);
    case SurfaceBiome::BirchForest:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.65, 0.18, 0.24);
    case SurfaceBiome::DarkForest:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.55, 0.05, 0.16)
            + std::max(
                   0.0,
                   noise::ridgeNoise2d(worldXd + 51.0, worldZd - 15.0, 136.0, mixedSeed(kDarkForestNoiseSeed, worldSeed))
                       - 0.36)
                * 0.85;
    case SurfaceBiome::Taiga:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.6, 0.14, 0.28)
            + std::max(
                   0.0,
                   noise::fbmNoise2d(worldXd + 71.0, worldZd - 49.0, 164.0, 3, mixedSeed(kTaigaShelfNoiseSeed, worldSeed))
                       * 2.0
                       - 1.0)
                * 0.45;
    case SurfaceBiome::OldGrowthSpruceTaiga:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.8, 0.10, 0.30)
            + std::max(
                   0.0,
                   noise::fbmNoise2d(worldXd + 91.0, worldZd - 71.0, 142.0, 3, mixedSeed(kTaigaShelfNoiseSeed + 7U, worldSeed))
                       * 2.0
                       - 1.0)
                * 0.62;
    case SurfaceBiome::OldGrowthPineTaiga:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.7, 0.12, 0.34)
            + std::max(
                   0.0,
                   noise::fbmNoise2d(worldXd - 113.0, worldZd + 39.0, 154.0, 3, mixedSeed(kTaigaShelfNoiseSeed + 11U, worldSeed))
                       * 2.0
                       - 1.0)
                * 0.54;
    case SurfaceBiome::WindsweptHills:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.4, 0.42, 0.64)
            + std::max(
                   0.0,
                   noise::ridgeNoise2d(worldXd - 23.0, worldZd + 51.0, 86.0, mixedSeed(kWindsweptRidgeNoiseSeed, worldSeed))
                       - 0.22)
                * 4.4;
    case SurfaceBiome::Swamp:
        return -std::clamp(
                   (noise::fbmNoise2d(worldXd + 61.0, worldZd - 49.0, 224.0, 2, mixedSeed(kSwampFlatNoiseSeed, worldSeed))
                        * 0.5
                        + 0.5)
                       * 2.5,
                   0.0,
                   2.5)
            + std::max(
                   0.0,
                   noise::fbmNoise2d(worldXd - 39.0, worldZd + 27.0, 108.0, 2, mixedSeed(kSwampFlatNoiseSeed + 19U, worldSeed))
                       * 2.0
                       - 1.0)
                * 0.65;
    case SurfaceBiome::MushroomField:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 0.55, 0.28, 0.06)
            - std::clamp(
                  (noise::fbmNoise2d(worldXd + 21.0, worldZd + 57.0, 188.0, 2, mixedSeed(kSwampFlatNoiseSeed + 37U, worldSeed))
                       * 0.5
                       + 0.5)
                      * 1.35,
                  0.0,
                  1.35);
    case SurfaceBiome::Meadow:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 0.95, 0.26, 0.12);
    case SurfaceBiome::SunflowerPlains:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.15, 0.30, 0.16);
    case SurfaceBiome::Plains:
    default:
        return temperateContribution(worldXd, worldZd, uplands, ridges, worldSeed, 1.2, 0.38, 0.18);
    }
}
}  // namespace vibecraft::world::biomes
