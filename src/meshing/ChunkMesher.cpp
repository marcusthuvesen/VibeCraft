#include "vibecraft/meshing/ChunkMesher.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::meshing
{
namespace
{
using vibecraft::world::BlockType;
using BlockStorage = std::array<BlockType, vibecraft::world::Chunk::kBlockCount>;

struct FaceDefinition
{
    int offsetX;
    int offsetY;
    int offsetZ;
    vibecraft::world::BlockFace blockFace;
    std::array<std::array<float, 3>, 4> corners;
    std::array<std::array<float, 2>, 4> uvs;
};

constexpr std::array<FaceDefinition, 6> kFaces{{
    {1, 0, 0, vibecraft::world::BlockFace::East,
     {{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {-1, 0, 0, vibecraft::world::BlockFace::West,
     {{{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {0, 1, 0, vibecraft::world::BlockFace::Top,
     {{{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}}}},
    {0, -1, 0, vibecraft::world::BlockFace::Bottom,
     {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}}}},
    {0, 0, 1, vibecraft::world::BlockFace::South,
     {{{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
    {0, 0, -1, vibecraft::world::BlockFace::North,
     {{{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}},
     {{{0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}}}},
}};

[[nodiscard]] constexpr bool usesCrossPlantMesh(const BlockType blockType)
{
    return blockType == BlockType::Dandelion || blockType == BlockType::Poppy
        || blockType == BlockType::BlueOrchid || blockType == BlockType::Allium
        || blockType == BlockType::OxeyeDaisy || blockType == BlockType::BrownMushroom
        || blockType == BlockType::RedMushroom || blockType == BlockType::Vines
        || blockType == BlockType::Ladder
        || blockType == BlockType::Bamboo
        || blockType == BlockType::Fern
        || blockType == BlockType::GrassTuft || blockType == BlockType::FlowerTuft
        || blockType == BlockType::DryTuft || blockType == BlockType::LushTuft
        || blockType == BlockType::FrostTuft || blockType == BlockType::SparseTuft
        || blockType == BlockType::CloverTuft || blockType == BlockType::SproutTuft;
}

[[nodiscard]] constexpr float crossPlantInset(const BlockType blockType)
{
    if (blockType == BlockType::Bamboo)
    {
        return 0.33f;
    }
    if (blockType == BlockType::Ladder)
    {
        return 0.42f;
    }
    return 0.18f;
}

[[nodiscard]] constexpr bool usesDenseFloraMesh(const BlockType blockType)
{
    return blockType == BlockType::GrassTuft || blockType == BlockType::FlowerTuft
        || blockType == BlockType::DryTuft || blockType == BlockType::LushTuft
        || blockType == BlockType::FrostTuft || blockType == BlockType::Dandelion
        || blockType == BlockType::Fern
        || blockType == BlockType::SparseTuft || blockType == BlockType::CloverTuft
        || blockType == BlockType::SproutTuft
        || blockType == BlockType::Poppy || blockType == BlockType::BlueOrchid
        || blockType == BlockType::Allium || blockType == BlockType::OxeyeDaisy
        || blockType == BlockType::BrownMushroom || blockType == BlockType::RedMushroom;
}

[[nodiscard]] constexpr bool usesTuftTriCrossMesh(const BlockType blockType)
{
    return blockType == BlockType::GrassTuft || blockType == BlockType::FlowerTuft
        || blockType == BlockType::DryTuft || blockType == BlockType::LushTuft
        || blockType == BlockType::FrostTuft || blockType == BlockType::SparseTuft
        || blockType == BlockType::CloverTuft || blockType == BlockType::SproutTuft;
}

[[nodiscard]] std::uint32_t hash32(const int x, const int y, const int z, const std::uint32_t seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x9e3779b9u;
    h ^= static_cast<std::uint32_t>(y) * 0x85ebca6bu + seed;
    h ^= static_cast<std::uint32_t>(z) * 0xc2b2ae35u + 0x165667b1u;
    h ^= h >> 16u;
    h *= 0x7feb352du;
    h ^= h >> 15u;
    h *= 0x846ca68bu;
    h ^= h >> 16u;
    return h;
}

[[nodiscard]] float hash01(const int x, const int y, const int z, const std::uint32_t seed)
{
    return static_cast<float>(hash32(x, y, z, seed)) * (1.0f / 4294967295.0f);
}

[[nodiscard]] std::uint32_t modulateAbgrRgb(const std::uint32_t abgr, const float factor)
{
    const float f = std::clamp(factor, 0.0f, 2.0f);
    const auto scale = [f](const std::uint8_t channel)
    {
        const int scaled = static_cast<int>(std::lround(static_cast<float>(channel) * f));
        return static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
    };

    const std::uint8_t r = static_cast<std::uint8_t>(abgr & 0xffu);
    const std::uint8_t g = static_cast<std::uint8_t>((abgr >> 8u) & 0xffu);
    const std::uint8_t b = static_cast<std::uint8_t>((abgr >> 16u) & 0xffu);
    const std::uint8_t a = static_cast<std::uint8_t>((abgr >> 24u) & 0xffu);
    return (static_cast<std::uint32_t>(a) << 24u)
        | (static_cast<std::uint32_t>(scale(b)) << 16u)
        | (static_cast<std::uint32_t>(scale(g)) << 8u)
        | static_cast<std::uint32_t>(scale(r));
}

constexpr std::uint16_t kAtlasColumns = vibecraft::kChunkAtlasTileColumns;
constexpr std::uint16_t kAtlasRows = vibecraft::kChunkAtlasTileRows;
constexpr float kTileInsetU = 0.5f / static_cast<float>(vibecraft::kChunkAtlasWidthPx);
constexpr float kTileInsetV = 0.5f / static_cast<float>(vibecraft::kChunkAtlasHeightPx);
[[nodiscard]] std::array<float, 2> atlasUvForBlockType(
    const std::uint8_t tileIndex,
    const std::array<float, 2>& faceUv)
{
    const float tileWidth = 1.0f / static_cast<float>(kAtlasColumns);
    const float tileHeight = 1.0f / static_cast<float>(kAtlasRows);
    const float tileX = static_cast<float>(tileIndex % kAtlasColumns);
    const float tileY = static_cast<float>(tileIndex / kAtlasColumns);
    const float minU = tileX * tileWidth + kTileInsetU;
    const float maxU = (tileX + 1.0f) * tileWidth - kTileInsetU;
    const float minV = tileY * tileHeight + kTileInsetV;
    const float maxV = (tileY + 1.0f) * tileHeight - kTileInsetV;
    const float u = minU + (maxU - minU) * faceUv[0];
    const float v = minV + (maxV - minV) * faceUv[1];
    return {u, v};
}

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

[[nodiscard]] BlockType blockFromStorage(
    const BlockStorage& storage,
    const int localX,
    const int y,
    const int localZ)
{
    return storage[chunkStorageIndex(localX, y, localZ)];
}

[[nodiscard]] BlockType sampledNeighborBlock(
    const BlockStorage& currentStorage,
    const vibecraft::world::Chunk* const westChunk,
    const vibecraft::world::Chunk* const eastChunk,
    const vibecraft::world::Chunk* const northChunk,
    const vibecraft::world::Chunk* const southChunk,
    const int localX,
    const int y,
    const int localZ)
{
    if (y < vibecraft::world::kWorldMinY)
    {
        return BlockType::Bedrock;
    }
    if (y > vibecraft::world::kWorldMaxY)
    {
        return BlockType::Air;
    }
    if (localX >= 0 && localX < vibecraft::world::Chunk::kSize
        && localZ >= 0 && localZ < vibecraft::world::Chunk::kSize)
    {
        return blockFromStorage(currentStorage, localX, y, localZ);
    }
    if (localX < 0)
    {
        return westChunk != nullptr
            ? westChunk->blockAt(vibecraft::world::Chunk::kSize - 1, y, localZ)
            : BlockType::Air;
    }
    if (localX >= vibecraft::world::Chunk::kSize)
    {
        return eastChunk != nullptr ? eastChunk->blockAt(0, y, localZ) : BlockType::Air;
    }
    if (localZ < 0)
    {
        return northChunk != nullptr
            ? northChunk->blockAt(localX, y, vibecraft::world::Chunk::kSize - 1)
            : BlockType::Air;
    }
    return southChunk != nullptr ? southChunk->blockAt(localX, y, 0) : BlockType::Air;
}

struct IntOffset
{
    int x = 0;
    int y = 0;
    int z = 0;
};

[[nodiscard]] IntOffset axisOffsetForFace(const FaceDefinition& face, const bool firstAxis)
{
    if (face.offsetX != 0)
    {
        return firstAxis ? IntOffset{0, 1, 0} : IntOffset{0, 0, 1};
    }
    if (face.offsetY != 0)
    {
        return firstAxis ? IntOffset{1, 0, 0} : IntOffset{0, 0, 1};
    }
    return firstAxis ? IntOffset{1, 0, 0} : IntOffset{0, 1, 0};
}

[[nodiscard]] int cornerSignForAxis(
    const IntOffset& axis,
    const std::array<float, 3>& corner)
{
    if (axis.x != 0)
    {
        return corner[0] > 0.5f ? 1 : -1;
    }
    if (axis.y != 0)
    {
        return corner[1] > 0.5f ? 1 : -1;
    }
    return corner[2] > 0.5f ? 1 : -1;
}

[[nodiscard]] float ambientOcclusionForCorner(
    const BlockStorage& currentStorage,
    const vibecraft::world::Chunk* const westChunk,
    const vibecraft::world::Chunk* const eastChunk,
    const vibecraft::world::Chunk* const northChunk,
    const vibecraft::world::Chunk* const southChunk,
    const int localX,
    const int y,
    const int localZ,
    const FaceDefinition& face,
    const std::array<float, 3>& corner)
{
    const IntOffset axisA = axisOffsetForFace(face, true);
    const IntOffset axisB = axisOffsetForFace(face, false);
    const int signA = cornerSignForAxis(axisA, corner);
    const int signB = cornerSignForAxis(axisB, corner);
    const int sampleOriginX = localX + face.offsetX;
    const int sampleOriginY = y + face.offsetY;
    const int sampleOriginZ = localZ + face.offsetZ;

    const bool sideAOccluder = vibecraft::world::occludesFaces(sampledNeighborBlock(
        currentStorage,
        westChunk,
        eastChunk,
        northChunk,
        southChunk,
        sampleOriginX + axisA.x * signA,
        sampleOriginY + axisA.y * signA,
        sampleOriginZ + axisA.z * signA));
    const bool sideBOccluder = vibecraft::world::occludesFaces(sampledNeighborBlock(
        currentStorage,
        westChunk,
        eastChunk,
        northChunk,
        southChunk,
        sampleOriginX + axisB.x * signB,
        sampleOriginY + axisB.y * signB,
        sampleOriginZ + axisB.z * signB));
    const bool cornerOccluder = vibecraft::world::occludesFaces(sampledNeighborBlock(
        currentStorage,
        westChunk,
        eastChunk,
        northChunk,
        southChunk,
        sampleOriginX + axisA.x * signA + axisB.x * signB,
        sampleOriginY + axisA.y * signA + axisB.y * signB,
        sampleOriginZ + axisA.z * signA + axisB.z * signB));

    int occluders = (sideAOccluder ? 1 : 0) + (sideBOccluder ? 1 : 0) + (cornerOccluder ? 1 : 0);
    if (sideAOccluder && sideBOccluder)
    {
        occluders = 3;
    }

    constexpr std::array<float, 4> kAoByOccluderCount{1.0f, 0.93f, 0.86f, 0.80f};
    return kAoByOccluderCount[static_cast<std::size_t>(std::clamp(occluders, 0, 3))];
}

struct TorchEmitter
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

[[nodiscard]] std::vector<TorchEmitter> collectNearbyTorchEmitters(
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
                    const BlockType torchBlockType = blockFromStorage(storage, localX, y, localZ);
                    if (!vibecraft::world::isTorchBlock(torchBlockType))
                    {
                        continue;
                    }
                    float emitterOffsetX = 0.5f;
                    float emitterOffsetY = 0.72f;
                    float emitterOffsetZ = 0.5f;
                    switch (torchBlockType)
                    {
                    case BlockType::TorchNorth:
                        emitterOffsetZ = 0.30f;
                        emitterOffsetY = 0.80f;
                        break;
                    case BlockType::TorchEast:
                        emitterOffsetX = 0.70f;
                        emitterOffsetY = 0.80f;
                        break;
                    case BlockType::TorchSouth:
                        emitterOffsetZ = 0.70f;
                        emitterOffsetY = 0.80f;
                        break;
                    case BlockType::TorchWest:
                        emitterOffsetX = 0.30f;
                        emitterOffsetY = 0.80f;
                        break;
                    case BlockType::Torch:
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

[[nodiscard]] float torchLightMultiplierAt(
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

void appendCustomQuad(
    ChunkMeshData& meshData,
    const std::array<std::array<float, 3>, 4>& corners,
    const std::array<std::array<float, 2>, 4>& uvs,
    const std::array<float, 3>& normal,
    const std::uint8_t tileIndex,
    const std::uint32_t abgr)
{
    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
    for (std::size_t vertexIndex = 0; vertexIndex < corners.size(); ++vertexIndex)
    {
        const auto atlasUv = atlasUvForBlockType(tileIndex, uvs[vertexIndex]);
        meshData.vertices.push_back(DebugVertex{
            .x = corners[vertexIndex][0],
            .y = corners[vertexIndex][1],
            .z = corners[vertexIndex][2],
            .nx = normal[0],
            .ny = normal[1],
            .nz = normal[2],
            .u = atlasUv[0],
            .v = atlasUv[1],
            .abgr = abgr,
        });
    }

    meshData.indices.push_back(baseIndex);
    meshData.indices.push_back(baseIndex + 1);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex);
    meshData.indices.push_back(baseIndex + 2);
    meshData.indices.push_back(baseIndex + 3);
    ++meshData.faceCount;
}

enum class StairFacing : std::uint8_t
{
    North = 0,
    East,
    South,
    West
};

enum class TorchFacing : std::uint8_t
{
    Standing = 0,
    North,
    East,
    South,
    West
};

[[nodiscard]] TorchFacing torchFacingForBlockType(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::TorchNorth:
        return TorchFacing::North;
    case BlockType::TorchEast:
        return TorchFacing::East;
    case BlockType::TorchSouth:
        return TorchFacing::South;
    case BlockType::TorchWest:
        return TorchFacing::West;
    case BlockType::Torch:
    default:
        return TorchFacing::Standing;
    }
}

[[nodiscard]] StairFacing stairFacingForBlockType(const BlockType blockType)
{
    switch (blockType)
    {
    case BlockType::OakStairsNorth:
    case BlockType::CobblestoneStairsNorth:
    case BlockType::StoneStairsNorth:
    case BlockType::BrickStairsNorth:
    case BlockType::SandstoneStairsNorth:
    case BlockType::JungleStairsNorth:
        return StairFacing::North;
    case BlockType::OakStairsEast:
    case BlockType::CobblestoneStairsEast:
    case BlockType::StoneStairsEast:
    case BlockType::BrickStairsEast:
    case BlockType::SandstoneStairsEast:
    case BlockType::JungleStairsEast:
        return StairFacing::East;
    case BlockType::OakStairsWest:
    case BlockType::CobblestoneStairsWest:
    case BlockType::StoneStairsWest:
    case BlockType::BrickStairsWest:
    case BlockType::SandstoneStairsWest:
    case BlockType::JungleStairsWest:
        return StairFacing::West;
    case BlockType::OakStairs:
    case BlockType::OakStairsSouth:
    case BlockType::CobblestoneStairs:
    case BlockType::CobblestoneStairsSouth:
    case BlockType::StoneStairs:
    case BlockType::StoneStairsSouth:
    case BlockType::BrickStairs:
    case BlockType::BrickStairsSouth:
    case BlockType::SandstoneStairs:
    case BlockType::SandstoneStairsSouth:
    case BlockType::JungleStairs:
    case BlockType::JungleStairsSouth:
    default:
        return StairFacing::South;
    }
}

void appendBoxQuads(
    ChunkMeshData& meshData,
    const int worldX,
    const int y,
    const int worldZ,
    const float x0,
    const float x1,
    const float y0,
    const float y1,
    const float z0,
    const float z1,
    const std::uint8_t topTile,
    const std::uint8_t bottomTile,
    const std::uint8_t sideTile,
    const std::uint32_t abgr)
{
    constexpr std::array<std::array<float, 2>, 4> kUv{{
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
    }};
    const float wx = static_cast<float>(worldX);
    const float wy = static_cast<float>(y);
    const float wz = static_cast<float>(worldZ);

    appendCustomQuad(
        meshData,
        {{{wx + x1, wy + y0, wz + z0}, {wx + x1, wy + y1, wz + z0}, {wx + x1, wy + y1, wz + z1}, {wx + x1, wy + y0, wz + z1}}},
        kUv,
        {1.0f, 0.0f, 0.0f},
        sideTile,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z1}, {wx + x0, wy + y1, wz + z1}, {wx + x0, wy + y1, wz + z0}, {wx + x0, wy + y0, wz + z0}}},
        kUv,
        {-1.0f, 0.0f, 0.0f},
        sideTile,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + x0, wy + y1, wz + z1}, {wx + x1, wy + y1, wz + z1}, {wx + x1, wy + y1, wz + z0}, {wx + x0, wy + y1, wz + z0}}},
        kUv,
        {0.0f, 1.0f, 0.0f},
        topTile,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z0}, {wx + x1, wy + y0, wz + z0}, {wx + x1, wy + y0, wz + z1}, {wx + x0, wy + y0, wz + z1}}},
        kUv,
        {0.0f, -1.0f, 0.0f},
        bottomTile,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + x1, wy + y0, wz + z1}, {wx + x1, wy + y1, wz + z1}, {wx + x0, wy + y1, wz + z1}, {wx + x0, wy + y0, wz + z1}}},
        kUv,
        {0.0f, 0.0f, 1.0f},
        sideTile,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z0}, {wx + x0, wy + y1, wz + z0}, {wx + x1, wy + y1, wz + z0}, {wx + x1, wy + y0, wz + z0}}},
        kUv,
        {0.0f, 0.0f, -1.0f},
        sideTile,
        abgr);
}

