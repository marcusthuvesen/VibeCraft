#include "ChunkMesherTorchGeometry.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::meshing
{
namespace
{
using vibecraft::world::BlockType;

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

constexpr float kTileInsetU = 0.5f / static_cast<float>(vibecraft::kChunkAtlasWidthPx);
constexpr float kTileInsetV = 0.5f / static_cast<float>(vibecraft::kChunkAtlasHeightPx);

[[nodiscard]] std::array<float, 2> atlasUvForTile(
    const std::uint8_t tileIndex,
    const float localU,
    const float localV)
{
    const float tileWidth = 1.0f / static_cast<float>(vibecraft::kChunkAtlasTileColumns);
    const float tileHeight = 1.0f / static_cast<float>(vibecraft::kChunkAtlasTileRows);
    const float tileX = static_cast<float>(tileIndex % vibecraft::kChunkAtlasTileColumns);
    const float tileY = static_cast<float>(tileIndex / vibecraft::kChunkAtlasTileColumns);
    const float minU = tileX * tileWidth + kTileInsetU;
    const float maxU = (tileX + 1.0f) * tileWidth - kTileInsetU;
    const float minV = tileY * tileHeight + kTileInsetV;
    const float maxV = (tileY + 1.0f) * tileHeight - kTileInsetV;
    return {
        minU + (maxU - minU) * std::clamp(localU, 0.0f, 1.0f),
        minV + (maxV - minV) * std::clamp(localV, 0.0f, 1.0f),
    };
}

void appendTorchQuad(
    ChunkMeshData& meshData,
    const std::array<std::array<float, 3>, 4>& corners,
    const std::array<std::array<float, 2>, 4>& localUvs,
    const std::array<float, 3>& normal,
    const std::uint8_t tileIndex,
    const std::uint32_t abgr)
{
    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
    for (std::size_t vertexIndex = 0; vertexIndex < corners.size(); ++vertexIndex)
    {
        const auto atlasUv = atlasUvForTile(
            tileIndex,
            localUvs[vertexIndex][0],
            localUvs[vertexIndex][1]);
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

[[nodiscard]] std::array<float, 2> centeredPixelRange(const float normalizedSpan)
{
    const float spanPx = std::clamp(normalizedSpan * 16.0f, 1.0f, 16.0f);
    const float halfSpanPx = spanPx * 0.5f;
    return {
        (8.0f - halfSpanPx) / 16.0f,
        (8.0f + halfSpanPx) / 16.0f,
    };
}

void appendTorchBox(
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
    const std::uint8_t tileIndex,
    const std::uint32_t abgr)
{
    const float wx = static_cast<float>(worldX);
    const float wy = static_cast<float>(y);
    const float wz = static_cast<float>(worldZ);
    const auto xRange = centeredPixelRange(x1 - x0);
    const auto zRange = centeredPixelRange(z1 - z0);
    const float vTop = std::clamp(1.0f - y1, 0.0f, 1.0f);
    const float vBottom = std::clamp(1.0f - y0, 0.0f, 1.0f);

    appendTorchQuad(
        meshData,
        {{{wx + x1, wy + y0, wz + z0}, {wx + x1, wy + y1, wz + z0}, {wx + x1, wy + y1, wz + z1}, {wx + x1, wy + y0, wz + z1}}},
        {{{zRange[0], vBottom}, {zRange[0], vTop}, {zRange[1], vTop}, {zRange[1], vBottom}}},
        {1.0f, 0.0f, 0.0f},
        tileIndex,
        abgr);
    appendTorchQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z1}, {wx + x0, wy + y1, wz + z1}, {wx + x0, wy + y1, wz + z0}, {wx + x0, wy + y0, wz + z0}}},
        {{{zRange[0], vBottom}, {zRange[0], vTop}, {zRange[1], vTop}, {zRange[1], vBottom}}},
        {-1.0f, 0.0f, 0.0f},
        tileIndex,
        abgr);
    appendTorchQuad(
        meshData,
        {{{wx + x0, wy + y1, wz + z1}, {wx + x1, wy + y1, wz + z1}, {wx + x1, wy + y1, wz + z0}, {wx + x0, wy + y1, wz + z0}}},
        {{{xRange[0], zRange[1]}, {xRange[1], zRange[1]}, {xRange[1], zRange[0]}, {xRange[0], zRange[0]}}},
        {0.0f, 1.0f, 0.0f},
        tileIndex,
        abgr);
    appendTorchQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z0}, {wx + x1, wy + y0, wz + z0}, {wx + x1, wy + y0, wz + z1}, {wx + x0, wy + y0, wz + z1}}},
        {{{xRange[0], zRange[1]}, {xRange[1], zRange[1]}, {xRange[1], zRange[0]}, {xRange[0], zRange[0]}}},
        {0.0f, -1.0f, 0.0f},
        tileIndex,
        abgr);
    appendTorchQuad(
        meshData,
        {{{wx + x1, wy + y0, wz + z1}, {wx + x1, wy + y1, wz + z1}, {wx + x0, wy + y1, wz + z1}, {wx + x0, wy + y0, wz + z1}}},
        {{{xRange[0], vBottom}, {xRange[0], vTop}, {xRange[1], vTop}, {xRange[1], vBottom}}},
        {0.0f, 0.0f, 1.0f},
        tileIndex,
        abgr);
    appendTorchQuad(
        meshData,
        {{{wx + x0, wy + y0, wz + z0}, {wx + x0, wy + y1, wz + z0}, {wx + x1, wy + y1, wz + z0}, {wx + x1, wy + y0, wz + z0}}},
        {{{xRange[0], vBottom}, {xRange[0], vTop}, {xRange[1], vTop}, {xRange[1], vBottom}}},
        {0.0f, 0.0f, -1.0f},
        tileIndex,
        abgr);
}
}  // namespace

