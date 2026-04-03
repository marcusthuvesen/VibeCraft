#include "WorldGenerationDetail.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::world::detail
{
namespace
{
constexpr std::uint32_t kFloraTypeSeed = 0x28fd99b6U;
constexpr std::uint32_t kFloraFlowerPatchNoiseSeed = 0x3af402d1U;
constexpr std::uint32_t kFloraMushroomPatchNoiseSeed = 0x51c903e8U;
constexpr std::uint32_t kFloraWetNoiseSeed = 0x62da14f9U;
constexpr std::uint32_t kFloraSpotSeed = 0x73eb260aU;
constexpr std::uint32_t kFloraSpotSeedB = 0x84fc371bU;
constexpr double kFloraFlowerPatchScale = 21.0;
constexpr double kFloraMushroomPatchScale = 29.0;
constexpr double kFloraWetScale = 46.0;
constexpr std::uint32_t kCactusChanceSeed = 0x4c1aafe3U;
constexpr std::uint32_t kDeadBushChanceSeed = 0x6db7d041U;
constexpr std::uint32_t kGrassTuftNoiseSeed = 0xe42e37b8U;
constexpr std::uint32_t kGrassTuftScatterSeed = 0xf53f48c9U;
constexpr std::uint32_t kJungleSpecialSeed = 0x1650a19dU;
constexpr std::uint32_t kTemperateForestPatchSeed = 0x2761b2aeU;
constexpr std::uint32_t kTemperateForestWetSeed = 0x3872c3bfU;
constexpr std::uint32_t kTemperateFernSeed = 0x4983d4c0U;
constexpr std::uint32_t kTemperateMushroomSeed = 0x5a94e5d1U;
constexpr std::uint32_t kTemperateGroundSeed = 0x6ba5f6e2U;
constexpr double kGrassTuftPatchScale = 28.0;

struct FloraPatchParams
{
    double flowerPatchMin = 1.0;
    double flowerSpotChance = 0.0;
    double mushroomPatchMin = 1.0;
    double mushroomSpotChance = 0.0;
};

struct SurfaceSample
{
    int worldX = 0;
    int worldZ = 0;
    int surfaceY = 0;
    BlockType surfaceBlock = BlockType::Air;
    SurfaceBiome biome = SurfaceBiome::Forest;
};

[[nodiscard]] FloraPatchParams floraPatchParamsForBiome(const SurfaceBiome biome)
{
    switch (biomes::biomeProfile(biome).floraFamily)
    {
    case biomes::FloraGenerationFamily::Plains:
        return FloraPatchParams{
            .flowerPatchMin = 0.62,
            .flowerSpotChance = 0.028,
            .mushroomPatchMin = 0.88,
            .mushroomSpotChance = 0.012,
        };
    case biomes::FloraGenerationFamily::Forest:
        return FloraPatchParams{
            .flowerPatchMin = 0.70,
            .flowerSpotChance = 0.018,
            .mushroomPatchMin = 0.76,
            .mushroomSpotChance = 0.026,
        };
    case biomes::FloraGenerationFamily::BirchForest:
        return FloraPatchParams{
            .flowerPatchMin = 0.64,
            .flowerSpotChance = 0.022,
            .mushroomPatchMin = 0.86,
            .mushroomSpotChance = 0.012,
        };
    case biomes::FloraGenerationFamily::DarkForest:
        return FloraPatchParams{
            .flowerPatchMin = 0.94,
            .flowerSpotChance = 0.003,
            .mushroomPatchMin = 0.64,
            .mushroomSpotChance = 0.050,
        };
    case biomes::FloraGenerationFamily::Taiga:
        return FloraPatchParams{
            .flowerPatchMin = 0.94,
            .flowerSpotChance = 0.002,
            .mushroomPatchMin = 0.72,
            .mushroomSpotChance = 0.028,
        };
    case biomes::FloraGenerationFamily::Jungle:
    case biomes::FloraGenerationFamily::SparseJungle:
    case biomes::FloraGenerationFamily::BambooJungle:
        return FloraPatchParams{
            .flowerPatchMin = 0.52,
            .flowerSpotChance = 0.10,
            .mushroomPatchMin = 0.60,
            .mushroomSpotChance = 0.18,
        };
    case biomes::FloraGenerationFamily::SnowyPlains:
    case biomes::FloraGenerationFamily::SnowyTaiga:
    case biomes::FloraGenerationFamily::Desert:
    default:
        return {};
    }
}

[[nodiscard]] BlockType pickTemperateFlowerBlock(const int worldX, const int worldZ, const double wetNoise)
{
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed);
    if (wetNoise > 0.52)
    {
        constexpr std::array<BlockType, 5> wetPool{
            BlockType::BlueOrchid,
            BlockType::BlueOrchid,
            BlockType::Allium,
            BlockType::OxeyeDaisy,
            BlockType::RedMushroom,
        };
        return wetPool[h % wetPool.size()];
    }

    constexpr std::array<BlockType, 4> dryPool{
        BlockType::Poppy,
        BlockType::Allium,
        BlockType::BlueOrchid,
        BlockType::OxeyeDaisy,
    };
    return dryPool[h % dryPool.size()];
}

[[nodiscard]] BlockType pickJungleFlowerBlock(const int worldX, const int worldZ)
{
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed + 17U);
    constexpr std::array<BlockType, 4> pool{
        BlockType::BlueOrchid,
        BlockType::BlueOrchid,
        BlockType::Poppy,
        BlockType::Allium,
    };
    return pool[h % pool.size()];
}

