#include "WorldGeneration.hpp"
#include "WorldGenerationLandmarks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world
{
namespace
{
constexpr int kTreeCellSize = 6;
constexpr int kTreeMaxCrownRadius = 7;
constexpr int kTreeMinSurfaceY = 27;
constexpr std::uint32_t kTreeSeed = 0x0ea52f4dU;
constexpr std::uint32_t kTreeChanceSeed = 0x6f4a31deU;
constexpr std::uint32_t kTreeOffsetXSeed = 0x34b6f0e1U;
constexpr std::uint32_t kTreeOffsetZSeed = 0x5cd2a907U;
constexpr std::uint32_t kTreeShapeSeed = 0x72f1a4b3U;
constexpr std::uint32_t kFloraTypeSeed = 0x28fd99b6U;
constexpr std::uint32_t kFloraFlowerPatchNoiseSeed = 0x3af402d1U;
constexpr std::uint32_t kFloraMushroomPatchNoiseSeed = 0x51c903e8U;
constexpr std::uint32_t kFloraWetNoiseSeed = 0x62da14f9U;
constexpr std::uint32_t kFloraSpotSeed = 0x73eb260aU;
constexpr std::uint32_t kFloraSpotSeedB = 0x84fc371bU;
constexpr std::uint32_t kJungleFeatureNoiseSeed = 0x950d482cU;
constexpr std::uint32_t kJungleFeatureRollSeed = 0xa61e593dU;
constexpr std::uint32_t kJungleCanopyVineSeed = 0xb72f6a4eU;
constexpr std::uint32_t kJungleTrunkVineSeed = 0xc8407b5fU;
constexpr std::uint32_t kJungleCocoaSeed = 0xd9518c60U;
constexpr std::uint32_t kJungleDenseTreeSeed = 0xea629d71U;
constexpr std::uint32_t kJungleDenseTreeRollSeed = 0xfb73ae82U;
constexpr std::uint32_t kJungleMossPatchSeed = 0x0c84bf93U;
constexpr std::uint32_t kJungleStoneSeed = 0x1d95d0a4U;
constexpr std::uint32_t kCrystalFieldNoiseSeed = 0x2ea6e1b5U;
constexpr std::uint32_t kCrystalFieldRollSeed = 0x3fb7f2c6U;
constexpr std::uint32_t kFerriteFieldNoiseSeed = 0x4ac903d7U;
constexpr std::uint32_t kFerriteFieldRollSeed = 0x5bda14e8U;
constexpr double kFloraFlowerPatchScale = 21.0;
constexpr double kFloraMushroomPatchScale = 29.0;
constexpr double kFloraWetScale = 46.0;
constexpr double kJungleFeaturePatchScale = 41.0;
constexpr double kCrystalFieldScale = 53.0;
constexpr double kFerriteFieldScale = 61.0;
constexpr std::uint32_t kCactusChanceSeed = 0x4c1aafe3U;
constexpr std::uint32_t kDeadBushChanceSeed = 0x6db7d041U;
constexpr std::uint32_t kDustGravelNoiseSeed = 0x7ec8e152U;
constexpr std::uint32_t kDustGravelRollSeed = 0x8fd9f263U;
constexpr std::uint32_t kIceGravelNoiseSeed = 0xa0eaf374U;
constexpr std::uint32_t kIceGravelRollSeed = 0xb1fb0485U;
constexpr std::uint32_t kRegolithScatterNoiseSeed = 0xc20c1596U;
constexpr std::uint32_t kRegolithScatterRollSeed = 0xd31d26a7U;

struct TreeBiomeSettings
{
    float spawnChance = 0.0f;
    int minTrunkHeight = 4;
    int maxTrunkHeight = 6;
    int crownRadius = 2;
    BlockType trunkBlock = BlockType::TreeTrunk;
    BlockType crownBlock = BlockType::TreeCrown;
};

[[nodiscard]] constexpr std::size_t chunkStorageIndex(const int localX, const int y, const int localZ)
{
    const int localY = y - kWorldMinY;
    return static_cast<std::size_t>(localY * Chunk::kSize * Chunk::kSize + localZ * Chunk::kSize + localX);
}

[[nodiscard]] int floorDiv(const int value, const int divisor)
{
    return value >= 0 ? value / divisor : (value - (divisor - 1)) / divisor;
}

[[nodiscard]] bool canGrowTreeAt(
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY,
    const int trunkHeight,
    const int crownRadius,
    const int trunkWidth)
{
    if (surfaceY < kTreeMinSurfaceY)
    {
        return false;
    }

    for (int trunkZ = 0; trunkZ < trunkWidth; ++trunkZ)
    {
        for (int trunkX = 0; trunkX < trunkWidth; ++trunkX)
        {
            const BlockType surfaceBlock =
                terrainGenerator.blockTypeAt(worldX + trunkX, surfaceY, worldZ + trunkZ);
            if (surfaceBlock != BlockType::Grass
                && surfaceBlock != BlockType::JungleGrass
                && surfaceBlock != BlockType::SnowGrass)
            {
                return false;
            }
        }
    }

    const int canopyTopY = surfaceY + trunkHeight + crownRadius + 1;
    if (canopyTopY > kWorldMaxY)
    {
        return false;
    }

    for (int y = surfaceY + 1; y <= canopyTopY; ++y)
    {
        for (int trunkZ = 0; trunkZ < trunkWidth; ++trunkZ)
        {
            for (int trunkX = 0; trunkX < trunkWidth; ++trunkX)
            {
                if (terrainGenerator.blockTypeAt(worldX + trunkX, y, worldZ + trunkZ) != BlockType::Air)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

[[nodiscard]] TreeBiomeSettings treeBiomeSettingsForSurfaceBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::Jungle:
        return TreeBiomeSettings{
            .spawnChance = 0.64f,
            .minTrunkHeight = 11,
            .maxTrunkHeight = 18,
            .crownRadius = 5,
            .trunkBlock = BlockType::JungleTreeTrunk,
            .crownBlock = BlockType::JungleTreeCrown,
        };
    case SurfaceBiome::Snowy:
        return TreeBiomeSettings{
            .spawnChance = 0.08f,
            .minTrunkHeight = 5,
            .maxTrunkHeight = 8,
            .crownRadius = 3,
            .trunkBlock = BlockType::SnowTreeTrunk,
            .crownBlock = BlockType::SnowTreeCrown,
        };
    case SurfaceBiome::Sandy:
        return TreeBiomeSettings{
            .spawnChance = 0.0f,
            .minTrunkHeight = 4,
            .maxTrunkHeight = 6,
            .crownRadius = 2,
        };
    case SurfaceBiome::TemperateGrassland:
    default:
        return TreeBiomeSettings{
            .spawnChance = 0.24f,
            .minTrunkHeight = 6,
            .maxTrunkHeight = 10,
            .crownRadius = 3,
        };
    }
}

struct FloraPatchParams
{
    double flowerPatchMin = 1.0;
    double flowerSpotChance = 0.0;
    double mushroomPatchMin = 1.0;
    double mushroomSpotChance = 0.0;
};

[[nodiscard]] FloraPatchParams floraPatchParamsForBiome(const SurfaceBiome biome)
{
    switch (biome)
    {
    case SurfaceBiome::TemperateGrassland:
        return FloraPatchParams{
            .flowerPatchMin = 0.56,
            .flowerSpotChance = 0.14,
            .mushroomPatchMin = 0.66,
            .mushroomSpotChance = 0.20,
        };
    case SurfaceBiome::Jungle:
        return FloraPatchParams{
            .flowerPatchMin = 0.48,
            .flowerSpotChance = 0.18,
            .mushroomPatchMin = 0.53,
            .mushroomSpotChance = 0.28,
        };
    case SurfaceBiome::Snowy:
        return FloraPatchParams{
            .flowerPatchMin = 1.5,
            .flowerSpotChance = 0.0,
            .mushroomPatchMin = 0.56,
            .mushroomSpotChance = 0.16,
        };
    case SurfaceBiome::Sandy:
    default:
        return FloraPatchParams{};
    }
}

[[nodiscard]] BlockType pickTemperateFlowerBlock(const int worldX, const int worldZ, const double wetNoise)
{
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed);
    if (wetNoise > 0.52)
    {
        constexpr std::array<BlockType, 5> pool{
            BlockType::BlueOrchid,
            BlockType::BlueOrchid,
            BlockType::Allium,
            BlockType::OxeyeDaisy,
            BlockType::RedMushroom,
        };
        return pool[h % pool.size()];
    }
    constexpr std::array<BlockType, 4> pool{
        BlockType::Poppy,
        BlockType::Allium,
        BlockType::BlueOrchid,
        BlockType::OxeyeDaisy,
    };
    return pool[h % pool.size()];
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

void placeBlockIfInsideChunk(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType blockType)
{
    if (y < kWorldMinY || y > kWorldMaxY)
    {
        return;
    }

    const int minWorldX = coord.x * Chunk::kSize;
    const int minWorldZ = coord.z * Chunk::kSize;
    const int maxWorldX = minWorldX + Chunk::kSize - 1;
    const int maxWorldZ = minWorldZ + Chunk::kSize - 1;
    if (worldX < minWorldX || worldX > maxWorldX || worldZ < minWorldZ || worldZ > maxWorldZ)
    {
        return;
    }

    const int localX = worldToLocalCoord(worldX);
    const int localZ = worldToLocalCoord(worldZ);
    if (chunk.blockAt(localX, y, localZ) != BlockType::Air)
    {
        return;
    }

    chunk.setBlock(localX, y, localZ, blockType);
}

void placeTreeForColumn(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int treeX,
    const int treeZ,
    const TreeBiomeSettings& settings)
{
    const int surfaceY = terrainGenerator.surfaceHeightAt(treeX, treeZ);
    if (settings.spawnChance <= 0.0f)
    {
        return;
    }
    const std::uint32_t treeHash = noise::hashCoordinates(treeX, treeZ, kTreeSeed);
    int trunkHeight = settings.minTrunkHeight
        + static_cast<int>(treeHash
                            % static_cast<std::uint32_t>(settings.maxTrunkHeight - settings.minTrunkHeight + 1));
    int crownRadius = settings.crownRadius;
    int trunkWidth = 1;

    // Keep glow forest hearts visually huge while leaving the fringe lighter and easier to traverse.
    if (settings.crownBlock == BlockType::JungleTreeCrown
        && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x193U) < 0.10)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x319U);
        trunkHeight += 5 + static_cast<int>(giantHash % 5U);
        crownRadius += 2;
        if ((giantHash & 1U) == 0U)
        {
            trunkWidth = 2;
            crownRadius += 1;
        }
    }
    else if (settings.crownBlock == BlockType::TreeCrown
             && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x41BU) < 0.04)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x58DU);
        trunkHeight += 3 + static_cast<int>(giantHash % 3U);
        crownRadius += 1;
    }

    if (!canGrowTreeAt(terrainGenerator, treeX, treeZ, surfaceY, trunkHeight, crownRadius, trunkWidth))
    {
        return;
    }

    for (int y = surfaceY + 1; y <= surfaceY + trunkHeight; ++y)
    {
        for (int trunkZ = 0; trunkZ < trunkWidth; ++trunkZ)
        {
            for (int trunkX = 0; trunkX < trunkWidth; ++trunkX)
            {
                placeBlockIfInsideChunk(chunk, coord, treeX + trunkX, y, treeZ + trunkZ, settings.trunkBlock);
            }
        }
    }

    const int crownCenterX = treeX + (trunkWidth - 1) / 2;
    const int crownCenterZ = treeZ + (trunkWidth - 1) / 2;
    const int crownCenterY = surfaceY + trunkHeight;
    for (int dy = -crownRadius; dy <= 1; ++dy)
    {
        const int radius = dy <= -1 ? crownRadius : std::max(1, crownRadius - 1);
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (dx == 0 && dz == 0)
                {
                    continue;
                }

                const int crownX = crownCenterX + dx;
                const int crownY = crownCenterY + dy;
                const int crownZ = crownCenterZ + dz;
                const bool outerCorner = std::abs(dx) == radius && std::abs(dz) == radius;
                if (outerCorner
                    && noise::random01(crownX, crownZ, kTreeShapeSeed + static_cast<std::uint32_t>(crownY))
                        > (crownRadius >= 3 ? 0.26 : 0.4))
                {
                    continue;
                }
                placeBlockIfInsideChunk(chunk, coord, crownX, crownY, crownZ, settings.crownBlock);
            }
        }
    }

    placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY + 2, crownCenterZ, settings.crownBlock);
    if (crownRadius >= 3)
    {
        placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY + 3, crownCenterZ, settings.crownBlock);
    }

    if (settings.crownBlock == BlockType::JungleTreeCrown)
    {
        // Add hanging grove tendrils along canopy edges so breathable pockets feel dense and overgrown.
        for (int dy = -crownRadius; dy <= 1; ++dy)
        {
            const int radius = dy <= -1 ? crownRadius : std::max(1, crownRadius - 1);
            for (int dz = -radius; dz <= radius; ++dz)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (std::abs(dx) != radius && std::abs(dz) != radius)
                    {
                        continue;
                    }
                    const int vineX = crownCenterX + dx;
                    const int vineZ = crownCenterZ + dz;
                    const int vineStartY = crownCenterY + dy - 1;
                    const std::uint32_t ySeed = static_cast<std::uint32_t>(vineStartY - kWorldMinY);
                    if (noise::random01(vineX, vineZ, kJungleCanopyVineSeed + ySeed) > 0.16)
                    {
                        continue;
                    }
                    const std::uint32_t lengthHash =
                        noise::hashCoordinates(vineX, vineZ, kJungleCanopyVineSeed + ySeed + 17U);
                    const int vineLength = 2 + static_cast<int>(lengthHash % 5U);
                    for (int i = 0; i < vineLength; ++i)
                    {
                        placeBlockIfInsideChunk(chunk, coord, vineX, vineStartY - i, vineZ, BlockType::Vines);
                    }
                }
            }
        }

        const std::array<std::array<int, 2>, 4> kTrunkSides{{
            {{trunkWidth, 0}},
            {{-1, 0}},
            {{0, trunkWidth}},
            {{0, -1}},
        }};
        const int trunkVineStartY = surfaceY + trunkHeight / 2;
        for (int y = trunkVineStartY; y <= surfaceY + trunkHeight; ++y)
        {
            const std::uint32_t ySeed = static_cast<std::uint32_t>(y - kWorldMinY);
            for (const auto& side : kTrunkSides)
            {
                const int sideX = crownCenterX + side[0];
                const int sideZ = crownCenterZ + side[1];
                if (noise::random01(sideX, sideZ, kJungleTrunkVineSeed + ySeed) < 0.045)
                {
                    const std::uint32_t vineLenHash =
                        noise::hashCoordinates(sideX, sideZ, kJungleTrunkVineSeed + ySeed + 23U);
                    const int vineLength = 2 + static_cast<int>(vineLenHash % 3U);
                    for (int i = 0; i < vineLength; ++i)
                    {
                        placeBlockIfInsideChunk(chunk, coord, sideX, y - i, sideZ, BlockType::Vines);
                    }
                }
                if (noise::random01(sideX, sideZ, kJungleCocoaSeed + ySeed) < 0.045)
                {
                    placeBlockIfInsideChunk(chunk, coord, sideX, y, sideZ, BlockType::CocoaPod);
                }
            }
        }
    }
}

