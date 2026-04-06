#include "WorldGenerationDetail.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/FloraGenerationTuning.hpp"
#include "vibecraft/world/biomes/FloraVariantRules.hpp"

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
constexpr std::uint32_t kSwampSpecialSeed = 0x1f63b4bfU;
constexpr std::uint32_t kMushroomFieldSeed = 0x2c74c5d0U;
constexpr std::uint32_t kTemperateForestPatchSeed = 0x2761b2aeU;
constexpr std::uint32_t kTemperateForestWetSeed = 0x3872c3bfU;
constexpr std::uint32_t kTemperateFernSeed = 0x4983d4c0U;
constexpr std::uint32_t kTemperateMushroomSeed = 0x5a94e5d1U;
constexpr std::uint32_t kTemperateGroundSeed = 0x6ba5f6e2U;
constexpr double kGrassTuftPatchScale = 28.0;
// Keep tuft cover intentionally sparse: broad patch masks plus low per-cell hit chance.
constexpr double kGrassTuftDensityScale = 0.10;
constexpr double kMushroomDensityScale = 0.5;

struct SurfaceSample
{
    int worldX = 0;
    int worldZ = 0;
    int surfaceY = 0;
    BlockType surfaceBlock = BlockType::Air;
    SurfaceBiome biome = SurfaceBiome::Forest;
    biomes::BiomeVariationSample variation{};
    biomes::BiomeTransitionSample transition{};
};

[[nodiscard]] BlockType pickTemperateFlowerBlock(const int worldX, const int worldZ, const double wetNoise)
{
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed);
    if (wetNoise > 0.52)
    {
        const auto& wetPool = biomes::temperateWetFlowerPool();
        return wetPool[h % wetPool.size()];
    }

    const auto& dryPool = biomes::temperateDryFlowerPool();
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
    outSample.variation = biomes::sampleBiomeVariation(outSample.biome, worldX, worldZ, terrainGenerator.worldSeed());
    outSample.transition = biomes::sampleBiomeTransition(
        outSample.biome,
        worldX,
        worldZ,
        [&](const int sampleX, const int sampleZ) { return terrainGenerator.surfaceBiomeAt(sampleX, sampleZ); });
    return true;
}

bool tryPopulateSandyDecor(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (sample.biome != SurfaceBiome::Desert || sample.surfaceBlock != BlockType::Sand)
    {
        return false;
    }

    if (noise::random01(sample.worldX, sample.worldZ, kCactusChanceSeed) < 0.010f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Cactus);
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kDeadBushChanceSeed) < 0.018f)
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

    const float tuftChance = sample.biome == SurfaceBiome::SnowyTaiga ? 0.008f : 0.005f;
    if (noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed + 101U) < tuftChance)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::FrostTuft);
        return true;
    }

    return false;
}