[[nodiscard]] BlockType pickMushroomBlock(const int worldX, const int worldZ)
{
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed + 29U);
    return (h & 1U) == 0U ? BlockType::BrownMushroom : BlockType::RedMushroom;
}

[[nodiscard]] double temperateForestPatchAt(const int worldX, const int worldZ)
{
    return std::clamp(
        noise::fbmNoise2d(
            static_cast<double>(worldX) + 19.0,
            static_cast<double>(worldZ) - 41.0,
            92.0,
            3,
            kTemperateForestPatchSeed)
            * 0.5
            + 0.5,
        0.0,
        1.0);
}

[[nodiscard]] double temperateForestWetAt(const int worldX, const int worldZ)
{
    return std::clamp(
        noise::fbmNoise2d(
            static_cast<double>(worldX) - 27.0,
            static_cast<double>(worldZ) + 13.0,
            54.0,
            2,
            kTemperateForestWetSeed)
            * 0.5
            + 0.5,
        0.0,
        1.0);
}

[[nodiscard]] bool isSurfaceGrassLike(const BlockType blockType)
{
    return blockType == BlockType::Grass
        || blockType == BlockType::JungleGrass
        || blockType == BlockType::SnowGrass;
}

[[nodiscard]] bool buildSurfaceSample(
    const Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int localX,
    const int localZ,
    SurfaceSample& outSample)
{
    const int worldX = coord.x * Chunk::kSize + localX;
    const int worldZ = coord.z * Chunk::kSize + localZ;
    const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
    if (surfaceY < kWorldMinY || surfaceY >= kWorldMaxY)
    {
        return false;
    }
    if (chunk.blockAt(localX, surfaceY + 1, localZ) != BlockType::Air)
    {
        return false;
    }

    outSample.worldX = worldX;
    outSample.worldZ = worldZ;
    outSample.surfaceY = surfaceY;
    outSample.surfaceBlock = chunk.blockAt(localX, surfaceY, localZ);
    outSample.biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
    return true;
}

bool tryPopulateSandyDecor(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (sample.biome != SurfaceBiome::Desert || sample.surfaceBlock != BlockType::Sand)
    {
        return false;
    }

    if (noise::random01(sample.worldX, sample.worldZ, kCactusChanceSeed) < 0.055f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Cactus);
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kDeadBushChanceSeed) < 0.09f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::DeadBush);
        return true;
    }

    return true;
}

bool tryPopulateSnowySurfaceDecor(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (!biomes::isSnowySurfaceBiome(sample.biome) || sample.surfaceBlock != BlockType::SnowGrass)
    {
        return false;
    }

    const float tuftChance = sample.biome == SurfaceBiome::SnowyTaiga ? 0.016f : 0.010f;
    if (noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed + 101U) < tuftChance)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::FrostTuft);
        return true;
    }

    return false;
}