void appendStairsGeometry(
    ChunkMeshData& meshData,
    const BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint32_t abgr)
{
    const vibecraft::world::BlockMetadata metadata = vibecraft::world::blockMetadata(blockType);
    const std::uint8_t topTile = metadata.textureTiles.top;
    const std::uint8_t bottomTile = metadata.textureTiles.bottom;
    const std::uint8_t sideTile = metadata.textureTiles.side;

    // Bottom half-slab.
    appendBoxQuads(
        meshData,
        worldX,
        y,
        worldZ,
        0.0f,
        1.0f,
        0.0f,
        0.5f,
        0.0f,
        1.0f,
        topTile,
        bottomTile,
        sideTile,
        abgr);

    switch (stairFacingForBlockType(blockType))
    {
    case StairFacing::North:
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.0f,
            1.0f,
            0.5f,
            1.0f,
            0.0f,
            0.5f,
            topTile,
            bottomTile,
            sideTile,
            abgr);
        break;
    case StairFacing::East:
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f,
            1.0f,
            0.5f,
            1.0f,
            0.0f,
            1.0f,
            topTile,
            bottomTile,
            sideTile,
            abgr);
        break;
    case StairFacing::South:
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.0f,
            1.0f,
            0.5f,
            1.0f,
            0.5f,
            1.0f,
            topTile,
            bottomTile,
            sideTile,
            abgr);
        break;
    case StairFacing::West:
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.0f,
            0.5f,
            0.5f,
            1.0f,
            0.0f,
            1.0f,
            topTile,
            bottomTile,
            sideTile,
            abgr);
        break;
    }
}

