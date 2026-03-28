#pragma once

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world
{
class TerrainGenerator
{
  public:
    [[nodiscard]] int surfaceHeightAt(int worldX, int worldZ) const;
    [[nodiscard]] BlockType blockTypeAt(int worldX, int y, int worldZ) const;
};
}  // namespace vibecraft::world
