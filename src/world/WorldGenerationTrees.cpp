#include "WorldGenerationDetail.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/TreeGenerationTuning.hpp"

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
constexpr std::uint32_t kTreeForestDensitySeed = 0x83c2b5d4U;
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

[[nodiscard]] float forestDensityAt(const SurfaceBiome biome, const int worldX, const int worldZ)
{
    const double worldXd = static_cast<double>(worldX);
    const double worldZd = static_cast<double>(worldZ);

    switch (biomes::biomeProfile(biome).treeFamily)
    {
    case biomes::TreeGenerationFamily::Plains:
    {
        const double field = noise::fbmNoise2d(worldXd + 31.0, worldZd - 17.0, 248.0, 3, kTreeForestDensitySeed);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::Forest:
    {
        const double field = noise::fbmNoise2d(worldXd + 31.0, worldZd - 17.0, 188.0, 3, kTreeForestDensitySeed + 3U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::BirchForest:
    {
        const double field = noise::fbmNoise2d(worldXd - 27.0, worldZd + 69.0, 180.0, 3, kTreeForestDensitySeed + 11U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::DarkForest:
    {
        const double field = noise::fbmNoise2d(worldXd - 85.0, worldZd + 17.0, 132.0, 3, kTreeForestDensitySeed + 19U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::Taiga:
    {
        const double field = noise::fbmNoise2d(worldXd - 59.0, worldZd + 43.0, 176.0, 3, kTreeForestDensitySeed + 17U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::SnowyTaiga:
    {
        const double field = noise::fbmNoise2d(worldXd - 59.0, worldZd + 43.0, 196.0, 3, kTreeForestDensitySeed + 23U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::Jungle:
    {
        const double field = noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 160.0, 3, kTreeForestDensitySeed + 31U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::SparseJungle:
    {
        const double field = noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 220.0, 3, kTreeForestDensitySeed + 37U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::BambooJungle:
    {
        const double field = noise::fbmNoise2d(worldXd + 73.0, worldZd + 91.0, 132.0, 3, kTreeForestDensitySeed + 41U);
        return static_cast<float>(std::clamp(field * 0.5 + 0.5, 0.0, 1.0));
    }
    case biomes::TreeGenerationFamily::None:
    default:
        return 0.0f;
    }
}

[[nodiscard]] float effectiveTreeSpawnChance(
    const SurfaceBiome biome,
    const TreeBiomeSettings& settings,
    const int worldX,
    const int worldZ)
{
    const float density = forestDensityAt(biome, worldX, worldZ);
    switch (biomes::biomeProfile(biome).treeFamily)
    {
    case biomes::TreeGenerationFamily::Plains:
        if (density < 0.16f)
        {
            return std::clamp(density * 0.08f, 0.0f, 0.015f);
        }
        return std::clamp(settings.spawnChance + (density - 0.16f) * 0.15f, 0.0f, 0.16f);
    case biomes::TreeGenerationFamily::Forest:
        if (density < 0.12f)
        {
            return std::clamp(density * 0.12f, 0.0f, 0.04f);
        }
        return std::clamp(settings.spawnChance + (density - 0.12f) * 0.75f, 0.0f, 0.87f);
    case biomes::TreeGenerationFamily::BirchForest:
        if (density < 0.12f)
        {
            return std::clamp(density * 0.12f, 0.0f, 0.04f);
        }
        return std::clamp(settings.spawnChance + (density - 0.12f) * 0.45f, 0.0f, 0.60f);
    case biomes::TreeGenerationFamily::DarkForest:
        if (density < 0.08f)
        {
            return std::clamp(density * 0.12f, 0.0f, 0.04f);
        }
        return std::clamp(settings.spawnChance + density * 0.36f, 0.0f, 0.74f);
    case biomes::TreeGenerationFamily::Taiga:
        if (density < 0.14f)
        {
            return std::clamp(density * 0.10f, 0.0f, 0.03f);
        }
        return std::clamp(settings.spawnChance + (density - 0.14f) * 0.34f, 0.0f, 0.48f);
    case biomes::TreeGenerationFamily::SnowyTaiga:
        if (density < 0.16f)
        {
            return std::clamp(density * 0.10f, 0.0f, 0.03f);
        }
        return std::clamp(settings.spawnChance + (density - 0.16f) * 0.36f, 0.0f, 0.50f);
    case biomes::TreeGenerationFamily::Jungle:
        if (density < 0.12f)
        {
            return std::clamp(density * 0.12f, 0.0f, 0.04f);
        }
        return std::clamp(settings.spawnChance + density * 0.28f, 0.0f, 0.58f);
    case biomes::TreeGenerationFamily::SparseJungle:
        if (density < 0.14f)
        {
            return std::clamp(density * 0.10f, 0.0f, 0.03f);
        }
        return std::clamp(settings.spawnChance + density * 0.16f, 0.0f, 0.34f);
    case biomes::TreeGenerationFamily::BambooJungle:
        if (density < 0.12f)
        {
            return std::clamp(density * 0.12f, 0.0f, 0.04f);
        }
        return std::clamp(settings.spawnChance + density * 0.32f, 0.0f, 0.64f);
    case biomes::TreeGenerationFamily::None:
    default:
        return 0.0f;
    }
}

[[nodiscard]] TreeBiomeSettings treeVariantForBiome(
    const SurfaceBiome biome,
    const int worldX,
    const int worldZ,
    const TreeBiomeSettings& baseSettings)
{
    TreeBiomeSettings settings = baseSettings;
    const float variantRoll = noise::random01(worldX, worldZ, kTreeShapeSeed + 0x6b9U);
    switch (biomes::biomeProfile(biome).treeFamily)
    {
    case biomes::TreeGenerationFamily::Plains:
        if (variantRoll < 0.86f)
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.crownRadius = 2;
        }
        else
        {
            settings.trunkBlock = BlockType::BirchLog;
            settings.crownBlock = BlockType::BirchLeaves;
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 5);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 7);
        }
        break;
    case biomes::TreeGenerationFamily::Forest:
        // Minecraft Forest: dominant oak with scattered birch (roughly ~1 in 8 trees birch).
        if (variantRoll < 0.88f)
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.crownRadius = 2;
        }
        else
        {
            settings.trunkBlock = BlockType::BirchLog;
            settings.crownBlock = BlockType::BirchLeaves;
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 5);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 7);
            settings.crownRadius = 2;
        }
        break;
    case biomes::TreeGenerationFamily::BirchForest:
        settings.trunkBlock = BlockType::BirchLog;
        settings.crownBlock = BlockType::BirchLeaves;
        settings.minTrunkHeight = std::max(settings.minTrunkHeight, variantRoll < 0.20f ? 7 : 5);
        settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, variantRoll < 0.20f ? 10 : 8);
        settings.crownRadius = variantRoll < 0.20f ? 3 : 2;
        break;
    case biomes::TreeGenerationFamily::DarkForest:
        if (variantRoll < 0.82f)
        {
            settings.trunkBlock = BlockType::DarkOakLog;
            settings.crownBlock = BlockType::DarkOakLeaves;
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 6);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 9);
            settings.crownRadius = 3;
        }
        else
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.minTrunkHeight = std::max(settings.minTrunkHeight, 5);
            settings.maxTrunkHeight = std::max(settings.maxTrunkHeight, 7);
            settings.crownRadius = 2;
        }
        break;
    case biomes::TreeGenerationFamily::Taiga:
        settings.trunkBlock = BlockType::SpruceLog;
        settings.crownBlock = BlockType::SpruceLeaves;
        if (variantRoll > 0.94f)
        {
            settings.trunkBlock = BlockType::BirchLog;
            settings.crownBlock = BlockType::BirchLeaves;
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        break;
    case biomes::TreeGenerationFamily::SnowyTaiga:
        if (variantRoll < 0.84f)
        {
            settings.trunkBlock = BlockType::SpruceLog;
            settings.crownBlock = BlockType::SpruceLeaves;
        }
        else if (variantRoll < 0.94f)
        {
            settings.trunkBlock = BlockType::BirchLog;
            settings.crownBlock = BlockType::BirchLeaves;
            settings.minTrunkHeight = 5;
            settings.maxTrunkHeight = 8;
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        else
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.minTrunkHeight = 5;
            settings.maxTrunkHeight = 7;
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        break;
    case biomes::TreeGenerationFamily::Jungle:
    case biomes::TreeGenerationFamily::SparseJungle:
    case biomes::TreeGenerationFamily::BambooJungle:
        if (variantRoll < 0.88f)
        {
            settings.trunkBlock = BlockType::JungleLog;
            settings.crownBlock = BlockType::JungleLeaves;
        }
        else
        {
            settings.trunkBlock = BlockType::OakLog;
            settings.crownBlock = BlockType::OakLeaves;
            settings.minTrunkHeight = 5;
            settings.maxTrunkHeight = 7;
            settings.crownRadius = 2;
            settings.canopyStyle = TreeBiomeSettings::CanopyStyle::Temperate;
        }
        break;
    case biomes::TreeGenerationFamily::None:
    default:
        break;
    }
    return settings;
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
    const TreeBiomeSettings& baseSettings)
{
    const TreeBiomeSettings settings = treeVariantForBiome(biome, treeX, treeZ, baseSettings);
    const int surfaceY = terrainGenerator.surfaceHeightAt(treeX, treeZ);
    if (settings.spawnChance <= 0.0f)
    {
        return;
    }

    const std::uint32_t treeHash = noise::hashCoordinates(treeX, treeZ, kTreeSeed);
    int trunkHeight = settings.minTrunkHeight
        + static_cast<int>(treeHash % static_cast<std::uint32_t>(settings.maxTrunkHeight - settings.minTrunkHeight + 1));
    int crownRadius = settings.crownRadius;
    int trunkWidth = 1;
    const bool isJungleTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Jungle;
    const bool isTemperateTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Temperate;
    const bool isSnowTree = settings.canopyStyle == TreeBiomeSettings::CanopyStyle::Snowy;

    if (isJungleTree && noise::random01(treeX, treeZ, kTreeShapeSeed + 0x193U) < 0.01)
    {
        const std::uint32_t giantHash = noise::hashCoordinates(treeX, treeZ, kTreeShapeSeed + 0x319U);
        trunkHeight += 3 + static_cast<int>(giantHash % 4U);
        crownRadius += 1;
        if ((giantHash & 1U) == 0U)
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
    if (isTemperateTree)
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
            const float effectiveChance = effectiveTreeSpawnChance(biome, settings, treeX, treeZ);
            if (noise::random01(cellX, cellZ, kTreeChanceSeed) > effectiveChance)
            {
                continue;
            }

            placeTreeForColumn(chunk, coord, terrainGenerator, treeX, treeZ, biome, settings);
            const biomes::TreeGenerationFamily biomeFamily = biomes::biomeProfile(biome).treeFamily;
            if (biomeFamily == biomes::TreeGenerationFamily::Jungle
                || biomeFamily == biomes::TreeGenerationFamily::BambooJungle)
            {
                const std::uint32_t denseHash = noise::hashCoordinates(cellX, cellZ, kJungleDenseTreeSeed);
                const int offsetX = static_cast<int>(denseHash % 5U) - 2;
                const int offsetZ = static_cast<int>((denseHash / 5U) % 5U) - 2;
                if ((offsetX == 0 && offsetZ == 0)
                    || noise::random01(cellX, cellZ, kJungleDenseTreeRollSeed) >= 0.12)
                {
                    continue;
                }

                const int denseTreeX = treeX + offsetX;
                const int denseTreeZ = treeZ + offsetZ;
                const SurfaceBiome denseBiome = terrainGenerator.surfaceBiomeAt(denseTreeX, denseTreeZ);
                const TreeBiomeSettings denseSettings = treeBiomeSettingsForSurfaceBiome(denseBiome);
                const biomes::TreeGenerationFamily denseFamily = biomes::biomeProfile(denseBiome).treeFamily;
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
                        denseSettings);
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
            float extraChance = 0.0f;
            switch (biomeFamily)
            {
            case biomes::TreeGenerationFamily::Forest:
                extraChance = 0.70f;
                break;
            case biomes::TreeGenerationFamily::BirchForest:
                extraChance = 0.28f;
                break;
            case biomes::TreeGenerationFamily::DarkForest:
                extraChance = 0.44f;
                break;
            case biomes::TreeGenerationFamily::Taiga:
                extraChance = 0.30f;
                break;
            case biomes::TreeGenerationFamily::SnowyTaiga:
                extraChance = 0.24f;
                break;
            default:
                extraChance = 0.0f;
                break;
            }
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
            if (biomes::isForestSurfaceBiome(extraBiome) && extraSettings.spawnChance > 0.0f)
            {
                placeTreeForColumn(
                    chunk,
                    coord,
                    terrainGenerator,
                    extraTreeX,
                    extraTreeZ,
                    extraBiome,
                    extraSettings);
            }
        }
    }
}
}  // namespace vibecraft::world::detail