void populateTreesForChunk(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator)
{
    const int chunkWorldMinX = coord.x * Chunk::kSize;
    const int chunkWorldMinZ = coord.z * Chunk::kSize;
    const int sampleMinX = chunkWorldMinX - kTreeMaxCrownRadius;
    const int sampleMinZ = chunkWorldMinZ - kTreeMaxCrownRadius;
    const int sampleMaxX = chunkWorldMinX + Chunk::kSize - 1 + kTreeMaxCrownRadius;
    const int sampleMaxZ = chunkWorldMinZ + Chunk::kSize - 1 + kTreeMaxCrownRadius;

    const int minCellX = floorDiv(sampleMinX, kTreeCellSize);
    const int maxCellX = floorDiv(sampleMaxX, kTreeCellSize);
    const int minCellZ = floorDiv(sampleMinZ, kTreeCellSize);
    const int maxCellZ = floorDiv(sampleMaxZ, kTreeCellSize);

    for (int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ)
    {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX)
        {
            const int treeX = cellX * kTreeCellSize
                + static_cast<int>(noise::hashCoordinates(cellX, cellZ, kTreeOffsetXSeed)
                                    % static_cast<std::uint32_t>(kTreeCellSize));
            const int treeZ = cellZ * kTreeCellSize
                + static_cast<int>(noise::hashCoordinates(cellX, cellZ, kTreeOffsetZSeed)
                                    % static_cast<std::uint32_t>(kTreeCellSize));
            const SurfaceBiome biome = terrainGenerator.surfaceBiomeAt(treeX, treeZ);
            const TreeBiomeSettings settings = treeBiomeSettingsForSurfaceBiome(biome);
            if (noise::random01(cellX, cellZ, kTreeChanceSeed) > settings.spawnChance)
            {
                continue;
            }
            placeTreeForColumn(chunk, coord, terrainGenerator, treeX, treeZ, settings);

            if (biome == SurfaceBiome::Jungle)
            {
                const std::uint32_t denseHash = noise::hashCoordinates(cellX, cellZ, kJungleDenseTreeSeed);
                const int offsetX = static_cast<int>(denseHash % 5U) - 2;
                const int offsetZ = static_cast<int>((denseHash / 5U) % 5U) - 2;
                if ((offsetX != 0 || offsetZ != 0)
                    && noise::random01(cellX, cellZ, kJungleDenseTreeRollSeed) < 0.45)
                {
                    const int denseTreeX = treeX + offsetX;
                    const int denseTreeZ = treeZ + offsetZ;
                    const SurfaceBiome denseBiome = terrainGenerator.surfaceBiomeAt(denseTreeX, denseTreeZ);
                    const TreeBiomeSettings denseSettings = treeBiomeSettingsForSurfaceBiome(denseBiome);
                    if (denseBiome == SurfaceBiome::Jungle && denseSettings.spawnChance > 0.0f)
                    {
                        placeTreeForColumn(chunk, coord, terrainGenerator, denseTreeX, denseTreeZ, denseSettings);
                    }
                }
            }
        }
    }
}