void appendTorchGeometry(
    ChunkMeshData& meshData,
    const BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint32_t baseAbgr)
{
    const vibecraft::world::BlockMetadata metadata = vibecraft::world::blockMetadata(blockType);
    const std::uint8_t tileIndex = metadata.textureTiles.side;
    const std::uint32_t flameAbgr = modulateAbgrRgb(baseAbgr, 1.35f);
    const TorchFacing facing = torchFacingForBlockType(blockType);
    constexpr float kStemHalfWidth = 1.0f / 16.0f;
    constexpr float kCoreHalfWidth = 1.5f / 16.0f;
    constexpr float kFlameHalfWidth = 2.0f / 16.0f;

    if (facing == TorchFacing::Standing)
    {
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            0.0f,
            0.60f,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            baseAbgr);
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f - kCoreHalfWidth,
            0.5f + kCoreHalfWidth,
            0.60f,
            0.78f,
            0.5f - kCoreHalfWidth,
            0.5f + kCoreHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            baseAbgr);
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f - kFlameHalfWidth,
            0.5f + kFlameHalfWidth,
            0.78f,
            0.92f,
            0.5f - kFlameHalfWidth,
            0.5f + kFlameHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            flameAbgr);
        return;
    }

    auto appendWallTorch = [&](const bool negativeDirection, const bool alongZAxis)
    {
        if (alongZAxis)
        {
            const float baseZ = negativeDirection ? 0.84f : 0.16f;
            const float midZ = negativeDirection ? 0.67f : 0.33f;
            const float tipZ = negativeDirection ? 0.50f : 0.50f;
            const float flameCenterZ = negativeDirection ? 0.39f : 0.61f;
            appendBoxQuads(
                meshData,
                worldX,
                y,
                worldZ,
                0.5f - kStemHalfWidth,
                0.5f + kStemHalfWidth,
                0.18f,
                0.36f,
                std::min(baseZ, midZ) - kStemHalfWidth,
                std::max(baseZ, midZ) + kStemHalfWidth,
                tileIndex,
                tileIndex,
                tileIndex,
                baseAbgr);
            appendBoxQuads(
                meshData,
                worldX,
                y,
                worldZ,
                0.5f - kStemHalfWidth,
                0.5f + kStemHalfWidth,
                0.36f,
                0.56f,
                std::min(midZ, tipZ) - kStemHalfWidth,
                std::max(midZ, tipZ) + kStemHalfWidth,
                tileIndex,
                tileIndex,
                tileIndex,
                baseAbgr);
            appendBoxQuads(
                meshData,
                worldX,
                y,
                worldZ,
                0.5f - kCoreHalfWidth,
                0.5f + kCoreHalfWidth,
                0.56f,
                0.74f,
                std::min(tipZ, flameCenterZ) - kCoreHalfWidth,
                std::max(tipZ, flameCenterZ) + kCoreHalfWidth,
                tileIndex,
                tileIndex,
                tileIndex,
                baseAbgr);
            appendBoxQuads(
                meshData,
                worldX,
                y,
                worldZ,
                0.5f - kFlameHalfWidth,
                0.5f + kFlameHalfWidth,
                0.74f,
                0.90f,
                flameCenterZ - kFlameHalfWidth,
                flameCenterZ + kFlameHalfWidth,
                tileIndex,
                tileIndex,
                tileIndex,
                flameAbgr);
            return;
        }

        const float baseX = negativeDirection ? 0.84f : 0.16f;
        const float midX = negativeDirection ? 0.67f : 0.33f;
        const float tipX = 0.50f;
        const float flameCenterX = negativeDirection ? 0.39f : 0.61f;
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            std::min(baseX, midX) - kStemHalfWidth,
            std::max(baseX, midX) + kStemHalfWidth,
            0.18f,
            0.36f,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            baseAbgr);
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            std::min(midX, tipX) - kStemHalfWidth,
            std::max(midX, tipX) + kStemHalfWidth,
            0.36f,
            0.56f,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            baseAbgr);
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            std::min(tipX, flameCenterX) - kCoreHalfWidth,
            std::max(tipX, flameCenterX) + kCoreHalfWidth,
            0.56f,
            0.74f,
            0.5f - kCoreHalfWidth,
            0.5f + kCoreHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            baseAbgr);
        appendBoxQuads(
            meshData,
            worldX,
            y,
            worldZ,
            flameCenterX - kFlameHalfWidth,
            flameCenterX + kFlameHalfWidth,
            0.74f,
            0.90f,
            0.5f - kFlameHalfWidth,
            0.5f + kFlameHalfWidth,
            tileIndex,
            tileIndex,
            tileIndex,
            flameAbgr);
    };

    switch (facing)
    {
    case TorchFacing::East:
        appendWallTorch(false, false);
        break;
    case TorchFacing::West:
        appendWallTorch(true, false);
        break;
    case TorchFacing::South:
        appendWallTorch(false, true);
        break;
    case TorchFacing::North:
        appendWallTorch(true, true);
        break;
    case TorchFacing::Standing:
    default:
        break;
    }
}

