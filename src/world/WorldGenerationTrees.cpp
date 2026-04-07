#include "WorldGenerationDetail.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/TreeGenerationTuning.hpp"
#include "vibecraft/world/biomes/TreeVariantRules.hpp"

namespace vibecraft::world::detail
{
using biomes::TreeBiomeSettings;
using biomes::treeBiomeSettingsForSurfaceBiome;

namespace
{
constexpr int kTreeCellSize = 7;
constexpr int kTreeMaxCrownRadius = 7;
constexpr int kTreeMinSurfaceY = 27;
constexpr std::uint32_t kTreeSeed = 0x0ea52f4dU;
constexpr std::uint32_t kTreeChanceSeed = 0x6f4a31deU;
constexpr std::uint32_t kTreeOffsetXSeed = 0x34b6f0e1U;
constexpr std::uint32_t kTreeOffsetZSeed = 0x5cd2a907U;
constexpr std::uint32_t kTreeShapeSeed = 0x72f1a4b3U;
constexpr std::uint32_t kJungleCanopyVineSeed = 0xb72f6a4eU;
constexpr std::uint32_t kJungleTrunkVineSeed = 0xc8407b5fU;
constexpr std::uint32_t kJungleCocoaSeed = 0xd9518c60U;
constexpr std::uint32_t kJungleDenseTreeSeed = 0xea629d71U;
constexpr std::uint32_t kJungleDenseTreeRollSeed = 0xfb73ae82U;

[[nodiscard]] int floorDiv(const int value, const int divisor)
{
    return value >= 0 ? value / divisor : (value - (divisor - 1)) / divisor;
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
            const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX + trunkX, surfaceY, worldZ + trunkZ);
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

void placeTemperateCanopy(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int crownCenterX,
    const int crownCenterY,
    const int crownCenterZ,
    const BlockType crownBlock)
{
    const std::array<std::pair<int, int>, 4> layers{{
        {-2, 1},
        {-1, 2},
        {0, 2},
        {1, 1},
    }};

    for (const auto& [dy, radius] : layers)
    {
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (radius >= 2 && std::abs(dx) == radius && std::abs(dz) == radius
                    && noise::random01(
                           crownCenterX + dx,
                           crownCenterZ + dz,
                           kTreeShapeSeed + static_cast<std::uint32_t>(crownCenterY + dy))
                        > 0.62f)
                {
                    continue;
                }
                placeBlockIfInsideChunk(
                    chunk,
                    coord,
                    crownCenterX + dx,
                    crownCenterY + dy,
                    crownCenterZ + dz,
                    crownBlock);
            }
        }
    }

    placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY + 2, crownCenterZ, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, crownCenterX + 1, crownCenterY, crownCenterZ, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, crownCenterX - 1, crownCenterY, crownCenterZ, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY, crownCenterZ + 1, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY, crownCenterZ - 1, crownBlock);
}

void placeBroadTemperateCanopy(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int trunkBaseX,
    const int crownCenterY,
    const int trunkBaseZ,
    const int crownRadius,
    const int trunkWidth,
    const BlockType crownBlock)
{
    const int radius = std::max(2, crownRadius);
    const std::array<std::pair<int, int>, 6> layers{{
        {-3, -1},
        {-2, 0},
        {-1, 1},
        {0, 1},
        {1, 0},
        {2, -1},
    }};

    for (const auto& [dy, radiusAdjust] : layers)
    {
        const int layerRadius = std::max(1, radius + radiusAdjust);
        const int y = crownCenterY + dy;
        for (int dz = -layerRadius; dz <= trunkWidth - 1 + layerRadius; ++dz)
        {
            for (int dx = -layerRadius; dx <= trunkWidth - 1 + layerRadius; ++dx)
            {
                const int outsideX = dx < 0 ? -dx : std::max(0, dx - (trunkWidth - 1));
                const int outsideZ = dz < 0 ? -dz : std::max(0, dz - (trunkWidth - 1));
                const int edgeDistance = std::max(outsideX, outsideZ);
                if (edgeDistance > layerRadius)
                {
                    continue;
                }
                const bool isOuterEdge = edgeDistance == layerRadius;
                const int worldX = trunkBaseX + dx;
                const int worldZ = trunkBaseZ + dz;
                if (isOuterEdge
                    && noise::random01(worldX, worldZ, kTreeShapeSeed + static_cast<std::uint32_t>(y - kWorldMinY))
                        > (layerRadius >= 3 ? 0.44f : 0.62f))
                {
                    continue;
                }
                placeBlockIfInsideChunk(chunk, coord, worldX, y, worldZ, crownBlock);
            }
        }
    }

    const int centerMinX = trunkBaseX;
    const int centerMinZ = trunkBaseZ;
    const int centerMaxX = trunkBaseX + trunkWidth - 1;
    const int centerMaxZ = trunkBaseZ + trunkWidth - 1;
    for (int z = centerMinZ; z <= centerMaxZ; ++z)
    {
        for (int x = centerMinX; x <= centerMaxX; ++x)
        {
            placeBlockIfInsideChunk(chunk, coord, x, crownCenterY + 3, z, crownBlock);
        }
    }

    const int midX = trunkBaseX + trunkWidth / 2;
    const int midZ = trunkBaseZ + trunkWidth / 2;
    placeBlockIfInsideChunk(chunk, coord, midX - radius, crownCenterY - 1, midZ, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, midX + radius, crownCenterY - 1, midZ, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, midX, crownCenterY - 1, midZ - radius, crownBlock);
    placeBlockIfInsideChunk(chunk, coord, midX, crownCenterY - 1, midZ + radius, crownBlock);
}

