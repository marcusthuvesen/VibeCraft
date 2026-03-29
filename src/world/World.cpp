#include "vibecraft/world/World.hpp"

#include <array>
#include <algorithm>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"
#include "vibecraft/world/WorldSerializer.hpp"

namespace vibecraft::world
{
std::uint32_t World::generationSeed() const
{
    return generationSeed_;
}

void World::setGenerationSeed(const std::uint32_t generationSeed)
{
    generationSeed_ = generationSeed;
}

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
constexpr double kFloraFlowerPatchScale = 21.0;
constexpr double kFloraMushroomPatchScale = 29.0;
constexpr double kFloraWetScale = 46.0;
constexpr double kJungleFeaturePatchScale = 41.0;
constexpr std::uint32_t kCactusChanceSeed = 0x4c1aafe3U;
constexpr std::uint32_t kDeadBushChanceSeed = 0x6db7d041U;

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
            .spawnChance = 0.70f,
            .minTrunkHeight = 9,
            .maxTrunkHeight = 14,
            .crownRadius = 4,
            .trunkBlock = BlockType::JungleTreeTrunk,
            .crownBlock = BlockType::JungleTreeCrown,
        };
    case SurfaceBiome::Snowy:
        return TreeBiomeSettings{
            .spawnChance = 0.08f,
            .minTrunkHeight = 4,
            .maxTrunkHeight = 6,
            .crownRadius = 2,
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
            .spawnChance = 0.28f,
            .minTrunkHeight = 4,
            .maxTrunkHeight = 6,
            .crownRadius = 2,
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
            .flowerPatchMin = 0.54,
            .flowerSpotChance = 0.17,
            .mushroomPatchMin = 0.73,
            .mushroomSpotChance = 0.20,
        };
    case SurfaceBiome::Jungle:
        return FloraPatchParams{
            .flowerPatchMin = 0.54,
            .flowerSpotChance = 0.14,
            .mushroomPatchMin = 0.57,
            .mushroomSpotChance = 0.24,
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
            BlockType::Dandelion,
            BlockType::Poppy,
            BlockType::OxeyeDaisy,
            BlockType::Allium,
            BlockType::BlueOrchid,
        };
        return pool[h % pool.size()];
    }
    constexpr std::array<BlockType, 4> pool{
        BlockType::Dandelion,
        BlockType::Poppy,
        BlockType::OxeyeDaisy,
        BlockType::Allium,
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

    // Spawn occasional giant variants so some trees are dramatically larger.
    if (settings.crownBlock == BlockType::JungleTreeCrown
        && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x193U) < 0.18)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x319U);
        trunkHeight += 8 + static_cast<int>(giantHash % 8U);
        crownRadius += 2;
        if ((giantHash & 1U) == 0U)
        {
            trunkWidth = 2;
            crownRadius += 1;
        }
    }
    else if (settings.crownBlock == BlockType::TreeCrown
             && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x41BU) < 0.06)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x58DU);
        trunkHeight += 5 + static_cast<int>(giantHash % 5U);
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
        // Add hanging jungle vines along canopy edges for a denser, Minecraft-like jungle silhouette.
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

            // Jungle gets an extra nearby tree attempt so canopy feels denser like Minecraft jungles.
            if (biome == SurfaceBiome::Jungle)
            {
                const std::uint32_t denseHash = noise::hashCoordinates(cellX, cellZ, kJungleDenseTreeSeed);
                const int offsetX = static_cast<int>(denseHash % 5U) - 2;
                const int offsetZ = static_cast<int>((denseHash / 5U) % 5U) - 2;
                if ((offsetX != 0 || offsetZ != 0)
                    && noise::random01(cellX, cellZ, kJungleDenseTreeRollSeed) < 0.72)
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
                continue;
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
                if (surfaceBlock == BlockType::JungleGrass && mossPatch > 0.67)
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
                if (jungleFeature > 0.82
                    && noise::random01(worldX, worldZ, kJungleStoneSeed) < 0.075)
                {
                    chunk.setBlock(localX, surfaceY + 1, localZ, BlockType::MossyCobblestone);
                    continue;
                }
                if (jungleFeature > 0.70
                    && noise::random01(worldX, worldZ, kJungleFeatureRollSeed) < 0.20)
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
                            const int bambooHeight = 3 + static_cast<int>(h % 8U);
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
                // Guardrail: keep crafted/player-only blocks out of procedural terrain.
                if (!isNaturalTerrainBlock(blockType))
                {
                    blockType = BlockType::Air;
                }
                storage[chunkStorageIndex(localX, y, localZ)] = blockType;
            }
        }
    }

    populateTreesForChunk(chunk, coord, terrainGenerator);
    populateSurfaceFloraForChunk(chunk, coord, terrainGenerator);
}
}  // namespace

