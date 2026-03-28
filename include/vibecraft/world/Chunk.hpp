#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <compare>
#include <vector>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world
{
struct ChunkCoord
{
    std::int32_t x = 0;
    std::int32_t z = 0;

    auto operator<=>(const ChunkCoord&) const = default;
};

struct ChunkCoordHash
{
    std::size_t operator()(const ChunkCoord& coord) const noexcept;
};

class Chunk
{
  public:
    static constexpr int kSize = 16;
    static constexpr int kHeight = kWorldHeight;
    static constexpr int kBlockCount = kSize * kSize * kHeight;

    explicit Chunk(ChunkCoord coord = {});

    [[nodiscard]] ChunkCoord coord() const;
    [[nodiscard]] BlockType blockAt(int localX, int y, int localZ) const;
    bool setBlock(int localX, int y, int localZ, BlockType blockType);

    [[nodiscard]] const std::array<BlockType, kBlockCount>& blockStorage() const;
    [[nodiscard]] std::array<BlockType, kBlockCount>& mutableBlockStorage();

  private:
    [[nodiscard]] static bool isInBounds(int localX, int y, int localZ);
    [[nodiscard]] static std::size_t toIndex(int localX, int y, int localZ);

    ChunkCoord coord_;
    std::array<BlockType, kBlockCount> blocks_;
};

[[nodiscard]] ChunkCoord worldToChunkCoord(int worldX, int worldZ);
[[nodiscard]] int worldToLocalCoord(int worldCoord);
[[nodiscard]] std::vector<ChunkCoord> neighboringChunkCoords(const ChunkCoord& coord);
}  // namespace vibecraft::world