void appendTorchGeometry(
    ChunkMeshData& meshData,
    const BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint8_t tileIndex,
    const std::uint32_t baseAbgr,
    const std::uint32_t flameAbgr)
{
    const TorchFacing facing = torchFacingForBlockType(blockType);
    constexpr float kStemHalfWidth = 1.0f / 16.0f;
    constexpr float kFlameHalfWidth = 2.0f / 16.0f;

    if (facing == TorchFacing::Standing)
    {
        // Standing torch: small upright stem with a compact flame cap.
        appendTorchBox(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            0.0f,
            0.62f,
            0.5f - kStemHalfWidth,
            0.5f + kStemHalfWidth,
            tileIndex,
            baseAbgr);
        appendTorchBox(
            meshData,
            worldX,
            y,
            worldZ,
            0.5f - kFlameHalfWidth,
            0.5f + kFlameHalfWidth,
            0.62f,
            0.80f,
            0.5f - kFlameHalfWidth,
            0.5f + kFlameHalfWidth,
            tileIndex,
            flameAbgr);
        return;
    }

    // Wall torch: one angled stick box + one small flame head.
    // Using two boxes (instead of the old 3–4 segment chain) keeps the shape clean.
    auto appendWallTorch = [&](const bool negativeDirection, const bool alongZAxis)
    {
        if (alongZAxis)
        {
            const float baseZ = negativeDirection ? 0.82f : 0.18f;
            const float tipZ  = negativeDirection ? 0.44f : 0.56f;
            appendTorchBox(
                meshData,
                worldX, y, worldZ,
                0.5f - kStemHalfWidth, 0.5f + kStemHalfWidth,
                0.14f, 0.62f,
                std::min(baseZ, tipZ) - kStemHalfWidth,
                std::max(baseZ, tipZ) + kStemHalfWidth,
                tileIndex, baseAbgr);
            appendTorchBox(
                meshData,
                worldX, y, worldZ,
                0.5f - kFlameHalfWidth, 0.5f + kFlameHalfWidth,
                0.62f, 0.80f,
                tipZ - kFlameHalfWidth, tipZ + kFlameHalfWidth,
                tileIndex, flameAbgr);
            return;
        }

        const float baseX = negativeDirection ? 0.82f : 0.18f;
        const float tipX  = negativeDirection ? 0.44f : 0.56f;
        appendTorchBox(
            meshData,
            worldX, y, worldZ,
            std::min(baseX, tipX) - kStemHalfWidth,
            std::max(baseX, tipX) + kStemHalfWidth,
            0.14f, 0.62f,
            0.5f - kStemHalfWidth, 0.5f + kStemHalfWidth,
            tileIndex, baseAbgr);
        appendTorchBox(
            meshData,
            worldX, y, worldZ,
            tipX - kFlameHalfWidth, tipX + kFlameHalfWidth,
            0.62f, 0.80f,
            0.5f - kFlameHalfWidth, 0.5f + kFlameHalfWidth,
            tileIndex, flameAbgr);
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
}  // namespace vibecraft::meshing