bool tryPopulateTemperateSurfaceDecor(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (!biomes::isForestSurfaceBiome(sample.biome) || sample.surfaceBlock != BlockType::Grass)
    {
        return false;
    }

    const biomes::FloraGenerationFamily floraFamily = biomes::biomeProfile(sample.biome).floraFamily;
    const double forestPatch = temperateForestPatchAt(sample.worldX, sample.worldZ);
    const double patchThreshold = sample.biome == SurfaceBiome::DarkForest ? 0.48 : 0.56;
    if (forestPatch < patchThreshold)
    {
        return false;
    }

    const double wetness = temperateForestWetAt(sample.worldX, sample.worldZ);
    const double fernRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateFernSeed);
    const double mushroomRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateMushroomSeed);
    const double groundRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateGroundSeed);
    const bool denseForest = forestPatch >= (sample.biome == SurfaceBiome::DarkForest ? 0.68 : 0.76);

    if ((floraFamily == biomes::FloraGenerationFamily::Taiga || floraFamily == biomes::FloraGenerationFamily::SnowyTaiga)
        && denseForest
        && groundRoll < 0.15)
    {
        chunk.setBlock(
            localX,
            sample.surfaceY,
            localZ,
            wetness > 0.54 || forestPatch > 0.88 ? BlockType::Podzol : BlockType::CoarseDirt);
        if (fernRoll < 0.26 && sample.biome == SurfaceBiome::Taiga)
        {
            chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
        }
        return true;
    }

    if (wetness > 0.70 && denseForest && groundRoll < (sample.biome == SurfaceBiome::DarkForest ? 0.13 : 0.09))
    {
        chunk.setBlock(localX, sample.surfaceY, localZ, BlockType::MossBlock);
        if (fernRoll < (sample.biome == SurfaceBiome::DarkForest ? 0.18 : 0.32))
        {
            chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
        }
        return true;
    }

    if (denseForest && wetness > 0.48
        && mushroomRoll < (sample.biome == SurfaceBiome::DarkForest ? 0.20 : 0.10))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, pickMushroomBlock(sample.worldX, sample.worldZ));
        return true;
    }

    if (forestPatch > 0.64 && fernRoll < (denseForest ? 0.28 : 0.18))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
        return true;
    }

    if (denseForest && wetness > 0.58 && groundRoll < 0.025)
    {
        chunk.setBlock(localX, sample.surfaceY, localZ, BlockType::MossyCobblestone);
        return true;
    }

    return false;
}

void tryPopulateTufts(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (!isSurfaceGrassLike(sample.surfaceBlock))
    {
        return;
    }
    if (sample.biome == SurfaceBiome::Desert)
    {
        return;
    }

    const biomes::FloraGenerationFamily floraFamily = biomes::biomeProfile(sample.biome).floraFamily;
    const double tuftField = noise::fbmNoise2d(
        static_cast<double>(sample.worldX) + 13.0,
        static_cast<double>(sample.worldZ) - 37.0,
        kGrassTuftPatchScale,
        3,
        kGrassTuftNoiseSeed);
    const double tuftStrength = std::clamp(tuftField * 0.5 + 0.5, 0.0, 1.0);
    const double scatter = noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed);
    const double tuftVariantRoll = noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed + 23U);

    double baseChance = 0.0;
    BlockType tuftBlock = BlockType::GrassTuft;
    switch (floraFamily)
    {
    case biomes::FloraGenerationFamily::Plains:
        baseChance = 0.013;
        tuftBlock = tuftVariantRoll < 0.94 ? BlockType::GrassTuft : BlockType::FlowerTuft;
        break;
    case biomes::FloraGenerationFamily::Forest:
    case biomes::FloraGenerationFamily::BirchForest:
        baseChance = 0.010;
        tuftBlock = tuftVariantRoll < 0.85 ? BlockType::GrassTuft : BlockType::CloverTuft;
        break;
    case biomes::FloraGenerationFamily::DarkForest:
        baseChance = 0.004;
        tuftBlock = BlockType::SparseTuft;
        break;
    case biomes::FloraGenerationFamily::Taiga:
        baseChance = 0.008;
        tuftBlock = BlockType::SparseTuft;
        break;
    case biomes::FloraGenerationFamily::Jungle:
    case biomes::FloraGenerationFamily::SparseJungle:
    case biomes::FloraGenerationFamily::BambooJungle:
        baseChance = 0.012;
        tuftBlock = BlockType::LushTuft;
        break;
    case biomes::FloraGenerationFamily::SnowyPlains:
    case biomes::FloraGenerationFamily::SnowyTaiga:
        baseChance = 0.004;
        tuftBlock = BlockType::FrostTuft;
        break;
    case biomes::FloraGenerationFamily::Desert:
    default:
        return;
    }

    const double tuftChance = std::clamp(baseChance + tuftStrength * 0.03, 0.0, 0.06);
    if (scatter < tuftChance)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, tuftBlock);
    }
}

