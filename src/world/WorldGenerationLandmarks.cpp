#include "WorldGenerationLandmarks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "vibecraft/world/TerrainNoise.hpp"

namespace vibecraft::world
{
namespace
{
constexpr std::uint32_t kCrystalSpireSeed = 0x54f2aa13U;
constexpr std::uint32_t kCrystalSpireRollSeed = 0x65c3bb24U;
constexpr std::uint32_t kSandstoneOutcropSeed = 0x87e5dd46U;
constexpr std::uint32_t kSandstoneOutcropRollSeed = 0x98f6ee57U;
constexpr std::uint32_t kGlowForestTowerSeed = 0xa9c7ef10U;
constexpr std::uint32_t kGlowForestTowerRollSeed = 0xbad8f021U;
constexpr std::uint32_t kGlowForestShelfSeed = 0xcbe90132U;
constexpr std::uint32_t kGlowForestShelfRollSeed = 0xdcfa1243U;
constexpr std::uint32_t kCrystalRidgeWallSeed = 0xed0b2354U;
constexpr std::uint32_t kCrystalRidgeWallRollSeed = 0xfe1c3465U;
constexpr std::uint32_t kSandstoneArchSeed = 0x0f2d4576U;
constexpr std::uint32_t kSandstoneArchRollSeed = 0x103e5687U;
constexpr std::uint32_t kSkyrootBridgeSeed = 0x214f6798U;
constexpr std::uint32_t kSkyrootBridgeRollSeed = 0x326078a9U;
constexpr std::uint32_t kCliffVineRollSeed = 0xba180079U;
constexpr std::uint32_t kCliffMossSeed = 0xcb29118aU;

[[nodiscard]] bool isColumnInsideChunk(const ChunkCoord& coord, const int worldX, const int worldZ)
{
    const int minWorldX = coord.x * Chunk::kSize;
    const int minWorldZ = coord.z * Chunk::kSize;
    return worldX >= minWorldX && worldX < minWorldX + Chunk::kSize
        && worldZ >= minWorldZ && worldZ < minWorldZ + Chunk::kSize;
}

void placeIfAir(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType blockType)
{
    if (y < kWorldMinY || y > kWorldMaxY || !isColumnInsideChunk(coord, worldX, worldZ))
    {
        return;
    }

    const int localX = worldToLocalCoord(worldX);
    const int localZ = worldToLocalCoord(worldZ);
    if (chunk.blockAt(localX, y, localZ) == BlockType::Air)
    {
        chunk.setBlock(localX, y, localZ, blockType);
    }
}

[[nodiscard]] int maxNeighborSlope(const TerrainGenerator& terrainGenerator, const int worldX, const int worldZ, const int surfaceY)
{
    int maxSlope = 0;
    constexpr std::array<std::array<int, 2>, 4> kOffsets{{{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}}};
    for (const auto& offset : kOffsets)
    {
        const int neighborY = terrainGenerator.surfaceHeightAt(worldX + offset[0], worldZ + offset[1]);
        maxSlope = std::max(maxSlope, std::abs(neighborY - surfaceY));
    }
    return maxSlope;
}

[[nodiscard]] int localRelief(const TerrainGenerator& terrainGenerator, const int worldX, const int worldZ)
{
    int minY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
    int maxY = minY;
    for (int dz = -2; dz <= 2; ++dz)
    {
        for (int dx = -2; dx <= 2; ++dx)
        {
            const int sampleY = terrainGenerator.surfaceHeightAt(worldX + dx, worldZ + dz);
            minY = std::min(minY, sampleY);
            maxY = std::max(maxY, sampleY);
        }
    }
    return maxY - minY;
}

[[nodiscard]] std::pair<int, int> steepestDropDirection(
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    std::pair<int, int> bestOffset{0, 0};
    int bestDrop = 0;
    constexpr std::array<std::array<int, 2>, 4> kOffsets{{{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}}};
    for (const auto& offset : kOffsets)
    {
        const int neighborY = terrainGenerator.surfaceHeightAt(worldX + offset[0], worldZ + offset[1]);
        const int drop = surfaceY - neighborY;
        if (drop > bestDrop)
        {
            bestDrop = drop;
            bestOffset = {offset[0], offset[1]};
        }
    }
    return bestOffset;
}

void decorateCrystalSpire(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kCrystalSpireRollSeed) >= 0.68f)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kCrystalSpireSeed);
    const int height = 3 + static_cast<int>(h % 5U);
    for (int i = 1; i <= height; ++i)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + i, worldZ, BlockType::Glowstone);
        if (i < 2)
        {
            continue;
        }

        constexpr std::array<std::array<int, 2>, 4> kOffsets{{{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}}};
        for (std::size_t index = 0; index < kOffsets.size(); ++index)
        {
            if (((h >> static_cast<std::uint32_t>(index + i)) & 1U) == 0U)
            {
                continue;
            }
            placeIfAir(
                chunk,
                coord,
                worldX + kOffsets[index][0],
                surfaceY + i,
                worldZ + kOffsets[index][1],
                BlockType::Glowstone);
        }
    }

    if ((h & 1U) != 0U)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + height + 1, worldZ, BlockType::Glowstone);
    }

    constexpr std::array<std::array<int, 2>, 8> kShardOffsets{{
        {{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}, {{1, 1}}, {{1, -1}}, {{-1, 1}}, {{-1, -1}},
    }};
    for (std::size_t index = 0; index < kShardOffsets.size(); ++index)
    {
        if (((h >> static_cast<std::uint32_t>(index + 6U)) & 1U) == 0U)
        {
            continue;
        }
        const int shardHeight = 1 + static_cast<int>((h >> static_cast<std::uint32_t>(index + 14U)) % 2U);
        for (int y = 1; y <= shardHeight; ++y)
        {
            placeIfAir(
                chunk,
                coord,
                worldX + kShardOffsets[index][0],
                surfaceY + y,
                worldZ + kShardOffsets[index][1],
                BlockType::Glowstone);
        }
    }
}

