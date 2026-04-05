#pragma once

#include <cstdint>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::meshing
{
using AppendBoxQuadsFn = void (*)(
    ChunkMeshData& meshData,
    int worldX,
    int y,
    int worldZ,
    float x0,
    float x1,
    float y0,
    float y1,
    float z0,
    float z1,
    std::uint8_t topTile,
    std::uint8_t bottomTile,
    std::uint8_t sideTile,
    std::uint32_t abgr);

void appendTorchGeometry(
    ChunkMeshData& meshData,
    vibecraft::world::BlockType blockType,
    int worldX,
    int y,
    int worldZ,
    std::uint8_t tileIndex,
    std::uint32_t baseAbgr,
    std::uint32_t flameAbgr,
    AppendBoxQuadsFn appendBoxQuads);
}  // namespace vibecraft::meshing