bool tryPopulateJungleSpecials(Chunk& chunk, const int localX, const int localZ, SurfaceSample& sample)
{
    const biomes::FloraGenerationFamily floraFamily = biomes::biomeProfile(sample.biome).floraFamily;
    if ((floraFamily != biomes::FloraGenerationFamily::Jungle
            && floraFamily != biomes::FloraGenerationFamily::SparseJungle
            && floraFamily != biomes::FloraGenerationFamily::BambooJungle)
        || sample.surfaceBlock != BlockType::JungleGrass)
    {
        return false;
    }

    float bambooChance = 0.020f;
    if (floraFamily == biomes::FloraGenerationFamily::SparseJungle)
    {
        bambooChance = 0.008f;
    }
    else if (floraFamily == biomes::FloraGenerationFamily::BambooJungle)
    {
        bambooChance = 0.065f;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kJungleSpecialSeed) < bambooChance)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Bamboo);
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kJungleSpecialSeed + 19U) < 0.006f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Melon);
        return true;
    }

    return false;
}

bool tryPopulateFlowersAndMushrooms(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    const FloraPatchParams patch = floraPatchParamsForBiome(sample.biome);
    const double flowerField = noise::fbmNoise2d(
        static_cast<double>(sample.worldX),
        static_cast<double>(sample.worldZ),
        kFloraFlowerPatchScale,
        3,
        kFloraFlowerPatchNoiseSeed);
    const double mushroomField = noise::fbmNoise2d(
        static_cast<double>(sample.worldX),
        static_cast<double>(sample.worldZ),
        kFloraMushroomPatchScale,
        3,
        kFloraMushroomPatchNoiseSeed);
    const double wetField = noise::fbmNoise2d(
        static_cast<double>(sample.worldX),
        static_cast<double>(sample.worldZ),
        kFloraWetScale,
        2,
        kFloraWetNoiseSeed);

    const bool inFlowerPatch = flowerField >= patch.flowerPatchMin;
    const bool inMushroomPatch = mushroomField >= patch.mushroomPatchMin;
    if (!inFlowerPatch && !inMushroomPatch)
    {
        return false;
    }

    const double flowerJitter = noise::random01(sample.worldX, sample.worldZ, kFloraSpotSeed);
    const double mushJitter = noise::random01(sample.worldX, sample.worldZ, kFloraSpotSeedB);
    bool tryFlower = inFlowerPatch && flowerJitter < patch.flowerSpotChance;
    bool tryMushroom = inMushroomPatch && mushJitter < patch.mushroomSpotChance;

    if (tryFlower && tryMushroom)
    {
        const double denomF = 1.0 - patch.flowerPatchMin;
        const double denomM = 1.0 - patch.mushroomPatchMin;
        const double flowerStrength = denomF > 1e-6 ? (flowerField - patch.flowerPatchMin) / denomF : 0.0;
        const double mushroomStrength = denomM > 1e-6 ? (mushroomField - patch.mushroomPatchMin) / denomM : 0.0;
        if (flowerStrength >= mushroomStrength)
        {
            tryMushroom = false;
        }
        else
        {
            tryFlower = false;
        }
    }

    if (tryMushroom)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, pickMushroomBlock(sample.worldX, sample.worldZ));
        return true;
    }
    if (!tryFlower || biomes::isSnowySurfaceBiome(sample.biome))
    {
        return false;
    }
    if (biomes::isJungleSurfaceBiome(sample.biome))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, pickJungleFlowerBlock(sample.worldX, sample.worldZ));
        return true;
    }

    chunk.setBlock(
        localX,
        sample.surfaceY + 1,
        localZ,
        pickTemperateFlowerBlock(sample.worldX, sample.worldZ, wetField));
    return true;
}
}  // namespace

void populateSurfaceFloraForChunk(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator)
{
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            SurfaceSample sample{};
            if (!buildSurfaceSample(chunk, coord, terrainGenerator, localX, localZ, sample))
            {
                continue;
            }
            if (tryPopulateSandyDecor(chunk, localX, localZ, sample))
            {
                continue;
            }

            if (tryPopulateSnowySurfaceDecor(chunk, localX, localZ, sample))
            {
                continue;
            }
            if (tryPopulateTemperateSurfaceDecor(chunk, localX, localZ, sample))
            {
                continue;
            }

            if (!isSurfaceGrassLike(sample.surfaceBlock))
            {
                continue;
            }

            tryPopulateTufts(chunk, localX, localZ, sample);
            if (tryPopulateJungleSpecials(chunk, localX, localZ, sample))
            {
                continue;
            }
            (void)tryPopulateFlowersAndMushrooms(chunk, localX, localZ, sample);
        }
    }
}
}  // namespace vibecraft::world::detail