void placeSnowCanopy(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int crownCenterX,
    const int crownCenterY,
    const int crownCenterZ,
    const BlockType crownBlock)
{
    const std::array<std::pair<int, int>, 6> layers{{
        {2, 0},
        {1, 1},
        {0, 1},
        {-1, 2},
        {-2, 2},
        {-3, 1},
    }};

    for (const auto& [dy, radius] : layers)
    {
        const int y = crownCenterY + dy;
        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                if (radius > 0 && std::abs(dx) == radius && std::abs(dz) == radius
                    && noise::random01(
                           crownCenterX + dx,
                           crownCenterZ + dz,
                           kTreeShapeSeed + static_cast<std::uint32_t>(y - kWorldMinY))
                        > 0.44f)
                {
                    continue;
                }
                placeBlockIfInsideChunk(chunk, coord, crownCenterX + dx, y, crownCenterZ + dz, crownBlock);
            }
        }
    }
}

void placeJungleCanopy(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int crownCenterX,
    const int crownCenterY,
    const int crownCenterZ,
    const int crownRadius,
    const BlockType crownBlock)
{
    for (int dy = -crownRadius; dy <= 0; ++dy)
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
                placeBlockIfInsideChunk(chunk, coord, crownX, crownY, crownZ, crownBlock);
            }
        }
    }

    placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY + 1, crownCenterZ, crownBlock);
    if (crownRadius >= 3)
    {
        placeBlockIfInsideChunk(chunk, coord, crownCenterX + crownRadius, crownCenterY - 1, crownCenterZ, crownBlock);
        placeBlockIfInsideChunk(chunk, coord, crownCenterX - crownRadius, crownCenterY - 1, crownCenterZ, crownBlock);
        placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY - 1, crownCenterZ + crownRadius, crownBlock);
        placeBlockIfInsideChunk(chunk, coord, crownCenterX, crownCenterY - 1, crownCenterZ - crownRadius, crownBlock);
    }

    // Lower fringe ring — drooping outer leaves one block below the main canopy body,
    // producing the characteristic wide overhanging silhouette of large Minecraft jungle trees.
    if (crownRadius >= 4)
    {
        const int fringeY = crownCenterY - crownRadius - 1;
        const int fringeRadius = crownRadius - 1;
        for (int dz = -fringeRadius; dz <= fringeRadius; ++dz)
        {
            for (int dx = -fringeRadius; dx <= fringeRadius; ++dx)
            {
                // Only the outer ring of the fringe layer (skip interior — already covered above)
                if (std::abs(dx) < fringeRadius - 1 && std::abs(dz) < fringeRadius - 1)
                {
                    continue;
                }
                const bool fringeCorner = std::abs(dx) == fringeRadius && std::abs(dz) == fringeRadius;
                if (fringeCorner
                    && noise::random01(
                           crownCenterX + dx,
                           crownCenterZ + dz,
                           kTreeShapeSeed + static_cast<std::uint32_t>(fringeY - kWorldMinY))
                        > 0.35)
                {
                    continue;
                }
                placeBlockIfInsideChunk(chunk, coord, crownCenterX + dx, fringeY, crownCenterZ + dz, crownBlock);
            }
        }
    }
}