void decorateSandstoneOutcrop(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kSandstoneOutcropRollSeed) >= 0.60f)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kSandstoneOutcropSeed);
    const int height = 2 + static_cast<int>(h % 4U);
    for (int i = 1; i <= height; ++i)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + i, worldZ, BlockType::Sandstone);
    }

    if (((h >> 3U) & 1U) != 0U)
    {
        placeIfAir(chunk, coord, worldX + 1, surfaceY + height - 1, worldZ, BlockType::Sandstone);
    }
    if (((h >> 4U) & 1U) != 0U)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + height - 2, worldZ + 1, BlockType::Sandstone);
    }

    constexpr std::array<std::array<int, 2>, 4> kFins{{{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}}};
    for (std::size_t index = 0; index < kFins.size(); ++index)
    {
        if (((h >> static_cast<std::uint32_t>(index + 8U)) & 1U) == 0U)
        {
            continue;
        }
        const int finHeight = 1 + static_cast<int>((h >> static_cast<std::uint32_t>(index + 12U)) % 3U);
        for (int y = 1; y <= finHeight; ++y)
        {
            placeIfAir(
                chunk,
                coord,
                worldX + kFins[index][0],
                surfaceY + y,
                worldZ + kFins[index][1],
                BlockType::Sandstone);
        }
    }

    if (((h >> 18U) & 1U) != 0U)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + 1, worldZ + 1, BlockType::DeadBush);
    }
}

void decorateCliffGreenery(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY,
    const SurfaceBiome biome)
{
    if (noise::random01(worldX, worldZ, kCliffVineRollSeed) >= (biome == SurfaceBiome::Jungle ? 0.14f : 0.07f))
    {
        return;
    }

    const auto [dx, dz] = steepestDropDirection(terrainGenerator, worldX, worldZ, surfaceY);
    if (dx == 0 && dz == 0)
    {
        return;
    }

    const int neighborY = terrainGenerator.surfaceHeightAt(worldX + dx, worldZ + dz);
    const int drop = surfaceY - neighborY;
    if (drop < 5)
    {
        return;
    }

    const int vineLength = std::min(drop - 1, biome == SurfaceBiome::Jungle ? 8 : 5);
    for (int i = 1; i <= vineLength; ++i)
    {
        placeIfAir(chunk, coord, worldX, surfaceY + 1 - i, worldZ, BlockType::Vines);
    }

    if (biome == SurfaceBiome::Jungle && noise::random01(worldX, worldZ, kCliffMossSeed) < 0.18f)
    {
        const int localX = worldToLocalCoord(worldX);
        const int localZ = worldToLocalCoord(worldZ);
        if (chunk.blockAt(localX, surfaceY, localZ) == BlockType::JungleGrass)
        {
            chunk.setBlock(localX, surfaceY, localZ, BlockType::MossBlock);
        }
    }
}