void populateSurfaceFloraForChunk(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator)
{
    const int chunkWorldMinX = coord.x * Chunk::kSize;
    const int chunkWorldMinZ = coord.z * Chunk::kSize;
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = chunkWorldMinX + localX;
            const int worldZ = chunkWorldMinZ + localZ;
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (surfaceY < kWorldMinY || surfaceY >= kWorldMaxY)
            {
                continue;
            }
            if (chunk.blockAt(localX, surfaceY + 1, localZ) != BlockType::Air)
            {
                continue;
            }

            BlockType surfaceBlock = chunk.blockAt(localX, surfaceY, localZ);
            const SurfaceBiome biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome == SurfaceBiome::Sandy)
            {
                if (surfaceBlock == BlockType::Sand
                    && noise::random01(worldX, worldZ, kCactusChanceSeed) < 0.055f)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::Cactus);
                }
                else if (
                    surfaceBlock == BlockType::Sand
                    && noise::random01(worldX, worldZ, kDeadBushChanceSeed) < 0.09f)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::DeadBush);
                }
                else if (surfaceBlock == BlockType::Sand)
                {
                    const double dustGravelField = noise::fbmNoise2d(
                        static_cast<double>(worldX) + 31.0,
                        static_cast<double>(worldZ) - 17.0,
                        49.0,
                        2,
                        kDustGravelNoiseSeed);
                    if (dustGravelField > 0.76
                        && noise::random01(worldX, worldZ, kDustGravelRollSeed) < 0.095f)
                    {
                        chunk.setBlock(localX, surfaceY, localZ, BlockType::Gravel);
                    }
                }
                continue;
            }

            if (biome == SurfaceBiome::Snowy && surfaceBlock == BlockType::SnowGrass)
            {
                const double iceGravelField = noise::fbmNoise2d(
                    static_cast<double>(worldX) + 23.0,
                    static_cast<double>(worldZ) - 41.0,
                    41.0,
                    2,
                    kIceGravelNoiseSeed);
                if (iceGravelField > 0.82
                    && noise::random01(worldX, worldZ, kIceGravelRollSeed) < 0.058f)
                {
                    chunk.setBlock(localX, surfaceY, localZ, BlockType::Gravel);
                    continue;
                }
            }

            if (biome != SurfaceBiome::Jungle)
            {
                const double crystalField = noise::fbmNoise2d(
                    static_cast<double>(worldX) - 11.0,
                    static_cast<double>(worldZ) + 7.0,
                    kCrystalFieldScale,
                    2,
                    kCrystalFieldNoiseSeed);
                const float crystalChance = biome == SurfaceBiome::Snowy ? 0.16f : 0.05f;
                if (crystalField > 0.78
                    && noise::random01(worldX, worldZ, kCrystalFieldRollSeed) < crystalChance)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::Glowstone);
                    continue;
                }
            }

            if (biome == SurfaceBiome::TemperateGrassland && surfaceBlock == BlockType::Grass)
            {
                const double ferriteField = noise::fbmNoise2d(
                    static_cast<double>(worldX) + 17.0,
                    static_cast<double>(worldZ) - 29.0,
                    kFerriteFieldScale,
                    2,
                    kFerriteFieldNoiseSeed);
                if (ferriteField > 0.78
                    && noise::random01(worldX, worldZ, kFerriteFieldRollSeed) < 0.075f)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::IronOre);
                    continue;
                }
                const double regolithScatter = noise::fbmNoise2d(
                    static_cast<double>(worldX) - 101.0,
                    static_cast<double>(worldZ) + 77.0,
                    31.0,
                    2,
                    kRegolithScatterNoiseSeed);
                if (regolithScatter > 0.84
                    && noise::random01(worldX, worldZ, kRegolithScatterRollSeed) < 0.036f)
                {
                    chunk.setBlock(localX, surfaceY, localZ, BlockType::Gravel);
                    continue;
                }
            }

            if (surfaceBlock != BlockType::Grass
                && surfaceBlock != BlockType::JungleGrass
                && surfaceBlock != BlockType::SnowGrass)
            {
                continue;
            }

            const FloraPatchParams patch = floraPatchParamsForBiome(biome);

            if (biome == SurfaceBiome::Jungle)
            {
                const double mossPatch = noise::fbmNoise2d(
                    static_cast<double>(worldX) + 19.0,
                    static_cast<double>(worldZ) - 13.0,
                    34.0,
                    2,
                    kJungleMossPatchSeed);
                if (surfaceBlock == BlockType::JungleGrass && mossPatch > 0.58)
                {
                    chunk.setBlock(localX, surfaceY, localZ, BlockType::MossBlock);
                    surfaceBlock = BlockType::MossBlock;
                }

                const double jungleFeature = noise::fbmNoise2d(
                    static_cast<double>(worldX),
                    static_cast<double>(worldZ),
                    kJungleFeaturePatchScale,
                    2,
                    kJungleFeatureNoiseSeed);
                if (jungleFeature > 0.76
                    && noise::random01(worldX, worldZ, kJungleStoneSeed) < 0.10)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::MossyCobblestone);
                    continue;
                }
                if (jungleFeature > 0.62
                    && noise::random01(worldX, worldZ, kJungleFeatureRollSeed) < 0.28)
                {
                    constexpr std::array<BlockType, 5> kJungleLargeFlora{
                        BlockType::Vines,
                        BlockType::Bamboo,
                        BlockType::Bamboo,
                        BlockType::CocoaPod,
                        BlockType::Melon,
                    };
                    const std::uint32_t pick = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed + 41U);
                    const BlockType floraBlock = kJungleLargeFlora[pick % kJungleLargeFlora.size()];
                    if (isNaturalDecorationBlock(floraBlock))
                    {
                        if (floraBlock == BlockType::Bamboo)
                        {
                            const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kFloraTypeSeed + 83U);
                            const int bambooHeight = 4 + static_cast<int>(h % 10U);
                            for (int bambooY = surfaceY + 1; bambooY <= surfaceY + bambooHeight; ++bambooY)
                            {
                                if (bambooY > kWorldMaxY)
                                {
                                    break;
                                }
                                if (chunk.blockAt(localX, bambooY, localZ) != BlockType::Air)
                                {
                                    break;
                                }
                                chunk.setBlock(localX, bambooY, localZ, BlockType::Bamboo);
                            }
                        }
                        else
                        {
                            chunk.setBlock(localX, surfaceY + 1, localZ, floraBlock);
                        }
                    }
                    continue;
                }
            }

            const double flowerField = noise::fbmNoise2d(
                static_cast<double>(worldX),
                static_cast<double>(worldZ),
                kFloraFlowerPatchScale,
                3,
                kFloraFlowerPatchNoiseSeed);
            const double mushroomField = noise::fbmNoise2d(
                static_cast<double>(worldX),
                static_cast<double>(worldZ),
                kFloraMushroomPatchScale,
                3,
                kFloraMushroomPatchNoiseSeed);
            const double wetField = noise::fbmNoise2d(
                static_cast<double>(worldX),
                static_cast<double>(worldZ),
                kFloraWetScale,
                2,
                kFloraWetNoiseSeed);

            const bool inFlowerPatch = flowerField >= patch.flowerPatchMin;
            const bool inMushroomPatch = mushroomField >= patch.mushroomPatchMin;
            if (!inFlowerPatch && !inMushroomPatch)
            {
                continue;
            }

            const double flowerJitter = noise::random01(worldX, worldZ, kFloraSpotSeed);
            const double mushJitter = noise::random01(worldX, worldZ, kFloraSpotSeedB);
            bool tryFlower = inFlowerPatch && flowerJitter < patch.flowerSpotChance;
            bool tryMushroom = inMushroomPatch && mushJitter < patch.mushroomSpotChance;

            if (tryFlower && tryMushroom)
            {
                const double denomF = 1.0 - patch.flowerPatchMin;
                const double denomM = 1.0 - patch.mushroomPatchMin;
                const double flowerStrength = denomF > 1e-6
                    ? (flowerField - patch.flowerPatchMin) / denomF
                    : 0.0;
                const double mushroomStrength = denomM > 1e-6
                    ? (mushroomField - patch.mushroomPatchMin) / denomM
                    : 0.0;
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
                chunk.setBlock(localX, surfaceY + 1, localZ, pickMushroomBlock(worldX, worldZ));
                continue;
            }
            if (!tryFlower)
            {
                continue;
            }

            if (biome == SurfaceBiome::Snowy)
            {
                continue;
            }
            if (biome == SurfaceBiome::Jungle)
            {
                const BlockType floraBlock = pickJungleFlowerBlock(worldX, worldZ);
                chunk.setBlock(localX, surfaceY + 1, localZ, floraBlock);
                continue;
            }

            const BlockType floraBlock = pickTemperateFlowerBlock(worldX, worldZ, wetField);
            chunk.setBlock(localX, surfaceY + 1, localZ, floraBlock);
        }
    }
}
}  // namespace

void populateChunkFromTerrain(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator)
{
    std::array<BlockType, kWorldHeight> columnBlocks{};
    auto& storage = chunk.mutableBlockStorage();
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = coord.x * Chunk::kSize + localX;
            const int worldZ = coord.z * Chunk::kSize + localZ;
            terrainGenerator.fillColumn(worldX, worldZ, columnBlocks.data());
            for (int y = kWorldMinY; y <= kWorldMaxY; ++y)
            {
                BlockType blockType = columnBlocks[y - kWorldMinY];
                if (!isNaturalTerrainBlock(blockType))
                {
                    blockType = BlockType::Air;
                }
                storage[chunkStorageIndex(localX, y, localZ)] = blockType;
            }
        }
    }

    populateTreesForChunk(chunk, coord, terrainGenerator);
    populateBiomeLandmarksForChunk(chunk, coord, terrainGenerator);
    populateSurfaceFloraForChunk(chunk, coord, terrainGenerator);
}
}  // namespace vibecraft::world