void World::generateRadius(const TerrainGenerator& terrainGenerator, const int chunkRadius)
{
    generateMissingChunksAround(terrainGenerator, ChunkCoord{0, 0}, chunkRadius);
}

void World::generateMissingChunksAround(
    const TerrainGenerator& terrainGenerator,
    const ChunkCoord& center,
    const int chunkRadius,
    const std::size_t maxChunksToGenerate)
{
    std::vector<ChunkCoord> pendingCoords;
    pendingCoords.reserve(static_cast<std::size_t>((chunkRadius * 2 + 1) * (chunkRadius * 2 + 1)));

    for (int chunkZ = center.z - chunkRadius; chunkZ <= center.z + chunkRadius; ++chunkZ)
    {
        for (int chunkX = center.x - chunkRadius; chunkX <= center.x + chunkRadius; ++chunkX)
        {
            const ChunkCoord coord{chunkX, chunkZ};
            if (chunks_.contains(coord))
            {
                continue;
            }
            pendingCoords.push_back(coord);
        }
    }

    std::sort(
        pendingCoords.begin(),
        pendingCoords.end(),
        [&center](const ChunkCoord& lhs, const ChunkCoord& rhs)
        {
            const int lhsDx = lhs.x - center.x;
            const int lhsDz = lhs.z - center.z;
            const int rhsDx = rhs.x - center.x;
            const int rhsDz = rhs.z - center.z;
            const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
            const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
            if (lhsDistanceSq != rhsDistanceSq)
            {
                return lhsDistanceSq < rhsDistanceSq;
            }
            if (lhs.z != rhs.z)
            {
                return lhs.z < rhs.z;
            }
            return lhs.x < rhs.x;
        });

    std::size_t generatedCount = 0;
    for (const ChunkCoord& coord : pendingCoords)
    {
        if (generatedCount >= maxChunksToGenerate)
        {
            break;
        }
        if (chunks_.contains(coord))
        {
            continue;
        }

        Chunk& chunk = ensureChunk(coord);
        populateChunkFromTerrain(chunk, coord, terrainGenerator);
        markChunkDirty(coord);
        ++generatedCount;
    }
}

bool World::applyEditCommand(const WorldEditCommand& command)
{
    if (command.position.y < kWorldMinY || command.position.y > kWorldMaxY)
    {
        return false;
    }

    const ChunkCoord coord = worldToChunkCoord(command.position.x, command.position.z);
    Chunk& chunk = ensureChunk(coord);
    const int localX = worldToLocalCoord(command.position.x);
    const int localZ = worldToLocalCoord(command.position.z);
    const BlockType existingType = chunk.blockAt(localX, command.position.y, localZ);

    if (existingType != BlockType::Air && !blockMetadata(existingType).breakable)
    {
        return false;
    }

    const BlockType targetType =
        command.action == WorldEditAction::Place ? command.blockType : BlockType::Air;

    if (!chunk.setBlock(localX, command.position.y, localZ, targetType))
    {
        return false;
    }

    for (const ChunkCoord& dirtyCoord : neighboringChunkCoords(coord))
    {
        markChunkDirty(dirtyCoord);
    }

    return true;
}