void decorateGlowForestTower(
    Chunk& chunk,
    const ChunkCoord& coord,
    const int worldX,
    const int worldZ,
    const int surfaceY,
    const SurfaceBiome biome)
{
    const float chance = biome == SurfaceBiome::Jungle ? 0.08f : 0.04f;
    if (noise::random01(worldX, worldZ, kGlowForestTowerRollSeed) >= chance)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kGlowForestTowerSeed);
    const BlockType trunkBlock = biome == SurfaceBiome::Jungle ? BlockType::JungleTreeTrunk : BlockType::TreeTrunk;
    const BlockType crownBlock = biome == SurfaceBiome::Jungle ? BlockType::JungleTreeCrown : BlockType::TreeCrown;
    const int trunkHeight = biome == SurfaceBiome::Jungle
        ? 9 + static_cast<int>(h % 6U)
        : 7 + static_cast<int>(h % 4U);
    const int crownRadius = biome == SurfaceBiome::Jungle ? 3 : 2;
    const int trunkWidth = biome == SurfaceBiome::Jungle && ((h >> 3U) & 1U) != 0U ? 2 : 1;

    for (int y = surfaceY + 1; y <= surfaceY + trunkHeight; ++y)
    {
        for (int dz = 0; dz < trunkWidth; ++dz)
        {
            for (int dx = 0; dx < trunkWidth; ++dx)
            {
                placeIfAir(chunk, coord, worldX + dx, y, worldZ + dz, trunkBlock);
            }
        }
    }

    const int crownCenterX = worldX + (trunkWidth - 1) / 2;
    const int crownCenterZ = worldZ + (trunkWidth - 1) / 2;
    const int crownBaseY = surfaceY + trunkHeight;
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
                if (std::abs(dx) == radius && std::abs(dz) == radius && radius >= 2)
                {
                    continue;
                }
                placeIfAir(chunk, coord, crownCenterX + dx, crownBaseY + dy, crownCenterZ + dz, crownBlock);
            }
        }
    }
    placeIfAir(chunk, coord, crownCenterX, crownBaseY + 2, crownCenterZ, crownBlock);

    constexpr std::array<std::array<int, 2>, 4> kCardinals{{{{1, 0}}, {{-1, 0}}, {{0, 1}}, {{0, -1}}}};
    for (std::size_t i = 0; i < kCardinals.size(); ++i)
    {
        const int sideX = crownCenterX + kCardinals[i][0] * trunkWidth;
        const int sideZ = crownCenterZ + kCardinals[i][1] * trunkWidth;
        if (((h >> static_cast<std::uint32_t>(i + 8U)) & 1U) != 0U)
        {
            placeIfAir(chunk, coord, sideX, surfaceY + trunkHeight / 2 + 1, sideZ, BlockType::CocoaPod);
        }
        if (((h >> static_cast<std::uint32_t>(i + 12U)) & 1U) != 0U)
        {
            placeIfAir(chunk, coord, sideX, surfaceY + 1, sideZ, BlockType::Melon);
        }
    }

    const int vineStartY = crownBaseY - 1;
    const int vineLength = biome == SurfaceBiome::Jungle ? 5 : 3;
    for (const auto& offset : kCardinals)
    {
        const int vineX = crownCenterX + offset[0] * crownRadius;
        const int vineZ = crownCenterZ + offset[1] * crownRadius;
        for (int i = 0; i < vineLength; ++i)
        {
            placeIfAir(chunk, coord, vineX, vineStartY - i, vineZ, BlockType::Vines);
        }
    }
}

