#pragma once

#include <vector>

#include "ChunkMesherTorchLighting.hpp"
#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::world
{
class World;
}

namespace vibecraft::meshing
{
void appendFluidGeometry(
    ChunkMeshData& meshData,
    const vibecraft::world::World& world,
    vibecraft::world::BlockType blockType,
    int worldX,
    int y,
    int worldZ,
    const vibecraft::world::BlockMetadata& metadata,
    const std::vector<TorchEmitter>& torchEmitters);
}  // namespace vibecraft::meshing
