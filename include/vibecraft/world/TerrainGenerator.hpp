#pragma once

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world
{
enum class SurfaceBiome : std::uint8_t
{
    TemperateGrassland,
    Sandy,
    Snowy,
    Jungle,
};

class TerrainGenerator
{
  public:
    [[nodiscard]] std::uint32_t worldSeed() const;
    void setWorldSeed(std::uint32_t worldSeed);
    [[nodiscard]] int surfaceHeightAt(int worldX, int worldZ) const;
    [[nodiscard]] SurfaceBiome surfaceBiomeAt(int worldX, int worldZ) const;
    [[nodiscard]] BlockType blockTypeAt(int worldX, int y, int worldZ) const;
    /// Fills one full world-height column; `outColumnBlocks` must point to `kWorldHeight` entries.
    void fillColumn(int worldX, int worldZ, BlockType* outColumnBlocks) const;

  private:
    std::uint32_t worldSeed_ = 0;
};
}  // namespace vibecraft::world