void appendDoorGeometry(
    ChunkMeshData& meshData,
    const BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint32_t abgr)
{
    const vibecraft::world::BlockMetadata metadata = vibecraft::world::blockMetadata(blockType);
    const vibecraft::world::BlockCollisionBox box = vibecraft::world::collisionBoxForBlockType(blockType);
    const std::uint8_t tileIndex = metadata.textureTiles.side;
    constexpr std::array<std::array<float, 2>, 4> kUv{{
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
    }};
    const float wx = static_cast<float>(worldX);
    const float wy = static_cast<float>(y);
    const float wz = static_cast<float>(worldZ);
    const vibecraft::world::DoorFacing facing = vibecraft::world::doorFacingForBlockType(blockType);
    const bool xAxisPlane = vibecraft::world::doorUsesXAxisPlane(facing);
    const bool handleOnPositiveSide = facing == vibecraft::world::DoorFacing::North
        || facing == vibecraft::world::DoorFacing::East;
    const auto maybeFlipHorizontal =
        [&](const std::array<std::array<float, 2>, 4>& baseUv, const bool flip)
    {
        auto uv = baseUv;
        if (!flip)
        {
            return uv;
        }
        for (auto& coord : uv)
        {
            coord[0] = 1.0f - coord[0];
        }
        return uv;
    };

    const auto eastUv = maybeFlipHorizontal(kUv, xAxisPlane && !handleOnPositiveSide);
    const auto westUv = maybeFlipHorizontal(kUv, xAxisPlane && handleOnPositiveSide);
    const auto southUv = maybeFlipHorizontal(kUv, !xAxisPlane && !handleOnPositiveSide);
    const auto northUv = maybeFlipHorizontal(kUv, !xAxisPlane && handleOnPositiveSide);

    appendCustomQuad(
        meshData,
        {{{wx + box.maxX, wy + box.minY, wz + box.minZ},
          {wx + box.maxX, wy + box.maxY, wz + box.minZ},
          {wx + box.maxX, wy + box.maxY, wz + box.maxZ},
          {wx + box.maxX, wy + box.minY, wz + box.maxZ}}},
        eastUv,
        {1.0f, 0.0f, 0.0f},
        tileIndex,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + box.minX, wy + box.minY, wz + box.maxZ},
          {wx + box.minX, wy + box.maxY, wz + box.maxZ},
          {wx + box.minX, wy + box.maxY, wz + box.minZ},
          {wx + box.minX, wy + box.minY, wz + box.minZ}}},
        westUv,
        {-1.0f, 0.0f, 0.0f},
        tileIndex,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + box.minX, wy + box.maxY, wz + box.maxZ},
          {wx + box.maxX, wy + box.maxY, wz + box.maxZ},
          {wx + box.maxX, wy + box.maxY, wz + box.minZ},
          {wx + box.minX, wy + box.maxY, wz + box.minZ}}},
        kUv,
        {0.0f, 1.0f, 0.0f},
        tileIndex,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + box.minX, wy + box.minY, wz + box.minZ},
          {wx + box.maxX, wy + box.minY, wz + box.minZ},
          {wx + box.maxX, wy + box.minY, wz + box.maxZ},
          {wx + box.minX, wy + box.minY, wz + box.maxZ}}},
        kUv,
        {0.0f, -1.0f, 0.0f},
        tileIndex,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + box.maxX, wy + box.minY, wz + box.maxZ},
          {wx + box.maxX, wy + box.maxY, wz + box.maxZ},
          {wx + box.minX, wy + box.maxY, wz + box.maxZ},
          {wx + box.minX, wy + box.minY, wz + box.maxZ}}},
        southUv,
        {0.0f, 0.0f, 1.0f},
        tileIndex,
        abgr);
    appendCustomQuad(
        meshData,
        {{{wx + box.minX, wy + box.minY, wz + box.minZ},
          {wx + box.minX, wy + box.maxY, wz + box.minZ},
          {wx + box.maxX, wy + box.maxY, wz + box.minZ},
          {wx + box.maxX, wy + box.minY, wz + box.minZ}}},
        northUv,
        {0.0f, 0.0f, -1.0f},
        tileIndex,
        abgr);
}