void decorateJungleTree(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int surfaceY,
    const int trunkHeight,
    const int crownCenterX,
    const int crownCenterY,
    const int crownCenterZ,
    const int crownRadius,
    const int trunkWidth)
{
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

    const std::array<std::array<int, 2>, 4> trunkSides{{
        {{trunkWidth, 0}},
        {{-1, 0}},
        {{0, trunkWidth}},
        {{0, -1}},
    }};
    const int trunkVineStartY = surfaceY + trunkHeight / 2;
    for (int y = trunkVineStartY; y <= surfaceY + trunkHeight; ++y)
    {
        const std::uint32_t ySeed = static_cast<std::uint32_t>(y - kWorldMinY);
        for (const auto& side : trunkSides)
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

void placeTreeForColumn(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int treeX,
    const int treeZ,
    const SurfaceBiome biome,
    const TreeBiomeSettings& baseSettings,
    const biomes::BiomeVariationSample& variation,
    const biomes::BiomeTransitionSample& transition)
{
    const TreeBiomeSettings settings = biomes::softenTreeSettingsForBiomeEdge(
        biome,
        transition,
        biomes::treeVariantSettings(biome, baseSettings, variation, treeX, treeZ));
    const int surfaceY = terrainGenerator.surfaceHeightAt(treeX, treeZ);
    if (settings.spawnChance <= 0.0f)
    {
        return;
    }

    const std::uint32_t treeHash = noise::hashCoordinates(treeX, treeZ, kTreeSeed);
    int trunkHeight = settings.minTrunkHeight
        + static_cast<int>(treeHash % static_cast<std::uint32_t>(settings.maxTrunkHeight - settings.minTrunkHeight + 1));
    int crownRadius = settings.crownRadius;
    int trunkWidth = settings.trunkWidth;
    const bool isJungleTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Jungle;
    const bool isBroadTemperateTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::BroadTemperate;
    const bool isTemperateTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Temperate || isBroadTemperateTree;
    const bool isSnowTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Snowy;

    if (isJungleTree && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x193U) < 0.18)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x319U);
        trunkHeight += 5 + static_cast<int>(giantHash % 10U);
        crownRadius += 2;
        if (giantHash % 3U != 0U)
        {
            trunkWidth = 2;
        }
    }
    else if (isTemperateTree && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x41BU) < 0.005)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x58DU);
        trunkHeight += 2 + static_cast<int>(giantHash % 2U);
        crownRadius += 1;
    }
    if (settings.largeTreeChance > 0.0f
        && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x8C3U) < settings.largeTreeChance)
    {
        trunkHeight += settings.largeTreeHeightBonus;
        crownRadius += settings.largeTreeCrownRadiusBonus;
        if (isJungleTree)
        {
            trunkWidth = 2;
        }
        else
        {
            trunkWidth = std::max(trunkWidth, 2);
        }
    }
    if (settings.trunkBlock == BlockType::DarkOakLog
        && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x72DU) < 0.18f)
    {
        trunkWidth = 2;
        trunkHeight += 1 + static_cast<int>(noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x733U) % 2U);
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
    if (isBroadTemperateTree)
    {
        placeBroadTemperateCanopy(chunk, coord, treeX, crownCenterY, treeZ, crownRadius, trunkWidth, settings.crownBlock);
    }
    else if (isTemperateTree)
    {
        placeTemperateCanopy(chunk, coord, crownCenterX, crownCenterY, crownCenterZ, settings.crownBlock);
    }
    else if (isSnowTree)
    {
        placeSnowCanopy(chunk, coord, crownCenterX, crownCenterY, crownCenterZ, settings.crownBlock);
    }
    else
    {
        placeJungleCanopy(chunk, coord, crownCenterX, crownCenterY, crownCenterZ, crownRadius, settings.crownBlock);
    }

    // Sub-canopies at intermediate heights for tall jungle trees — gives the
    // layered "emergent tree" silhouette seen in Minecraft jungle biomes.
    if (isJungleTree && trunkHeight >= 14)
    {
        const int midCanopyY = surfaceY + trunkHeight * 2 / 3;
        const int midRadius = std::max(2, crownRadius - 1);
        placeJungleCanopy(chunk, coord, crownCenterX, midCanopyY, crownCenterZ, midRadius, settings.crownBlock);
    }
    if (isJungleTree && trunkHeight >= 22)
    {
        const int lowCanopyY = surfaceY + trunkHeight / 3;
        const int lowRadius = std::max(2, crownRadius - 2);
        placeJungleCanopy(chunk, coord, crownCenterX, lowCanopyY, crownCenterZ, lowRadius, settings.crownBlock);
    }

    if (isJungleTree)
    {
        decorateJungleTree(
            chunk,
            coord,
            surfaceY,
            trunkHeight,
            crownCenterX,
            crownCenterY,
            crownCenterZ,
            crownRadius,
            trunkWidth);
    }
}
}  // namespace