bool World::save(const std::filesystem::path& outputPath) const
{
    return WorldSerializer::save(*this, outputPath);
}

bool World::load(const std::filesystem::path& inputPath)
{
    return WorldSerializer::load(*this, inputPath);
}

BlockType World::blockAt(const int worldX, const int y, const int worldZ) const
{
    // Below the supported world depth, behave like an unbroken bedrock floor so physics
    // never drops into a void.
    if (y < kWorldMinY)
    {
        return BlockType::Bedrock;
    }
    if (y > kWorldMaxY)
    {
        return BlockType::Air;
    }

    const ChunkCoord coord = worldToChunkCoord(worldX, worldZ);
    const auto chunkIt = chunks_.find(coord);
    if (chunkIt == chunks_.end())
    {
        return BlockType::Air;
    }

    return chunkIt->second.blockAt(worldToLocalCoord(worldX), y, worldToLocalCoord(worldZ));
}

std::optional<RaycastHit> World::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const float maxDistance,
    const float stepSize) const
{
    static_cast<void>(stepSize);
    if (maxDistance <= 0.0f || glm::dot(direction, direction) == 0.0f)
    {
        return std::nullopt;
    }

    const glm::vec3 normalizedDirection = glm::normalize(direction);
    glm::ivec3 cell(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z)));
    glm::ivec3 previousCell = cell;

    const BlockType startingBlock = blockAt(cell.x, cell.y, cell.z);
    if (isRaycastTarget(startingBlock))
    {
        return RaycastHit{
            .solidBlock = cell,
            .buildTarget = previousCell,
            .blockType = startingBlock,
        };
    }

    constexpr float kInfinity = std::numeric_limits<float>::infinity();
    const glm::ivec3 step(
        normalizedDirection.x > 0.0f ? 1 : (normalizedDirection.x < 0.0f ? -1 : 0),
        normalizedDirection.y > 0.0f ? 1 : (normalizedDirection.y < 0.0f ? -1 : 0),
        normalizedDirection.z > 0.0f ? 1 : (normalizedDirection.z < 0.0f ? -1 : 0));
    const auto firstBoundaryDistance =
        [kInfinity](const float originCoord, const float directionCoord, const int cellCoord)
    {
        if (directionCoord > 0.0f)
        {
            return (static_cast<float>(cellCoord + 1) - originCoord) / directionCoord;
        }
        if (directionCoord < 0.0f)
        {
            return (originCoord - static_cast<float>(cellCoord)) / -directionCoord;
        }
        return kInfinity;
    };
    const auto deltaDistance = [kInfinity](const float directionCoord)
    {
        return directionCoord != 0.0f ? 1.0f / std::abs(directionCoord) : kInfinity;
    };

    float nextX = firstBoundaryDistance(origin.x, normalizedDirection.x, cell.x);
    float nextY = firstBoundaryDistance(origin.y, normalizedDirection.y, cell.y);
    float nextZ = firstBoundaryDistance(origin.z, normalizedDirection.z, cell.z);
    const float deltaX = deltaDistance(normalizedDirection.x);
    const float deltaY = deltaDistance(normalizedDirection.y);
    const float deltaZ = deltaDistance(normalizedDirection.z);

    while (true)
    {
        const float travelDistance = std::min(nextX, std::min(nextY, nextZ));
        if (travelDistance > maxDistance)
        {
            break;
        }

        previousCell = cell;
        if (nextX <= nextY && nextX <= nextZ)
        {
            cell.x += step.x;
            nextX += deltaX;
        }
        else if (nextY <= nextZ)
        {
            cell.y += step.y;
            nextY += deltaY;
        }
        else
        {
            cell.z += step.z;
            nextZ += deltaZ;
        }

        const BlockType blockType = blockAt(cell.x, cell.y, cell.z);
        if (isRaycastTarget(blockType))
        {
            return RaycastHit{
                .solidBlock = cell,
                .buildTarget = previousCell,
                .blockType = blockType,
            };
        }
    }

    return std::nullopt;
}