void appendBookshelfInsetFace(
    ChunkMeshData& meshData,
    const FaceDefinition& face,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint8_t centerTileIndex,
    const std::uint8_t frameTileIndex,
    const std::uint32_t abgr)
{
    constexpr std::array<std::array<float, 2>, 4> kQuadUv{{
        {0.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
    }};
    constexpr float kInsetDepth = 0.09f;
    constexpr float kFrameThickness = 0.12f;
    const float wx = static_cast<float>(worldX);
    const float wy = static_cast<float>(y);
    const float wz = static_cast<float>(worldZ);

    const auto emitEast = [&](const float xPlane, const float y0, const float y1, const float z0, const float z1, const std::uint8_t tile)
    {
        appendCustomQuad(
            meshData,
            {{{wx + xPlane, wy + y0, wz + z0}, {wx + xPlane, wy + y1, wz + z0}, {wx + xPlane, wy + y1, wz + z1}, {wx + xPlane, wy + y0, wz + z1}}},
            kQuadUv,
            {1.0f, 0.0f, 0.0f},
            tile,
            abgr);
    };
    const auto emitWest = [&](const float xPlane, const float y0, const float y1, const float z0, const float z1, const std::uint8_t tile)
    {
        appendCustomQuad(
            meshData,
            {{{wx + xPlane, wy + y0, wz + z1}, {wx + xPlane, wy + y1, wz + z1}, {wx + xPlane, wy + y1, wz + z0}, {wx + xPlane, wy + y0, wz + z0}}},
            kQuadUv,
            {-1.0f, 0.0f, 0.0f},
            tile,
            abgr);
    };
    const auto emitSouth = [&](const float zPlane, const float x0, const float x1, const float y0, const float y1, const std::uint8_t tile)
    {
        appendCustomQuad(
            meshData,
            {{{wx + x1, wy + y0, wz + zPlane}, {wx + x1, wy + y1, wz + zPlane}, {wx + x0, wy + y1, wz + zPlane}, {wx + x0, wy + y0, wz + zPlane}}},
            kQuadUv,
            {0.0f, 0.0f, 1.0f},
            tile,
            abgr);
    };
    const auto emitNorth = [&](const float zPlane, const float x0, const float x1, const float y0, const float y1, const std::uint8_t tile)
    {
        appendCustomQuad(
            meshData,
            {{{wx + x0, wy + y0, wz + zPlane}, {wx + x0, wy + y1, wz + zPlane}, {wx + x1, wy + y1, wz + zPlane}, {wx + x1, wy + y0, wz + zPlane}}},
            kQuadUv,
            {0.0f, 0.0f, -1.0f},
            tile,
            abgr);
    };

    if (face.offsetX > 0)
    {
        const float xPlane = 1.0f - kInsetDepth;
        emitEast(xPlane, kFrameThickness, 1.0f - kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, centerTileIndex);
        emitEast(xPlane, 1.0f - kFrameThickness, 1.0f, 0.0f, 1.0f, frameTileIndex);
        emitEast(xPlane, 0.0f, kFrameThickness, 0.0f, 1.0f, frameTileIndex);
        emitEast(xPlane, kFrameThickness, 1.0f - kFrameThickness, 0.0f, kFrameThickness, frameTileIndex);
        emitEast(xPlane, kFrameThickness, 1.0f - kFrameThickness, 1.0f - kFrameThickness, 1.0f, frameTileIndex);
        return;
    }
    if (face.offsetX < 0)
    {
        const float xPlane = kInsetDepth;
        emitWest(xPlane, kFrameThickness, 1.0f - kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, centerTileIndex);
        emitWest(xPlane, 1.0f - kFrameThickness, 1.0f, 0.0f, 1.0f, frameTileIndex);
        emitWest(xPlane, 0.0f, kFrameThickness, 0.0f, 1.0f, frameTileIndex);
        emitWest(xPlane, kFrameThickness, 1.0f - kFrameThickness, 0.0f, kFrameThickness, frameTileIndex);
        emitWest(xPlane, kFrameThickness, 1.0f - kFrameThickness, 1.0f - kFrameThickness, 1.0f, frameTileIndex);
        return;
    }
    if (face.offsetZ > 0)
    {
        const float zPlane = 1.0f - kInsetDepth;
        emitSouth(zPlane, kFrameThickness, 1.0f - kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, centerTileIndex);
        emitSouth(zPlane, 0.0f, 1.0f, 1.0f - kFrameThickness, 1.0f, frameTileIndex);
        emitSouth(zPlane, 0.0f, 1.0f, 0.0f, kFrameThickness, frameTileIndex);
        emitSouth(zPlane, 0.0f, kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, frameTileIndex);
        emitSouth(zPlane, 1.0f - kFrameThickness, 1.0f, kFrameThickness, 1.0f - kFrameThickness, frameTileIndex);
        return;
    }

    const float zPlane = kInsetDepth;
    emitNorth(zPlane, kFrameThickness, 1.0f - kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, centerTileIndex);
    emitNorth(zPlane, 0.0f, 1.0f, 1.0f - kFrameThickness, 1.0f, frameTileIndex);
    emitNorth(zPlane, 0.0f, 1.0f, 0.0f, kFrameThickness, frameTileIndex);
    emitNorth(zPlane, 0.0f, kFrameThickness, kFrameThickness, 1.0f - kFrameThickness, frameTileIndex);
    emitNorth(zPlane, 1.0f - kFrameThickness, 1.0f, kFrameThickness, 1.0f - kFrameThickness, frameTileIndex);
}
}  // namespace