bool tryPopulateTemperateSurfaceDecor(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (!biomes::isForestSurfaceBiome(sample.biome) || !biomes::isTemperateForestDecorSurface(sample.surfaceBlock))
    {
        return false;
    }

    const biomes::FloraGenerationFamily floraFamily = biomes::biomeProfile(sample.biome).floraFamily;
    const biomes::TemperateForestDecorProfile decor =
        biomes::softenTemperateForestDecorForBiomeEdge(
            sample.biome,
            sample.transition,
            biomes::applyTemperateForestDecorVariant(sample.biome, sample.variation, biomes::temperateForestDecorProfile(sample.biome)));
    const double forestPatch = temperateForestPatchAt(sample.worldX, sample.worldZ);
    if (forestPatch < decor.patchEnterThreshold)
    {
        return false;
    }

    const double wetness = temperateForestWetAt(sample.worldX, sample.worldZ);
    const double fernRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateFernSeed);
    const double mushroomRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateMushroomSeed);
    const double groundRoll = noise::random01(sample.worldX, sample.worldZ, kTemperateGroundSeed);
    const bool denseForest = forestPatch >= decor.denseForestThreshold;
    const bool isDark = sample.biome == SurfaceBiome::DarkForest;

    if ((floraFamily == biomes::FloraGenerationFamily::Taiga || floraFamily == biomes::FloraGenerationFamily::SnowyTaiga)
        && denseForest
        && groundRoll < decor.taigaPodzolGroundRollMax)
    {
        if (fernRoll < decor.taigaFernOnPodzolChance && sample.biome == SurfaceBiome::Taiga)
        {
            chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
            return true;
        }
    }

    if (wetness > decor.mossWetnessMin && denseForest
        && groundRoll < (isDark ? decor.mossGroundRollDark : decor.mossGroundRollOther))
    {
        chunk.setBlock(localX, sample.surfaceY, localZ, BlockType::MossBlock);
        if (fernRoll < (isDark ? decor.mossFernChanceDark : decor.mossFernChanceOther))
        {
            chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
        }
        return true;
    }

    if (denseForest && wetness > decor.mushroomWetnessMin
        && mushroomRoll < (isDark ? decor.mushroomRollDark : decor.mushroomRollOther) * kMushroomDensityScale)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, pickMushroomBlock(sample.worldX, sample.worldZ));
        return true;
    }

    if (forestPatch > decor.fernForestPatchMin
        && fernRoll < (denseForest ? decor.fernChanceDense : decor.fernChanceSparse))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Fern);
        return true;
    }

    if (denseForest && wetness > decor.mossyCobbleWetnessMin && groundRoll < decor.mossyCobbleGroundRollMax)
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
    const biomes::GrassTuftTuning tuft =
        biomes::softenGrassTuftForBiomeEdge(
            sample.biome,
            sample.transition,
            biomes::applyGrassTuftVariant(sample.biome, sample.variation, biomes::grassTuftTuning(floraFamily)));
    const double tuftField = noise::fbmNoise2d(
        static_cast<double>(sample.worldX) + 13.0,
        static_cast<double>(sample.worldZ) - 37.0,
        kGrassTuftPatchScale,
        3,
        kGrassTuftNoiseSeed);
    const double tuftStrength = std::clamp(tuftField * 0.5 + 0.5, 0.0, 1.0);
    // Gate out weak-noise areas so grass reads as occasional natural clusters, not uniform salt-and-pepper.
    constexpr double kTuftPatchGate = 0.57;
    if (tuftStrength < kTuftPatchGate)
    {
        return;
    }
    const double gatedStrength = (tuftStrength - kTuftPatchGate) / (1.0 - kTuftPatchGate);
    const double scatter = noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed);
    const double tuftVariantRoll = noise::random01(sample.worldX, sample.worldZ, kGrassTuftScatterSeed + 23U);

    const BlockType tuftBlock = tuft.primaryFraction >= 1.0
        ? tuft.primaryTuft
        : (tuftVariantRoll < tuft.primaryFraction ? tuft.primaryTuft : tuft.secondaryTuft);

    const double tuftChance = std::clamp(
        (tuft.baseChance + gatedStrength * 0.010) * kGrassTuftDensityScale,
        0.0,
        0.008);
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
    bambooChance = biomes::softenSpecialFloraChanceForBiomeEdge(sample.biome, sample.transition, bambooChance);
    if (noise::random01(sample.worldX, sample.worldZ, kJungleSpecialSeed) < bambooChance)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Bamboo);
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kJungleSpecialSeed + 19U)
        < biomes::softenSpecialFloraChanceForBiomeEdge(sample.biome, sample.transition, 0.006f))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Melon);
        return true;
    }

    return false;
}

bool tryPopulateSwampSpecials(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (sample.biome != SurfaceBiome::Swamp || sample.surfaceBlock != BlockType::Grass)
    {
        return false;
    }

    if (noise::random01(sample.worldX, sample.worldZ, kSwampSpecialSeed)
        < biomes::softenSpecialFloraChanceForBiomeEdge(sample.biome, sample.transition, 0.17f))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Vines);
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kSwampSpecialSeed + 13U)
        < biomes::softenSpecialFloraChanceForBiomeEdge(sample.biome, sample.transition, 0.11f))
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::Water);
        return true;
    }

    return false;
}

bool tryPopulateMushroomFieldSpecials(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    if (sample.biome != SurfaceBiome::MushroomField || sample.surfaceBlock != BlockType::MossBlock)
    {
        return false;
    }

    if (noise::random01(sample.worldX, sample.worldZ, kMushroomFieldSeed) < 0.17f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, pickMushroomBlock(sample.worldX, sample.worldZ));
        return true;
    }
    if (noise::random01(sample.worldX, sample.worldZ, kMushroomFieldSeed + 7U) < 0.03f)
    {
        chunk.setBlock(localX, sample.surfaceY + 1, localZ, BlockType::BrownMushroom);
        chunk.setBlock(localX, sample.surfaceY + 2, localZ, BlockType::BrownMushroom);
        return true;
    }

    return false;
}

bool tryPopulateFlowersAndMushrooms(Chunk& chunk, const int localX, const int localZ, const SurfaceSample& sample)
{
    const biomes::FloraPatchTuning patch
        = biomes::softenFloraPatchForBiomeEdge(
            sample.biome,
            sample.transition,
            biomes::applyFloraPatchVariant(
                sample.biome,
                sample.variation,
                biomes::floraPatchTuning(biomes::biomeProfile(sample.biome).floraFamily)));
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
    bool tryMushroom = inMushroomPatch && mushJitter < patch.mushroomSpotChance * kMushroomDensityScale;

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
            if (tryPopulateMushroomFieldSpecials(chunk, localX, localZ, sample))
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
            if (tryPopulateSwampSpecials(chunk, localX, localZ, sample))
            {
                continue;
            }
            if (tryPopulateJungleSpecials(chunk, localX, localZ, sample))
            {
                continue;
            }
            (void)tryPopulateFlowersAndMushrooms(chunk, localX, localZ, sample);
        }
    }
}
}  // namespace vibecraft::world::detail