const World::ChunkMap& World::chunks() const
{
    return chunks_;
}

const std::unordered_map<ChunkCoord, ChunkMeshStats, ChunkCoordHash>& World::meshStats() const
{
    return meshStats_;
}

std::size_t World::dirtyChunkCount() const
{
    return dirtyChunks_.size();
}

std::vector<ChunkCoord> World::dirtyChunkCoords() const
{
    return std::vector<ChunkCoord>(dirtyChunks_.begin(), dirtyChunks_.end());
}

std::uint32_t World::totalVisibleFaces() const
{
    std::uint32_t faceCount = 0;
    for (const auto& [coord, stats] : meshStats_)
    {
        static_cast<void>(coord);
        faceCount += stats.faceCount;
    }
    return faceCount;
}

void World::rebuildDirtyMeshes(const vibecraft::meshing::ChunkMesher& chunkMesher)
{
    const std::vector<ChunkCoord> dirtyCoords(dirtyChunks_.begin(), dirtyChunks_.end());
    rebuildDirtyMeshes(chunkMesher, dirtyCoords);
}

void World::rebuildDirtyMeshes(
    const vibecraft::meshing::ChunkMesher& chunkMesher,
    const std::span<const ChunkCoord> chunkCoords)
{
    for (const ChunkCoord& coord : chunkCoords)
    {
        if (!dirtyChunks_.contains(coord))
        {
            continue;
        }

        if (chunks_.find(coord) == chunks_.end())
        {
            dirtyChunks_.erase(coord);
            continue;
        }

        const vibecraft::meshing::ChunkMeshData meshData = chunkMesher.buildMesh(*this, coord);
        meshStats_[coord] = ChunkMeshStats{
            .faceCount = meshData.faceCount,
            .vertexCount = static_cast<std::uint32_t>(meshData.vertices.size()),
            .indexCount = static_cast<std::uint32_t>(meshData.indices.size()),
        };
        dirtyChunks_.erase(coord);
    }
}

void World::applyMeshStatsAndClearDirty(const std::span<const ChunkMeshUpdate> updates)
{
    for (const ChunkMeshUpdate& update : updates)
    {
        if (chunks_.find(update.coord) == chunks_.end())
        {
            dirtyChunks_.erase(update.coord);
            meshStats_.erase(update.coord);
            continue;
        }

        meshStats_[update.coord] = update.stats;
        dirtyChunks_.erase(update.coord);
    }
}

void World::replaceChunk(Chunk chunk)
{
    const ChunkCoord coord = chunk.coord();
    const auto existingIt = chunks_.find(coord);
    if (existingIt != chunks_.end() && existingIt->second.blockStorage() == chunk.blockStorage())
    {
        return;
    }

    chunks_[coord] = std::move(chunk);

    // A streamed chunk update can change faces on the chunk itself and along all four borders.
    for (const ChunkCoord& dirtyCoord : neighboringChunkCoords(coord))
    {
        markChunkDirty(dirtyCoord);
    }
}

void World::replaceChunks(ChunkMap chunks)
{
    chunks_ = std::move(chunks);
    dirtyChunks_.clear();
    meshStats_.clear();

    for (const auto& [coord, chunk] : chunks_)
    {
        static_cast<void>(chunk);
        markChunkDirty(coord);
    }
}

Chunk& World::ensureChunk(const ChunkCoord& coord)
{
    auto [chunkIt, inserted] = chunks_.try_emplace(coord, coord);
    static_cast<void>(inserted);
    return chunkIt->second;
}

void World::markChunkDirty(const ChunkCoord& coord)
{
    dirtyChunks_.insert(coord);
}
}  // namespace vibecraft::world
