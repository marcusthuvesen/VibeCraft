#pragma once

#include <vector>

#include "vibecraft/world/Chunk.hpp"

namespace vibecraft::world
{
class World;
}

namespace vibecraft::meshing
{
struct TorchEmitter
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

[[nodiscard]] std::vector<TorchEmitter> collectNearbyTorchEmitters(
    const vibecraft::world::World& world,
    const vibecraft::world::ChunkCoord& center);

[[nodiscard]] float torchLightMultiplierAt(
    float x,
    float y,
    float z,
    const std::vector<TorchEmitter>& emitters);
}  // namespace vibecraft::meshing