void populateTreesForChunk(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator)
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
            const biomes::BiomeVariationSample variation =
                biomes::sampleBiomeVariation(biome, treeX, treeZ, terrainGenerator.worldSeed());
            const biomes::BiomeTransitionSample transition = biomes::sampleBiomeTransition(
                biome,
                treeX,
                treeZ,
                [&](const int sampleX, const int sampleZ) { return terrainGenerator.surfaceBiomeAt(sampleX, sampleZ); });
            const float effectiveChance = biomes::softenTreeSpawnChanceForBiomeEdge(
                biome,
                transition,
                biomes::effectiveTreeSpawnChance(biome, settings, variation, treeX, treeZ));
            if (noise::random01(cellX, cellZ, kTreeChanceSeed) > effectiveChance)
            {
                continue;
            }

            placeTreeForColumn(chunk, coord, terrainGenerator, treeX, treeZ, biome, settings, variation, transition);
            const biomes::TreeGenerationFamily biomeFamily = biomes::biomeProfile(biome).treeFamily;
            if (biomeFamily == biomes::TreeGenerationFamily::Jungle
                || biomeFamily == biomes::TreeGenerationFamily::BambooJungle)
            {
                const std::uint32_t denseHash = noise::hashCoordinates(cellX, cellZ, kJungleDenseTreeSeed);
                const int offsetX = static_cast<int>(denseHash % 5U) - 2;
                const int offsetZ = static_cast<int>((denseHash / 5U) % 5U) - 2;
                if ((offsetX == 0 && offsetZ == 0)
                    || noise::random01(cellX, cellZ, kJungleDenseTreeRollSeed) >= 0.30)
                {
                    continue;
                }

                const int denseTreeX = treeX + offsetX;
                const int denseTreeZ = treeZ + offsetZ;
                const SurfaceBiome denseBiome = terrainGenerator.surfaceBiomeAt(denseTreeX, denseTreeZ);
                const TreeBiomeSettings denseSettings = treeBiomeSettingsForSurfaceBiome(denseBiome);
                const biomes::TreeGenerationFamily denseFamily = biomes::biomeProfile(denseBiome).treeFamily;
                const biomes::BiomeVariationSample denseVariation =
                    biomes::sampleBiomeVariation(denseBiome, denseTreeX, denseTreeZ, terrainGenerator.worldSeed());
                const biomes::BiomeTransitionSample denseTransition = biomes::sampleBiomeTransition(
                    denseBiome,
                    denseTreeX,
                    denseTreeZ,
                    [&](const int sampleX, const int sampleZ) { return terrainGenerator.surfaceBiomeAt(sampleX, sampleZ); });
                if ((denseFamily == biomes::TreeGenerationFamily::Jungle
                        || denseFamily == biomes::TreeGenerationFamily::BambooJungle)
                    && denseSettings.spawnChance > 0.0f)
                {
                    placeTreeForColumn(
                        chunk,
                        coord,
                        terrainGenerator,
                        denseTreeX,
                        denseTreeZ,
                        denseBiome,
                        denseSettings,
                        denseVariation,
                        denseTransition);
                }
                continue;
            }

            // Secondary tree chance for denser mixed forests in temperate/snow biomes.
            const std::uint32_t extraHash = noise::hashCoordinates(cellX, cellZ, kJungleDenseTreeSeed + 0x1123U);
            const int extraOffsetX = static_cast<int>(extraHash % 5U) - 2;
            const int extraOffsetZ = static_cast<int>((extraHash / 5U) % 5U) - 2;
            if (extraOffsetX == 0 && extraOffsetZ == 0)
            {
                continue;
            }
            const float extraChance = biomes::softenSecondaryTreeChanceForBiomeEdge(
                biome,
                transition,
                biomes::secondaryTreeChance(biome, variation));
            if (extraChance <= 0.0f)
            {
                continue;
            }
            if (noise::random01(cellX, cellZ, kJungleDenseTreeRollSeed + 0x5123U) >= extraChance)
            {
                continue;
            }
            const int extraTreeX = treeX + extraOffsetX;
            const int extraTreeZ = treeZ + extraOffsetZ;
            const SurfaceBiome extraBiome = terrainGenerator.surfaceBiomeAt(extraTreeX, extraTreeZ);
            const TreeBiomeSettings extraSettings = treeBiomeSettingsForSurfaceBiome(extraBiome);
            const biomes::BiomeVariationSample extraVariation =
                biomes::sampleBiomeVariation(extraBiome, extraTreeX, extraTreeZ, terrainGenerator.worldSeed());
            const biomes::BiomeTransitionSample extraTransition = biomes::sampleBiomeTransition(
                extraBiome,
                extraTreeX,
                extraTreeZ,
                [&](const int sampleX, const int sampleZ) { return terrainGenerator.surfaceBiomeAt(sampleX, sampleZ); });
            if (biomes::isForestSurfaceBiome(extraBiome) && extraSettings.spawnChance > 0.0f)
            {
                placeTreeForColumn(
                    chunk,
                    coord,
                    terrainGenerator,
                    extraTreeX,
                    extraTreeZ,
                    extraBiome,
                    extraSettings,
                    extraVariation,
                    extraTransition);
            }
        }
    }
}
}  // namespace vibecraft::world::detail