void decorateGlowForestShelf(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kGlowForestShelfRollSeed) >= 0.58f)
    {
        return;
    }

    const auto [dx, dz] = steepestDropDirection(terrainGenerator, worldX, worldZ, surfaceY);
    if (dx == 0 && dz == 0)
    {
        return;
    }

    const int neighborY = terrainGenerator.surfaceHeightAt(worldX + dx, worldZ + dz);
    const int drop = surfaceY - neighborY;
    if (drop < 4)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kGlowForestShelfSeed);
    const int pedestalHeight = 4 + static_cast<int>(h % 5U);
    const int shelfReach = 3 + static_cast<int>((h >> 4U) % 4U);
    const int shelfHalfWidth = 1 + static_cast<int>((h >> 8U) % 2U);
    const int shelfY = surfaceY + pedestalHeight;
    const int sideX = -dz;
    const int sideZ = dx;

    for (int y = surfaceY + 1; y <= shelfY; ++y)
    {
        placeIfAir(chunk, coord, worldX, y, worldZ, BlockType::JungleTreeTrunk);
    }

    for (int forward = 0; forward <= shelfReach; ++forward)
    {
        const int branchX = worldX + dx * forward;
        const int branchZ = worldZ + dz * forward;
        const int branchY = shelfY + (forward >= 2 && ((h >> static_cast<std::uint32_t>(forward + 10U)) & 1U) != 0U ? 1 : 0);
        placeIfAir(chunk, coord, branchX, branchY, branchZ, BlockType::JungleTreeTrunk);

        for (int side = -shelfHalfWidth; side <= shelfHalfWidth; ++side)
        {
            placeIfAir(
                chunk,
                coord,
                branchX + sideX * side,
                branchY,
                branchZ + sideZ * side,
                BlockType::JungleTreeCrown);

            if (forward == shelfReach || std::abs(side) == shelfHalfWidth)
            {
                placeIfAir(
                    chunk,
                    coord,
                    branchX + sideX * side,
                    branchY + 1,
                    branchZ + sideZ * side,
                    BlockType::JungleTreeCrown);
            }

            const bool hangPoint = forward >= 1 && (forward == shelfReach || std::abs(side) == shelfHalfWidth);
            if (!hangPoint)
            {
                continue;
            }

            const int vineLength = 2 + static_cast<int>((h >> static_cast<std::uint32_t>(forward + side + 16)) & 3U);
            for (int i = 1; i <= vineLength; ++i)
            {
                placeIfAir(
                    chunk,
                    coord,
                    branchX + sideX * side,
                    branchY - i,
                    branchZ + sideZ * side,
                    BlockType::Vines);
            }
        }
    }

    const int fanBaseX = worldX + dx * shelfReach;
    const int fanBaseZ = worldZ + dz * shelfReach;
    for (int sideSign : {-1, 1})
    {
        const int fanX = fanBaseX + sideX * sideSign * (shelfHalfWidth + 1);
        const int fanZ = fanBaseZ + sideZ * sideSign * (shelfHalfWidth + 1);
        const int fanHeight = 3 + static_cast<int>((h >> static_cast<std::uint32_t>(20 + sideSign + 1)) % 3U);
        for (int y = 0; y < fanHeight; ++y)
        {
            placeIfAir(chunk, coord, fanX, shelfY + y, fanZ, BlockType::Bamboo);
        }
        placeIfAir(chunk, coord, fanX, shelfY + fanHeight, fanZ, BlockType::JungleTreeCrown);
    }

    if (((h >> 26U) & 1U) != 0U)
    {
        placeIfAir(chunk, coord, worldX, shelfY - 1, worldZ, BlockType::CocoaPod);
    }
}

void decorateCrystalRidgeWall(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kCrystalRidgeWallRollSeed) >= 0.07f)
    {
        return;
    }

    const auto [dropDx, dropDz] = steepestDropDirection(terrainGenerator, worldX, worldZ, surfaceY);
    if (dropDx == 0 && dropDz == 0)
    {
        return;
    }

    const int alongX = -dropDz;
    const int alongZ = dropDx;
    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kCrystalRidgeWallSeed);
    const int halfLength = 1 + static_cast<int>(h % 2U);
    const int maxHeight = 3 + static_cast<int>((h >> 3U) % 2U);

    for (int step = -halfLength; step <= halfLength; ++step)
    {
        const int ridgeX = worldX + alongX * step;
        const int ridgeZ = worldZ + alongZ * step;
        const int ridgeSurfaceY = terrainGenerator.surfaceHeightAt(ridgeX, ridgeZ);
        const int wallHeight = std::max(2, maxHeight - std::abs(step) / 2);
        for (int y = 1; y <= wallHeight; ++y)
        {
            placeIfAir(chunk, coord, ridgeX, ridgeSurfaceY + y, ridgeZ, BlockType::Glowstone);
        }

        if (((h >> static_cast<std::uint32_t>(step + halfLength + 8)) & 1U) != 0U)
        {
            placeIfAir(
                chunk,
                coord,
                ridgeX + dropDx,
                ridgeSurfaceY + wallHeight - 1,
                ridgeZ + dropDz,
                BlockType::Glowstone);
        }
    }
}

