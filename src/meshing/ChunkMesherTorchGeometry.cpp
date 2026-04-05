#include "ChunkMesherTorchGeometry.hpp"

#include <algorithm>
#include <cstdint>

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
}  // namespace

void appendTorchGeometry(
    ChunkMeshData& meshData,
    const BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::uint8_t tileIndex,
    const std::uint32_t baseAbgr,
    const std::uint32_t flameAbgr,
    const AppendBoxQuadsFn appendBoxQuads)
{
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
            const float tipZ = 0.50f;
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
}  // namespace vibecraft::meshing
