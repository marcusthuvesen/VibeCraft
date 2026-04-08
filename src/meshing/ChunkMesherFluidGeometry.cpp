#include "ChunkMesherFluidGeometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::meshing
{
namespace
{
enum class FluidFaceKind
{
    Top,
    SideTopEdge,
    SideBottomEdge,
    Bottom,
};

[[nodiscard]] std::uint32_t scaleAbgrChannels(
    const std::uint32_t abgr,
    const float redFactor,
    const float greenFactor,
    const float blueFactor,
    const float alphaFactor = 1.0f)
{
    const auto scale = [](const std::uint8_t channel, const float factor)
    {
        const float clampedFactor = std::clamp(factor, 0.0f, 2.0f);
        const int scaled = static_cast<int>(std::lround(static_cast<float>(channel) * clampedFactor));
        return static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
    };

    const std::uint8_t r = static_cast<std::uint8_t>(abgr & 0xffu);
    const std::uint8_t g = static_cast<std::uint8_t>((abgr >> 8u) & 0xffu);
    const std::uint8_t b = static_cast<std::uint8_t>((abgr >> 16u) & 0xffu);
    const std::uint8_t a = static_cast<std::uint8_t>((abgr >> 24u) & 0xffu);
    return (static_cast<std::uint32_t>(scale(a, alphaFactor)) << 24u)
        | (static_cast<std::uint32_t>(scale(b, blueFactor)) << 16u)
        | (static_cast<std::uint32_t>(scale(g, greenFactor)) << 8u)
        | static_cast<std::uint32_t>(scale(r, redFactor));
}

[[nodiscard]] std::uint32_t applyLightToAbgr(const std::uint32_t abgr, const float lightFactor)
{
    return scaleAbgrChannels(abgr, lightFactor, lightFactor, lightFactor);
}

[[nodiscard]] std::uint32_t fluidFaceBaseColor(
    const vibecraft::world::BlockType fluidType,
    const std::uint32_t baseAbgr,
    const FluidFaceKind faceKind)
{
    if (fluidType == vibecraft::world::BlockType::Water)
    {
        switch (faceKind)
        {
        case FluidFaceKind::Top:
            return scaleAbgrChannels(baseAbgr, 1.04f, 1.10f, 1.22f, 1.02f);
        case FluidFaceKind::SideTopEdge:
            return scaleAbgrChannels(baseAbgr, 0.96f, 1.02f, 1.14f, 0.96f);
        case FluidFaceKind::SideBottomEdge:
            return scaleAbgrChannels(baseAbgr, 0.72f, 0.80f, 0.94f, 0.88f);
        case FluidFaceKind::Bottom:
            return scaleAbgrChannels(baseAbgr, 0.62f, 0.70f, 0.82f, 0.80f);
        }
    }

    switch (faceKind)
    {
    case FluidFaceKind::Top:
        return scaleAbgrChannels(baseAbgr, 1.08f, 1.02f, 0.92f, 1.0f);
    case FluidFaceKind::SideTopEdge:
        return scaleAbgrChannels(baseAbgr, 0.94f, 0.90f, 0.84f, 0.96f);
    case FluidFaceKind::SideBottomEdge:
        return scaleAbgrChannels(baseAbgr, 0.78f, 0.72f, 0.66f, 0.90f);
    case FluidFaceKind::Bottom:
        return scaleAbgrChannels(baseAbgr, 0.70f, 0.64f, 0.58f, 0.84f);
    }

    return baseAbgr;
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

void appendQuad(
    ChunkMeshData& meshData,
    const std::array<std::array<float, 3>, 4>& corners,
    const std::array<std::array<float, 2>, 4>& localUvs,
    const std::array<float, 3>& normal,
    const std::uint8_t tileIndex,
    const std::array<std::uint32_t, 4>& vertexColors)
{
    const std::uint32_t baseIndex = static_cast<std::uint32_t>(meshData.vertices.size());
    for (std::size_t vertexIndex = 0; vertexIndex < corners.size(); ++vertexIndex)
    {
        const auto atlasUv = atlasUvForTile(tileIndex, localUvs[vertexIndex][0], localUvs[vertexIndex][1]);
        meshData.vertices.push_back(DebugVertex{
            .x = corners[vertexIndex][0],
            .y = corners[vertexIndex][1],
            .z = corners[vertexIndex][2],
            .nx = normal[0],
            .ny = normal[1],
            .nz = normal[2],
            .u = atlasUv[0],
            .v = atlasUv[1],
            .abgr = vertexColors[vertexIndex],
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

[[nodiscard]] std::array<float, 3> quadNormal(
    const std::array<float, 3>& a,
    const std::array<float, 3>& b,
    const std::array<float, 3>& d)
{
    const std::array<float, 3> ab{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const std::array<float, 3> ad{d[0] - a[0], d[1] - a[1], d[2] - a[2]};
    std::array<float, 3> normal{
        ab[1] * ad[2] - ab[2] * ad[1],
        ab[2] * ad[0] - ab[0] * ad[2],
        ab[0] * ad[1] - ab[1] * ad[0],
    };
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    if (length <= 1e-6f)
    {
        return {0.0f, 1.0f, 0.0f};
    }
    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;
    return normal;
}

[[nodiscard]] float fluidFillHeightAt(
    const vibecraft::world::World& world,
    const vibecraft::world::BlockType fluidType,
    const int worldX,
    const int y,
    const int worldZ)
{
    if (world.blockAt(worldX, y, worldZ) != fluidType)
    {
        return 0.0f;
    }
    if (world.blockAt(worldX, y + 1, worldZ) == fluidType)
    {
        return 1.0f;
    }

    const vibecraft::world::FluidRenderState state = world.fluidRenderStateAt(worldX, y, worldZ);
    if (state.type != fluidType)
    {
        return 0.0f;
    }
    if (state.isSource || state.horizontalDistance == 0)
    {
        return 1.0f;
    }

    const float stepDrop = fluidType == vibecraft::world::BlockType::Water ? 0.12f : 0.16f;
    const float minHeight = fluidType == vibecraft::world::BlockType::Water ? 0.28f : 0.34f;
    return std::clamp(1.0f - static_cast<float>(state.horizontalDistance) * stepDrop, minHeight, 0.92f);
}

[[nodiscard]] float fluidCornerHeight(
    const vibecraft::world::World& world,
    const vibecraft::world::BlockType fluidType,
    const int worldX,
    const int y,
    const int worldZ,
    const std::array<std::array<int, 2>, 4>& sampleOffsets)
{
    float weightedHeight = 0.0f;
    float weightSum = 0.0f;
    for (const auto& offset : sampleOffsets)
    {
        const int sampleX = worldX + offset[0];
        const int sampleZ = worldZ + offset[1];
        if (world.blockAt(sampleX, y + 1, sampleZ) == fluidType)
        {
            return 1.0f;
        }

        const vibecraft::world::FluidRenderState state = world.fluidRenderStateAt(sampleX, y, sampleZ);
        if (state.type != fluidType)
        {
            continue;
        }

        const float sampleHeight = fluidFillHeightAt(world, fluidType, sampleX, y, sampleZ);
        const float weight = state.isSource ? 1.35f : 1.0f;
        weightedHeight += sampleHeight * weight;
        weightSum += weight;
    }

    return weightSum > 0.0f ? (weightedHeight / weightSum) : 0.0f;
}

[[nodiscard]] bool shouldRenderFluidFace(
    const vibecraft::world::BlockType fluidType,
    const vibecraft::world::BlockType neighborBlock,
    const bool renderAgainstOccluders = false)
{
    if (neighborBlock == fluidType)
    {
        return false;
    }
    if (!renderAgainstOccluders && vibecraft::world::occludesFaces(neighborBlock))
    {
        return false;
    }
    return true;
}
}  // namespace

void appendFluidGeometry(
    ChunkMeshData& meshData,
    const vibecraft::world::World& world,
    const vibecraft::world::BlockType blockType,
    const int worldX,
    const int y,
    const int worldZ,
    const vibecraft::world::BlockMetadata& metadata,
    const std::vector<TorchEmitter>& torchEmitters)
{
    constexpr std::array<std::array<float, 2>, 4> kUv{{
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f},
    }};

    const float wx = static_cast<float>(worldX);
    const float wy = static_cast<float>(y);
    const float wz = static_cast<float>(worldZ);
    const std::uint8_t topTile = metadata.textureTiles.top;
    const std::uint8_t bottomTile = metadata.textureTiles.bottom;
    const std::uint8_t sideTile = metadata.textureTiles.side;
    const std::uint32_t topFaceBaseColor = fluidFaceBaseColor(blockType, metadata.debugColor, FluidFaceKind::Top);
    const std::uint32_t sideTopBaseColor =
        fluidFaceBaseColor(blockType, metadata.debugColor, FluidFaceKind::SideTopEdge);
    const std::uint32_t sideBottomBaseColor =
        fluidFaceBaseColor(blockType, metadata.debugColor, FluidFaceKind::SideBottomEdge);
    const std::uint32_t bottomFaceBaseColor =
        fluidFaceBaseColor(blockType, metadata.debugColor, FluidFaceKind::Bottom);
    const auto vertexColorAt = [&](const float vx, const float vy, const float vz, const std::uint32_t baseColor)
    {
        const float torchLight = torchLightMultiplierAt(vx, vy, vz, torchEmitters);
        return applyLightToAbgr(baseColor, torchLight);
    };

    const float southWestHeight = fluidCornerHeight(
        world,
        blockType,
        worldX,
        y,
        worldZ,
        {{{0, 0}, {-1, 0}, {0, 1}, {-1, 1}}});
    const float southEastHeight = fluidCornerHeight(
        world,
        blockType,
        worldX,
        y,
        worldZ,
        {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}});
    const float northEastHeight = fluidCornerHeight(
        world,
        blockType,
        worldX,
        y,
        worldZ,
        {{{0, 0}, {1, 0}, {0, -1}, {1, -1}}});
    const float northWestHeight = fluidCornerHeight(
        world,
        blockType,
        worldX,
        y,
        worldZ,
        {{{0, 0}, {-1, 0}, {0, -1}, {-1, -1}}});

    const vibecraft::world::BlockType above = world.blockAt(worldX, y + 1, worldZ);
    if (shouldRenderFluidFace(blockType, above))
    {
        const std::array<std::array<float, 3>, 4> topCorners{{
            {{wx + 0.0f, wy + southWestHeight, wz + 1.0f}},
            {{wx + 1.0f, wy + southEastHeight, wz + 1.0f}},
            {{wx + 1.0f, wy + northEastHeight, wz + 0.0f}},
            {{wx + 0.0f, wy + northWestHeight, wz + 0.0f}},
        }};
        appendQuad(
            meshData,
            topCorners,
            kUv,
            quadNormal(topCorners[0], topCorners[1], topCorners[3]),
            topTile,
            {{
                vertexColorAt(topCorners[0][0], topCorners[0][1], topCorners[0][2], topFaceBaseColor),
                vertexColorAt(topCorners[1][0], topCorners[1][1], topCorners[1][2], topFaceBaseColor),
                vertexColorAt(topCorners[2][0], topCorners[2][1], topCorners[2][2], topFaceBaseColor),
                vertexColorAt(topCorners[3][0], topCorners[3][1], topCorners[3][2], topFaceBaseColor),
            }});
    }

    const vibecraft::world::BlockType below = world.blockAt(worldX, y - 1, worldZ);
    if (shouldRenderFluidFace(blockType, below))
    {
        const std::array<std::array<float, 3>, 4> bottomCorners{{
            {{wx + 0.0f, wy + 0.0f, wz + 0.0f}},
            {{wx + 1.0f, wy + 0.0f, wz + 0.0f}},
            {{wx + 1.0f, wy + 0.0f, wz + 1.0f}},
            {{wx + 0.0f, wy + 0.0f, wz + 1.0f}},
        }};
        const std::uint32_t bottomColor = vertexColorAt(wx + 0.5f, wy, wz + 0.5f, bottomFaceBaseColor);
        appendQuad(
            meshData,
            bottomCorners,
            kUv,
            {0.0f, -1.0f, 0.0f},
            bottomTile,
            {{bottomColor, bottomColor, bottomColor, bottomColor}});
    }

    const auto appendSideFace = [&](const vibecraft::world::BlockType neighborBlock,
                                    const std::array<std::array<float, 3>, 4>& corners,
                                    const std::array<std::array<float, 2>, 4>& sideUvs,
                                    const std::array<float, 3>& normal)
    {
        if (!shouldRenderFluidFace(blockType, neighborBlock))
        {
            return;
        }

        appendQuad(
            meshData,
            corners,
            sideUvs,
            normal,
            sideTile,
            {{
                vertexColorAt(corners[0][0], corners[0][1], corners[0][2], sideBottomBaseColor),
                vertexColorAt(corners[1][0], corners[1][1], corners[1][2], sideTopBaseColor),
                vertexColorAt(corners[2][0], corners[2][1], corners[2][2], sideTopBaseColor),
                vertexColorAt(corners[3][0], corners[3][1], corners[3][2], sideBottomBaseColor),
            }});
    };

    appendSideFace(
        world.blockAt(worldX + 1, y, worldZ),
        {{{wx + 1.0f, wy + 0.0f, wz + 0.0f},
          {wx + 1.0f, wy + northEastHeight, wz + 0.0f},
          {wx + 1.0f, wy + southEastHeight, wz + 1.0f},
          {wx + 1.0f, wy + 0.0f, wz + 1.0f}}},
        {{{0.0f, 1.0f},
          {0.0f, 1.0f - northEastHeight},
          {1.0f, 1.0f - southEastHeight},
          {1.0f, 1.0f}}},
        {1.0f, 0.0f, 0.0f});
    appendSideFace(
        world.blockAt(worldX - 1, y, worldZ),
        {{{wx + 0.0f, wy + 0.0f, wz + 1.0f},
          {wx + 0.0f, wy + southWestHeight, wz + 1.0f},
          {wx + 0.0f, wy + northWestHeight, wz + 0.0f},
          {wx + 0.0f, wy + 0.0f, wz + 0.0f}}},
        {{{0.0f, 1.0f},
          {0.0f, 1.0f - southWestHeight},
          {1.0f, 1.0f - northWestHeight},
          {1.0f, 1.0f}}},
        {-1.0f, 0.0f, 0.0f});
    appendSideFace(
        world.blockAt(worldX, y, worldZ + 1),
        {{{wx + 1.0f, wy + 0.0f, wz + 1.0f},
          {wx + 1.0f, wy + southEastHeight, wz + 1.0f},
          {wx + 0.0f, wy + southWestHeight, wz + 1.0f},
          {wx + 0.0f, wy + 0.0f, wz + 1.0f}}},
        {{{0.0f, 1.0f},
          {0.0f, 1.0f - southEastHeight},
          {1.0f, 1.0f - southWestHeight},
          {1.0f, 1.0f}}},
        {0.0f, 0.0f, 1.0f});
    appendSideFace(
        world.blockAt(worldX, y, worldZ - 1),
        {{{wx + 0.0f, wy + 0.0f, wz + 0.0f},
          {wx + 0.0f, wy + northWestHeight, wz + 0.0f},
          {wx + 1.0f, wy + northEastHeight, wz + 0.0f},
          {wx + 1.0f, wy + 0.0f, wz + 0.0f}}},
        {{{0.0f, 1.0f},
          {0.0f, 1.0f - northWestHeight},
          {1.0f, 1.0f - northEastHeight},
          {1.0f, 1.0f}}},
        {0.0f, 0.0f, -1.0f});
}
}  // namespace vibecraft::meshing
