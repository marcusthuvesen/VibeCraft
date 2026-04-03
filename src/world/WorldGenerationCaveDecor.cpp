#include "WorldGenerationDetail.hpp"

#include <glm/vec3.hpp>

#include <array>

#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/underground/CaveBiomeRules.hpp"

namespace vibecraft::world::detail
{
namespace
{
constexpr std::array<glm::ivec3, 6> kNeighborOffsets{{
    {1, 0, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {0, 0, 1},
    {0, 0, -1},
}};

[[nodiscard]] bool isCaveOpenBlock(const BlockType blockType)
{
    return blockType == BlockType::Air || blockType == BlockType::Water || blockType == BlockType::Lava;
}

[[nodiscard]] bool isCarvableCaveHost(const BlockType blockType)
{
    return blockType == BlockType::Stone || blockType == BlockType::Deepslate || blockType == BlockType::MossyCobblestone;
}

[[nodiscard]] BlockType caveBiomeSurfaceReplacement(
    const underground::CaveBiome caveBiome,
    const bool openAbove,
    const bool openBelow,
    const bool openSide,
    const int worldX,
    const int y,
    const int worldZ)
{
    const double roll = noise::random01(worldX + y * 13, worldZ - y * 7, 0xc4beb10cU);
    switch (caveBiome)
    {
    case underground::CaveBiome::Lush:
        if (openAbove && roll < 0.52)
        {
            return BlockType::MossBlock;
        }
        if (openSide && roll < 0.22)
        {
            return BlockType::MossyCobblestone;
        }
        if (openBelow && roll < 0.18)
        {
            return BlockType::MossBlock;
        }
        break;
    case underground::CaveBiome::Dripstone:
        if ((openAbove || openBelow) && roll < 0.44)
        {
            return BlockType::DripstoneBlock;
        }
        if (openSide && roll < 0.18)
        {
            return BlockType::DripstoneBlock;
        }
        break;
    case underground::CaveBiome::DeepDark:
        if (openAbove && roll < 0.58)
        {
            return BlockType::SculkBlock;
        }
        if (openSide && roll < 0.32)
        {
            return BlockType::SculkBlock;
        }
        if (openBelow && roll < 0.20)
        {
            return BlockType::SculkBlock;
        }
        break;
    case underground::CaveBiome::Default:
    default:
        break;
    }

    return BlockType::Air;
}
}  // namespace

void populateCaveDecorForChunk(Chunk& chunk, const ChunkCoord& coord, const TerrainGenerator& terrainGenerator)
{
    for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < Chunk::kSize; ++localX)
        {
            const int worldX = coord.x * Chunk::kSize + localX;
            const int worldZ = coord.z * Chunk::kSize + localZ;
            const int surfaceHeight = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const SurfaceBiome surfaceBiome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);

            for (int y = kWorldMinY + 1; y <= surfaceHeight - 18; ++y)
            {
                const BlockType blockType = chunk.blockAt(localX, y, localZ);
                const bool canUseLocalNeighbors = y > kWorldMinY && y < kWorldMaxY;
                if (isCarvableCaveHost(blockType) && canUseLocalNeighbors)
                {
                    bool openAbove = false;
                    bool openBelow = false;
                    bool openSide = false;
                    for (const glm::ivec3& offset : kNeighborOffsets)
                    {
                        const int neighborLocalX = localX + offset.x;
                        const int neighborLocalZ = localZ + offset.z;
                        if (neighborLocalX < 0 || neighborLocalX >= Chunk::kSize
                            || neighborLocalZ < 0 || neighborLocalZ >= Chunk::kSize)
                        {
                            continue;
                        }
                        const BlockType neighborBlock = chunk.blockAt(neighborLocalX, y + offset.y, neighborLocalZ);
                        if (!isCaveOpenBlock(neighborBlock))
                        {
                            continue;
                        }
                        if (offset.y > 0)
                        {
                            openAbove = true;
                        }
                        else if (offset.y < 0)
                        {
                            openBelow = true;
                        }
                        else
                        {
                            openSide = true;
                        }
                    }

                    if (!(openAbove || openBelow || openSide))
                    {
                        continue;
                    }
                    const underground::CaveBiome caveBiome =
                        underground::sampleCaveBiome(worldX, y, worldZ, surfaceHeight, surfaceBiome);
                    const BlockType replacement =
                        caveBiomeSurfaceReplacement(caveBiome, openAbove, openBelow, openSide, worldX, y, worldZ);
                    if (replacement != BlockType::Air)
                    {
                        chunk.setBlock(localX, y, localZ, replacement);
                        continue;
                    }
                }

                if (blockType != BlockType::Air || !canUseLocalNeighbors)
                {
                    continue;
                }

                const BlockType aboveBlock = chunk.blockAt(localX, y + 1, localZ);
                const BlockType belowBlock = chunk.blockAt(localX, y - 1, localZ);
                if (!isSolid(aboveBlock) || belowBlock != BlockType::Air)
                {
                    continue;
                }
                const underground::CaveBiome caveBiome =
                    underground::sampleCaveBiome(worldX, y, worldZ, surfaceHeight, surfaceBiome);
                if (caveBiome != underground::CaveBiome::Lush)
                {
                    continue;
                }

                const double vineRoll = noise::random01(worldX - y * 9, worldZ + y * 5, 0x1a57ca0eU);
                if (vineRoll < 0.10)
                {
                    chunk.setBlock(localX, y, localZ, BlockType::Vines);
                }
            }
        }
    }
}
}  // namespace vibecraft::world::detail
