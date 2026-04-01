#pragma once

#include "vibecraft/world/Block.hpp"

#include <optional>

namespace vibecraft::world
{
enum class SurfaceBiome : std::uint8_t
{
    TemperateGrassland,
    Sandy,
    Snowy,
    Jungle,
};

[[nodiscard]] const char* surfaceBiomeLabel(SurfaceBiome biome);

class TerrainGenerator
{
  public:
    [[nodiscard]] std::uint32_t worldSeed() const;
    void setWorldSeed(std::uint32_t worldSeed);
    [[nodiscard]] std::optional<SurfaceBiome> biomeOverride() const;
    void setBiomeOverride(std::optional<SurfaceBiome> biomeOverride);
    [[nodiscard]] int surfaceHeightAt(int worldX, int worldZ) const;
    [[nodiscard]] SurfaceBiome surfaceBiomeAt(int worldX, int worldZ) const;
    [[nodiscard]] BlockType blockTypeAt(int worldX, int y, int worldZ) const;
    /// Fills one full world-height column; `outColumnBlocks` must point to `kWorldHeight` entries.
    void fillColumn(int worldX, int worldZ, BlockType* outColumnBlocks) const;

  private:
    std::uint32_t worldSeed_ = 0;
    std::optional<SurfaceBiome> biomeOverride_;
};
}  // namespace vibecraft::world
