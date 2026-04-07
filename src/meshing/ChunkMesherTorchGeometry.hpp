#pragma once

#include <cstdint>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::meshing
{
void appendTorchGeometry(
    ChunkMeshData& meshData,
    vibecraft::world::BlockType blockType,
    int worldX,
    int y,
    int worldZ,
    std::uint8_t tileIndex,
    std::uint32_t baseAbgr,
    std::uint32_t flameAbgr);
}  // namespace vibecraft::meshing