void decorateSandstoneArch(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kSandstoneArchRollSeed) >= 0.08f)
    {
        return;
    }

    const auto [dx, dz] = steepestDropDirection(terrainGenerator, worldX, worldZ, surfaceY);
    if (dx == 0 && dz == 0)
    {
        return;
    }

    const int span = 2 + static_cast<int>(noise::hashCoordinates(worldX, worldZ, kSandstoneArchSeed) % 2U);
    const int farX = worldX + dx * span;
    const int farZ = worldZ + dz * span;
    const int farSurfaceY = terrainGenerator.surfaceHeightAt(farX, farZ);
    if (surfaceY - farSurfaceY < 6)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kSandstoneArchSeed);
    int startTopY = surfaceY + 3 + static_cast<int>((h >> 4U) % 2U);
    int farTopY = farSurfaceY + 2 + static_cast<int>((h >> 7U) % 2U);
    const int bridgeY = std::max(startTopY, farTopY);
    startTopY = bridgeY;
    farTopY = bridgeY;

    for (int y = surfaceY + 1; y <= startTopY; ++y)
    {
        placeIfAir(chunk, coord, worldX, y, worldZ, BlockType::Sandstone);
    }
    for (int y = farSurfaceY + 1; y <= farTopY; ++y)
    {
        placeIfAir(chunk, coord, farX, y, farZ, BlockType::Sandstone);
    }

    const int sideX = -dz;
    const int sideZ = dx;
    for (int forward = 0; forward <= span; ++forward)
    {
        const int beamX = worldX + dx * forward;
        const int beamZ = worldZ + dz * forward;
        const int lift = (forward > 0 && forward < span) ? 1 : 0;
        placeIfAir(chunk, coord, beamX, bridgeY + lift, beamZ, BlockType::Sandstone);
        if (((h >> static_cast<std::uint32_t>(forward + 10U)) & 1U) != 0U)
        {
            placeIfAir(chunk, coord, beamX + sideX, bridgeY + lift - 1, beamZ + sideZ, BlockType::Sandstone);
        }
    }
}

void decorateSkyrootBridge(
    Chunk& chunk,
    const ChunkCoord& coord,
    const TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const int surfaceY)
{
    if (noise::random01(worldX, worldZ, kSkyrootBridgeRollSeed) >= 0.06f)
    {
        return;
    }

    const auto [dx, dz] = steepestDropDirection(terrainGenerator, worldX, worldZ, surfaceY);
    if (dx == 0 && dz == 0)
    {
        return;
    }

    const int neighborY = terrainGenerator.surfaceHeightAt(worldX + dx, worldZ + dz);
    if (surfaceY - neighborY < 9)
    {
        return;
    }

    const std::uint32_t h = noise::hashCoordinates(worldX, worldZ, kSkyrootBridgeSeed);
    const int pedestalHeight = 3 + static_cast<int>(h % 3U);
    const int reach = 3 + static_cast<int>((h >> 4U) % 2U);
    const int shelfY = surfaceY + pedestalHeight;
    const int sideX = -dz;
    const int sideZ = dx;

    for (int y = surfaceY + 1; y <= shelfY; ++y)
    {
        placeIfAir(chunk, coord, worldX, y, worldZ, BlockType::JungleTreeTrunk);
        if (((h >> 8U) & 1U) != 0U)
        {
            placeIfAir(chunk, coord, worldX + sideX, y, worldZ + sideZ, BlockType::JungleTreeTrunk);
        }
    }

    for (int forward = 0; forward <= reach; ++forward)
    {
        const int bridgeX = worldX + dx * forward;
        const int bridgeZ = worldZ + dz * forward;
        const int bridgeY = shelfY + (forward > 1 && forward < reach ? 1 : 0);
        placeIfAir(chunk, coord, bridgeX, bridgeY, bridgeZ, BlockType::JungleTreeTrunk);
        for (int side = -1; side <= 1; ++side)
        {
            placeIfAir(
                chunk,
                coord,
                bridgeX + sideX * side,
                bridgeY,
                bridgeZ + sideZ * side,
                BlockType::JungleTreeCrown);
            if (forward >= 1 && std::abs(side) == 1)
            {
                const int vineLength = 2 + static_cast<int>((h >> static_cast<std::uint32_t>(forward + side + 14)) & 3U);
                for (int i = 1; i <= vineLength; ++i)
                {
                    placeIfAir(
                        chunk,
                        coord,
                        bridgeX + sideX * side,
                        bridgeY - i,
                        bridgeZ + sideZ * side,
                        BlockType::Vines);
                }
            }
        }
    }

    const int crownX = worldX + dx * reach;
    const int crownZ = worldZ + dz * reach;
    for (int crownDz = -1; crownDz <= 1; ++crownDz)
    {
        for (int crownDx = -1; crownDx <= 1; ++crownDx)
        {
            if (std::abs(crownDx) == 1 && std::abs(crownDz) == 1)
            {
                continue;
            }
            placeIfAir(chunk, coord, crownX + crownDx, shelfY + 1, crownZ + crownDz, BlockType::JungleTreeCrown);
        }
    }
}
}  // namespace

