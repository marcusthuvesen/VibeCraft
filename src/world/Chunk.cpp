#include "vibecraft/world/Chunk.hpp"

#include <algorithm>

namespace vibecraft::world
{
namespace
{
constexpr int kChunkMask = Chunk::kSize - 1;
}

std::size_t ChunkCoordHash::operator()(const ChunkCoord& coord) const noexcept
{
    const auto x = static_cast<std::uint32_t>(coord.x);
    const auto z = static_cast<std::uint32_t>(coord.z);
    return (static_cast<std::size_t>(x) << 32U) ^ static_cast<std::size_t>(z);
}

Chunk::Chunk(const ChunkCoord coord) : coord_(coord)
{
    blocks_.fill(BlockType::Air);
}

ChunkCoord Chunk::coord() const
{
    return coord_;
}

BlockType Chunk::blockAt(const int localX, const int y, const int localZ) const
{
    if (!isInBounds(localX, y, localZ))
    {
        return BlockType::Air;
    }

    return blocks_[toIndex(localX, y, localZ)];
}

bool Chunk::setBlock(const int localX, const int y, const int localZ, const BlockType blockType)
{
    if (!isInBounds(localX, y, localZ))
    {
        return false;
    }

    const std::size_t index = toIndex(localX, y, localZ);
    if (blocks_[index] == blockType)
    {
        return false;
    }

    blocks_[index] = blockType;
    return true;
}

const std::array<BlockType, Chunk::kBlockCount>& Chunk::blockStorage() const
{
    return blocks_;
}

std::array<BlockType, Chunk::kBlockCount>& Chunk::mutableBlockStorage()
{
    return blocks_;
}

bool Chunk::isInBounds(const int localX, const int y, const int localZ)
{
    return localX >= 0 && localX < kSize && y >= 0 && y < kHeight && localZ >= 0 && localZ < kSize;
}

std::size_t Chunk::toIndex(const int localX, const int y, const int localZ)
{
    return static_cast<std::size_t>(y * kSize * kSize + localZ * kSize + localX);
}

ChunkCoord worldToChunkCoord(const int worldX, const int worldZ)
{
    auto floorDiv = [](const int value) {
        return value >= 0 ? value / Chunk::kSize : (value - kChunkMask) / Chunk::kSize;
    };

    return {
        .x = floorDiv(worldX),
        .z = floorDiv(worldZ),
    };
}

int worldToLocalCoord(const int worldCoord)
{
    return worldCoord & kChunkMask;
}

std::vector<ChunkCoord> neighboringChunkCoords(const ChunkCoord& coord)
{
    return {
        coord,
        ChunkCoord{coord.x + 1, coord.z},
        ChunkCoord{coord.x - 1, coord.z},
        ChunkCoord{coord.x, coord.z + 1},
        ChunkCoord{coord.x, coord.z - 1},
    };
}
}  // namespace vibecraft::world
