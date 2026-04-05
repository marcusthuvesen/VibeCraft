#include "ChunkMesherTorchLighting.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::meshing
{
namespace
{
using BlockStorage = std::array<vibecraft::world::BlockType, vibecraft::world::Chunk::kBlockCount>;

[[nodiscard]] constexpr std::size_t chunkStorageIndex(
    const int localX,
    const int y,
    const int localZ)
{
    const int localY = y - vibecraft::world::kWorldMinY;
    return static_cast<std::size_t>(localY * vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize
        + localZ * vibecraft::world::Chunk::kSize
        + localX);
}

[[nodiscard]] vibecraft::world::BlockType blockFromStorage(
    const BlockStorage& storage,
    const int localX,
    const int y,
    const int localZ)
{
    return storage[chunkStorageIndex(localX, y, localZ)];
}
}  // namespace

std::vector<TorchEmitter> collectNearbyTorchEmitters(
    const vibecraft::world::World& world,
    const vibecraft::world::ChunkCoord& center)
{
    constexpr int kTorchEmitterChunkRadius = 1;
    std::vector<TorchEmitter> emitters;
    for (const auto& [chunkCoord, chunk] : world.chunks())
    {
        if (std::abs(chunkCoord.x - center.x) > kTorchEmitterChunkRadius
            || std::abs(chunkCoord.z - center.z) > kTorchEmitterChunkRadius)
        {
            continue;
        }

        const BlockStorage& storage = chunk.blockStorage();
        const int chunkBaseX = chunkCoord.x * vibecraft::world::Chunk::kSize;
        const int chunkBaseZ = chunkCoord.z * vibecraft::world::Chunk::kSize;
        for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
        {
            for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
            {
                for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
                {
                    const vibecraft::world::BlockType torchBlockType =
                        blockFromStorage(storage, localX, y, localZ);
                    if (!vibecraft::world::isTorchBlock(torchBlockType))
                    {
                        continue;
                    }

                    float emitterOffsetX = 0.5f;
                    float emitterOffsetY = 0.72f;
                    float emitterOffsetZ = 0.5f;
                    switch (torchBlockType)
                    {
                    case vibecraft::world::BlockType::TorchNorth:
                        emitterOffsetZ = 0.30f;
                        emitterOffsetY = 0.80f;
                        break;
                    case vibecraft::world::BlockType::TorchEast:
                        emitterOffsetX = 0.70f;
                        emitterOffsetY = 0.80f;
                        break;
                    case vibecraft::world::BlockType::TorchSouth:
                        emitterOffsetZ = 0.70f;
                        emitterOffsetY = 0.80f;
                        break;
                    case vibecraft::world::BlockType::TorchWest:
                        emitterOffsetX = 0.30f;
                        emitterOffsetY = 0.80f;
                        break;
                    case vibecraft::world::BlockType::Torch:
                    default:
                        break;
                    }

                    emitters.push_back(TorchEmitter{
                        .x = static_cast<float>(chunkBaseX + localX) + emitterOffsetX,
                        .y = static_cast<float>(y) + emitterOffsetY,
                        .z = static_cast<float>(chunkBaseZ + localZ) + emitterOffsetZ,
                    });
                }
            }
        }
    }
    return emitters;
}

float torchLightMultiplierAt(
    const float x,
    const float y,
    const float z,
    const std::vector<TorchEmitter>& emitters)
{
    constexpr float kTorchLightRadius = 8.0f;
    constexpr float kTorchLightStrength = 0.85f;
    constexpr float kTorchLightRadiusSq = kTorchLightRadius * kTorchLightRadius;

    float bestIntensity = 0.0f;
    for (const TorchEmitter& emitter : emitters)
    {
        const float dx = x - emitter.x;
        const float dy = y - emitter.y;
        const float dz = z - emitter.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (distanceSq >= kTorchLightRadiusSq)
        {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float linear = 1.0f - distance / kTorchLightRadius;
        const float intensity = linear * linear * (3.0f - 2.0f * linear);
        bestIntensity = std::max(bestIntensity, intensity);
    }

    return 1.0f + bestIntensity * kTorchLightStrength;
}
}  // namespace vibecraft::meshing