void populateBiomeLandmarksForChunk(
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
            if (surfaceY < kWorldMinY || surfaceY >= kWorldMaxY - 8)
            {
                continue;
            }

            if (chunk.blockAt(localX, surfaceY + 1, localZ) != BlockType::Air)
            {
                continue;
            }

            const SurfaceBiome biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            const BlockType surfaceBlock = chunk.blockAt(localX, surfaceY, localZ);
            const int slope = maxNeighborSlope(terrainGenerator, worldX, worldZ, surfaceY);
            const int relief = localRelief(terrainGenerator, worldX, worldZ);

            if (biome == SurfaceBiome::Snowy
                && (surfaceBlock == BlockType::SnowGrass
                    || surfaceBlock == BlockType::Gravel
                    || surfaceBlock == BlockType::Stone)
                && slope >= 7
                && relief >= 14)
            {
                decorateCrystalRidgeWall(chunk, coord, terrainGenerator, worldX, worldZ, surfaceY);
            }
            else if (biome == SurfaceBiome::Snowy
                && (surfaceBlock == BlockType::SnowGrass
                    || surfaceBlock == BlockType::Gravel
                    || surfaceBlock == BlockType::Stone)
                && slope >= 1
                && relief >= 2)
            {
                decorateCrystalSpire(chunk, coord, worldX, worldZ, surfaceY);
            }
            else if (biome == SurfaceBiome::Sandy
                && (surfaceBlock == BlockType::Sand || surfaceBlock == BlockType::Gravel)
                && slope >= 7
                && relief >= 12)
            {
                decorateSandstoneArch(chunk, coord, terrainGenerator, worldX, worldZ, surfaceY);
            }
            else if (biome == SurfaceBiome::Sandy
                && (surfaceBlock == BlockType::Sand || surfaceBlock == BlockType::Gravel || surfaceBlock == BlockType::Sandstone)
                && slope >= 1
                && relief >= 1)
            {
                decorateSandstoneOutcrop(chunk, coord, worldX, worldZ, surfaceY);
            }
            else if ((biome == SurfaceBiome::Jungle || biome == SurfaceBiome::TemperateGrassland)
                && (surfaceBlock == BlockType::Grass
                    || surfaceBlock == BlockType::JungleGrass
                    || surfaceBlock == BlockType::MossBlock)
                && biome == SurfaceBiome::Jungle
                && slope >= 10
                && relief >= 18)
            {
                decorateSkyrootBridge(chunk, coord, terrainGenerator, worldX, worldZ, surfaceY);
            }
            else if ((biome == SurfaceBiome::Jungle || biome == SurfaceBiome::TemperateGrassland)
                && (surfaceBlock == BlockType::Grass
                    || surfaceBlock == BlockType::JungleGrass
                    || surfaceBlock == BlockType::MossBlock)
                && biome == SurfaceBiome::Jungle
                && slope >= 3
                && relief >= 6)
            {
                decorateGlowForestShelf(chunk, coord, terrainGenerator, worldX, worldZ, surfaceY);
            }
            else if ((biome == SurfaceBiome::Jungle || biome == SurfaceBiome::TemperateGrassland)
                && (surfaceBlock == BlockType::Grass
                    || surfaceBlock == BlockType::JungleGrass
                    || surfaceBlock == BlockType::MossBlock)
                && slope <= 5
                && relief >= 6
                && relief <= 15)
            {
                decorateGlowForestTower(chunk, coord, worldX, worldZ, surfaceY, biome);
            }
            else if ((biome == SurfaceBiome::Jungle || biome == SurfaceBiome::TemperateGrassland)
                && (surfaceBlock == BlockType::Grass
                    || surfaceBlock == BlockType::JungleGrass
                    || surfaceBlock == BlockType::MossBlock)
                && slope >= 9
                && relief >= 20)
            {
                decorateCliffGreenery(chunk, coord, terrainGenerator, worldX, worldZ, surfaceY, biome);
            }
        }
    }
}
}  // namespace vibecraft::world