ChunkMeshData ChunkMesher::buildMesh(
    const vibecraft::world::World& world,
    const vibecraft::world::ChunkCoord& coord) const
{
    ChunkMeshData meshData;
    const auto currentChunkIt = world.chunks().find(coord);
    if (currentChunkIt == world.chunks().end())
    {
        return meshData;
    }

    const auto chunkAt = [&world](const vibecraft::world::ChunkCoord& neighborCoord)
    {
        const auto chunkIt = world.chunks().find(neighborCoord);
        return chunkIt != world.chunks().end() ? &chunkIt->second : nullptr;
    };
    const vibecraft::world::Chunk* const westChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x - 1, coord.z});
    const vibecraft::world::Chunk* const eastChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x + 1, coord.z});
    const vibecraft::world::Chunk* const northChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x, coord.z - 1});
    const vibecraft::world::Chunk* const southChunk =
        chunkAt(vibecraft::world::ChunkCoord{coord.x, coord.z + 1});
    const std::vector<TorchEmitter> torchEmitters = collectNearbyTorchEmitters(world, coord);
    const BlockStorage& currentStorage = currentChunkIt->second.blockStorage();
    constexpr int kNoRenderableBlock = std::numeric_limits<int>::min();
    std::array<int, vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize> columnMinY;
    std::array<int, vibecraft::world::Chunk::kSize * vibecraft::world::Chunk::kSize> columnMaxY;
    columnMinY.fill(kNoRenderableBlock);
    columnMaxY.fill(kNoRenderableBlock);

    for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
        {
            const std::size_t columnIndex = static_cast<std::size_t>(localZ * vibecraft::world::Chunk::kSize + localX);
            for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
            {
                if (!vibecraft::world::isRenderable(blockFromStorage(currentStorage, localX, y, localZ)))
                {
                    continue;
                }
                if (columnMinY[columnIndex] == kNoRenderableBlock)
                {
                    columnMinY[columnIndex] = y;
                }
                columnMaxY[columnIndex] = y;
            }
        }
    }

    for (int localZ = 0; localZ < vibecraft::world::Chunk::kSize; ++localZ)
    {
        for (int localX = 0; localX < vibecraft::world::Chunk::kSize; ++localX)
        {
            const std::size_t columnIndex = static_cast<std::size_t>(localZ * vibecraft::world::Chunk::kSize + localX);
            if (columnMinY[columnIndex] == kNoRenderableBlock)
            {
                continue;
            }

            const int worldX = coord.x * vibecraft::world::Chunk::kSize + localX;
            const int worldZ = coord.z * vibecraft::world::Chunk::kSize + localZ;
            for (int y = columnMinY[columnIndex]; y <= columnMaxY[columnIndex]; ++y)
            {
                const BlockType blockType = blockFromStorage(currentStorage, localX, y, localZ);
                if (!vibecraft::world::isRenderable(blockType))
                {
                    continue;
                }

                if (vibecraft::world::isTorchBlock(blockType))
                {
                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const float torchLight = torchLightMultiplierAt(
                        static_cast<float>(worldX) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        static_cast<float>(worldZ) + 0.5f,
                        torchEmitters);
                    appendTorchGeometry(
                        meshData,
                        blockType,
                        worldX,
                        y,
                        worldZ,
                        modulateAbgrRgb(metadata.debugColor, torchLight));
                    continue;
                }

                if (vibecraft::world::isStairBlock(blockType))
                {
                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const float torchLight = torchLightMultiplierAt(
                        static_cast<float>(worldX) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        static_cast<float>(worldZ) + 0.5f,
                        torchEmitters);
                    appendStairsGeometry(
                        meshData,
                        blockType,
                        worldX,
                        y,
                        worldZ,
                        modulateAbgrRgb(metadata.debugColor, torchLight));
                    continue;
                }

                if (vibecraft::world::isDoorVariantBlock(blockType))
                {
                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const float torchLight = torchLightMultiplierAt(
                        static_cast<float>(worldX) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        static_cast<float>(worldZ) + 0.5f,
                        torchEmitters);
                    appendDoorGeometry(
                        meshData,
                        blockType,
                        worldX,
                        y,
                        worldZ,
                        modulateAbgrRgb(metadata.debugColor, torchLight));
                    continue;
                }

                if (usesCrossPlantMesh(blockType))
                {
                    // Flora meshes are crossed billboards instead of cubes. Dense flora uses tri-cross quads
                    // with per-cell jitter so fields read organic rather than a repeated X pattern.
                    constexpr std::array<std::array<float, 2>, 4> kPlantUv{{
                        {0.0f, 1.0f},
                        {0.0f, 0.0f},
                        {1.0f, 0.0f},
                        {1.0f, 1.0f},
                    }};

                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const std::uint8_t tileIndex =
                        vibecraft::world::textureTileIndex(blockType, vibecraft::world::BlockFace::Side);
                    const bool denseFlora = usesDenseFloraMesh(blockType);
                    const bool tuftTriCross = usesTuftTriCrossMesh(blockType);
                    const bool bamboo = blockType == BlockType::Bamboo;
                    const float randomA = hash01(worldX, y, worldZ, 0x31a67c59u);
                    const float randomB = hash01(worldX, y, worldZ, 0x7f4a7c15u);
                    const float baseInset = crossPlantInset(blockType);
                    float halfWidth = 0.5f - baseInset;
                    if (!bamboo && denseFlora)
                    {
                        halfWidth = tuftTriCross ? (0.18f + randomA * 0.05f) : (0.24f + randomA * 0.08f);
                    }
                    halfWidth = std::clamp(halfWidth, 0.10f, 0.49f);

                    float heightScale = 1.0f;
                    if (!bamboo)
                    {
                        if (blockType == BlockType::BrownMushroom || blockType == BlockType::RedMushroom)
                        {
                            heightScale = 0.56f + randomB * 0.16f;
                        }
                        else if (blockType == BlockType::DryTuft || blockType == BlockType::GrassTuft
                            || blockType == BlockType::FlowerTuft || blockType == BlockType::LushTuft
                            || blockType == BlockType::FrostTuft || blockType == BlockType::SparseTuft
                            || blockType == BlockType::CloverTuft || blockType == BlockType::SproutTuft)
                        {
                            heightScale = 0.68f + randomB * 0.10f;
                        }
                        else if (denseFlora)
                        {
                            heightScale = 0.82f + randomB * 0.14f;
                        }
                    }

                    const std::size_t quadCount = tuftTriCross ? 3u : 2u;
                    constexpr float kPi = 3.14159265358979323846f;
                    const float startAngle = bamboo ? (kPi * 0.25f) : (tuftTriCross ? (randomA * (kPi / 3.0f)) : (randomA * kPi));
                    const float angleStep = kPi / static_cast<float>(quadCount);
                    for (std::size_t quadIndex = 0; quadIndex < quadCount; ++quadIndex)
                    {
                        const float angle = startAngle + static_cast<float>(quadIndex) * angleStep;
                        const float dirX = std::cos(angle);
                        const float dirZ = std::sin(angle);
                        const std::array<std::array<float, 3>, 4> quad{{
                            {{0.5f - dirX * halfWidth, 0.0f, 0.5f - dirZ * halfWidth}},
                            {{0.5f - dirX * halfWidth, heightScale, 0.5f - dirZ * halfWidth}},
                            {{0.5f + dirX * halfWidth, heightScale, 0.5f + dirZ * halfWidth}},
                            {{0.5f + dirX * halfWidth, 0.0f, 0.5f + dirZ * halfWidth}},
                        }};

                        float nx = dirZ;
                        float ny = 0.0f;
                        float nz = -dirX;
                        if (!bamboo)
                        {
                            ny = tuftTriCross ? 0.12f : (denseFlora ? 0.24f : 0.15f);
                            const float invLength = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
                            nx *= invLength;
                            ny *= invLength;
                            nz *= invLength;
                        }

                        const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
                        for (std::size_t vertexIndex = 0; vertexIndex < quad.size(); ++vertexIndex)
                        {
                            const auto& corner = quad[vertexIndex];
                            const auto atlasUv = atlasUvForBlockType(tileIndex, kPlantUv[vertexIndex]);
                            const float tintNoise = denseFlora ? (0.98f + randomB * 0.04f) : 1.0f;
                            const float torchLight = torchLightMultiplierAt(
                                static_cast<float>(worldX) + corner[0],
                                static_cast<float>(y) + corner[1],
                                static_cast<float>(worldZ) + corner[2],
                                torchEmitters);
                            meshData.vertices.push_back(DebugVertex{
                                .x = static_cast<float>(worldX) + corner[0],
                                .y = static_cast<float>(y) + corner[1],
                                .z = static_cast<float>(worldZ) + corner[2],
                                .nx = nx,
                                .ny = ny,
                                .nz = nz,
                                .u = atlasUv[0],
                                .v = atlasUv[1],
                                .abgr = modulateAbgrRgb(metadata.debugColor, tintNoise * torchLight),
                            });
                        }

                        meshData.indices.push_back(baseIndex);
                        meshData.indices.push_back(baseIndex + 1);
                        meshData.indices.push_back(baseIndex + 2);
                        meshData.indices.push_back(baseIndex);
                        meshData.indices.push_back(baseIndex + 2);
                        meshData.indices.push_back(baseIndex + 3);
                        ++meshData.faceCount;
                    }
                    continue;
                }

                for (const FaceDefinition& face : kFaces)
                {
                    const BlockType neighborBlock = sampledNeighborBlock(
                        currentStorage,
                        westChunk,
                        eastChunk,
                        northChunk,
                        southChunk,
                        localX + face.offsetX,
                        y + face.offsetY,
                        localZ + face.offsetZ);
                    if (neighborBlock == blockType
                        || vibecraft::world::occludesFaces(neighborBlock))
                    {
                        continue;
                    }

                    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
                    const auto metadata = vibecraft::world::blockMetadata(blockType);
                    const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(blockType, face.blockFace);
                    if (blockType == BlockType::Bookshelf
                        && (face.blockFace == vibecraft::world::BlockFace::East
                            || face.blockFace == vibecraft::world::BlockFace::West
                            || face.blockFace == vibecraft::world::BlockFace::South
                            || face.blockFace == vibecraft::world::BlockFace::North))
                    {
                        appendBookshelfInsetFace(
                            meshData,
                            face,
                            worldX,
                            y,
                            worldZ,
                            tileIndex,
                            metadata.textureTiles.top,
                            modulateAbgrRgb(
                                metadata.debugColor,
                                torchLightMultiplierAt(
                                    static_cast<float>(worldX) + 0.5f,
                                    static_cast<float>(y) + 0.5f,
                                    static_cast<float>(worldZ) + 0.5f,
                                    torchEmitters)));
                        continue;
                    }
                    for (std::size_t vertexIndex = 0; vertexIndex < face.corners.size(); ++vertexIndex)
                    {
                        const auto& corner = face.corners[vertexIndex];
                        const auto atlasUv = atlasUvForBlockType(tileIndex, face.uvs[vertexIndex]);
                        const float ao = ambientOcclusionForCorner(
                            currentStorage,
                            westChunk,
                            eastChunk,
                            northChunk,
                            southChunk,
                            localX,
                            y,
                            localZ,
                            face,
                            corner);
                        const float torchLight = torchLightMultiplierAt(
                            static_cast<float>(worldX) + corner[0],
                            static_cast<float>(y) + corner[1],
                            static_cast<float>(worldZ) + corner[2],
                            torchEmitters);
                        meshData.vertices.push_back(DebugVertex{
                            .x = static_cast<float>(worldX) + corner[0],
                            .y = static_cast<float>(y) + corner[1],
                            .z = static_cast<float>(worldZ) + corner[2],
                            .nx = static_cast<float>(face.offsetX),
                            .ny = static_cast<float>(face.offsetY),
                            .nz = static_cast<float>(face.offsetZ),
                            .u = atlasUv[0],
                            .v = atlasUv[1],
                            .abgr = modulateAbgrRgb(metadata.debugColor, ao * torchLight),
                        });
                    }

                    meshData.indices.push_back(baseIndex);
                    meshData.indices.push_back(baseIndex + 1);
                    meshData.indices.push_back(baseIndex + 2);
                    meshData.indices.push_back(baseIndex);
                    meshData.indices.push_back(baseIndex + 2);
                    meshData.indices.push_back(baseIndex + 3);
                    ++meshData.faceCount;
                }
            }
        }
    }

    return meshData;
}
}  // namespace vibecraft::meshing
