#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/core/Logger.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
namespace
{
constexpr float kTileInsetU = 0.5f / static_cast<float>(kChunkAtlasWidthPx);
constexpr float kTileInsetV = 0.5f / static_cast<float>(kChunkAtlasHeightPx);

struct CuboidUvSet
{
    TextureUvRect front{};
    TextureUvRect back{};
    TextureUvRect left{};
    TextureUvRect right{};
    TextureUvRect top{};
    TextureUvRect bottom{};
};

[[nodiscard]] TextureUvRect uvRectFromPixels(
    const float textureWidth,
    const float textureHeight,
    const float x,
    const float y,
    const float width,
    const float height)
{
    const float insetU = 0.5f / textureWidth;
    const float insetV = 0.5f / textureHeight;
    return TextureUvRect{
        .minU = std::clamp(x / textureWidth + insetU, 0.0f, 1.0f),
        .maxU = std::clamp((x + width) / textureWidth - insetU, 0.0f, 1.0f),
        .minV = std::clamp(y / textureHeight + insetV, 0.0f, 1.0f),
        .maxV = std::clamp((y + height) / textureHeight - insetV, 0.0f, 1.0f),
    };
}

[[nodiscard]] CuboidUvSet makeCuboidUvSet(
    const float textureWidth,
    const float textureHeight,
    const float u,
    const float v,
    const float width,
    const float height,
    const float depth)
{
    return CuboidUvSet{
        .front = uvRectFromPixels(textureWidth, textureHeight, u + depth, v + depth, width, height),
        .back = uvRectFromPixels(textureWidth, textureHeight, u + depth + width + depth, v + depth, width, height),
        .left = uvRectFromPixels(textureWidth, textureHeight, u, v + depth, depth, height),
        .right = uvRectFromPixels(textureWidth, textureHeight, u + depth + width, v + depth, depth, height),
        .top = uvRectFromPixels(textureWidth, textureHeight, u + depth, v, width, depth),
        .bottom = uvRectFromPixels(textureWidth, textureHeight, u + depth + width, v, width, depth),
    };
}

struct MobUvLayout
{
    CuboidUvSet body{};
    CuboidUvSet head{};
    CuboidUvSet leg{};
    CuboidUvSet arm{};
    CuboidUvSet wing{};
    CuboidUvSet beak{};
    CuboidUvSet snout{};
    CuboidUvSet horn{};
    CuboidUvSet wattle{};
};

struct ModelPartSpec
{
    glm::vec3 centerOffsetPx{0.0f};
    glm::vec3 halfExtentsPx{0.0f};
    const CuboidUvSet* uv = nullptr;
};

[[nodiscard]] MobUvLayout uvLayoutForMobKind(const vibecraft::game::MobKind mobKind)
{
    constexpr float kTexW = 64.0f;
    constexpr float kTexH = 32.0f;
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 8.0f, 12.0f, 4.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .arm = makeCuboidUvSet(kTexW, kTexH, 40.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .horn = makeCuboidUvSet(kTexW, kTexH, 24.0f, 0.0f, 2.0f, 8.0f, 2.0f),
        };
    case MK::Cow:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 18.0f, 4.0f, 12.0f, 18.0f, 10.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 6.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 0.0f, 22.0f, 4.0f, 3.0f, 2.0f),
            .horn = makeCuboidUvSet(kTexW, kTexH, 22.0f, 0.0f, 1.0f, 3.0f, 1.0f),
        };
    case MK::Pig:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 28.0f, 8.0f, 10.0f, 16.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 6.0f, 4.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 4.0f, 3.0f, 2.0f),
        };
    case MK::Sheep:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 28.0f, 8.0f, 12.0f, 16.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 6.0f, 6.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 6.0f, 4.0f),
        };
    case MK::Chicken:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 0.0f, 9.0f, 6.0f, 8.0f, 6.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 4.0f, 6.0f, 3.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 26.0f, 0.0f, 3.0f, 5.0f, 3.0f),
            .wing = makeCuboidUvSet(kTexW, kTexH, 24.0f, 13.0f, 1.0f, 4.0f, 6.0f),
            .beak = makeCuboidUvSet(kTexW, kTexH, 14.0f, 0.0f, 4.0f, 2.0f, 2.0f),
            .wattle = makeCuboidUvSet(kTexW, kTexH, 14.0f, 4.0f, 2.0f, 2.0f, 2.0f),
        };
    }
    return {};
}

[[nodiscard]] float referenceHeightPxForMobKind(const vibecraft::game::MobKind mobKind)
{
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:
        return 32.0f;
    case MK::Cow:
        return 20.0f;
    case MK::Pig:
        return 16.0f;
    case MK::Sheep:
        return 18.0f;
    case MK::Chicken:
        return 12.0f;
    }
    return 16.0f;
}

[[nodiscard]] float referenceHalfWidthPxForMobKind(const vibecraft::game::MobKind mobKind)
{
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:
        return 6.0f;
    case MK::Cow:
        return 6.0f;
    case MK::Pig:
        return 5.0f;
    case MK::Sheep:
        return 5.0f;
    case MK::Chicken:
        return 4.0f;
    }
    return 4.0f;
}

struct TextGridMetrics
{
    float charWidthPx = 1.0f;
    float charHeightPx = 1.0f;
};

struct PixelRect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

[[nodiscard]] TextGridMetrics makeTextGridMetrics(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    return TextGridMetrics{
        .charWidthPx = static_cast<float>(windowWidth) / static_cast<float>(std::max<std::uint16_t>(1, textWidth)),
        .charHeightPx = static_cast<float>(windowHeight) / static_cast<float>(std::max<std::uint16_t>(1, textHeight)),
    };
}

[[nodiscard]] PixelRect gridRectFromChars(
    const TextGridMetrics& metrics,
    const int col,
    const int row,
    const int widthChars,
    const int heightRows)
{
    const float x0 = std::floor(static_cast<float>(std::max(0, col)) * metrics.charWidthPx);
    const float y0 = std::floor(static_cast<float>(std::max(0, row)) * metrics.charHeightPx);
    const float x1 = std::ceil(static_cast<float>(std::max(0, col + std::max(0, widthChars))) * metrics.charWidthPx);
    const float y1 = std::ceil(static_cast<float>(std::max(0, row + std::max(0, heightRows))) * metrics.charHeightPx);
    return PixelRect{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
}

[[nodiscard]] PixelRect expandedRect(const PixelRect& rect, const float insetPx)
{
    return PixelRect{
        .x0 = rect.x0 - insetPx,
        .y0 = rect.y0 - insetPx,
        .x1 = rect.x1 + insetPx,
        .y1 = rect.y1 + insetPx,
    };
}
} // namespace

void Renderer::drawUiSolidRect(
    const float x0,
    const float y0,
    const float x1,
    const float y1,
    const std::uint32_t abgr)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX)
    {
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    const float ax0 = std::floor(std::min(x0, x1));
    const float ay0 = std::floor(std::min(y0, y1));
    const float ax1 = std::ceil(std::max(x0, x1));
    const float ay1 = std::ceil(std::max(y0, y1));
    if (ax1 - ax0 < 0.5f || ay1 - ay0 < 0.5f)
    {
        return;
    }

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = ax0, .y = ay0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 0.0f, .abgr = abgr},
        detail::ChunkVertex{.x = ax1, .y = ay0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 0.0f, .abgr = abgr},
        detail::ChunkVertex{.x = ax1, .y = ay1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 1.0f, .abgr = abgr},
        detail::ChunkVertex{.x = ax0, .y = ay1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 1.0f, .abgr = abgr},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
    std::memcpy(tvb.data, vertices, sizeof(vertices));

    bgfx::TransientIndexBuffer tib{};
    bgfx::allocTransientIndexBuffer(&tib, 6);
    auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(
        0,
        detail::toUniformHandle(inventoryUiSamplerHandle_),
        detail::toTextureHandle(chunkAtlasTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiSolidProgramHandle_));
}

void Renderer::drawInventoryItemIcons(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const std::uint16_t hotbarRow,
    const std::uint16_t bagRow0,
    const std::uint16_t bagRow1,
    const std::uint16_t bagRow2)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX || textWidth == 0 || textHeight == 0)
    {
        return;
    }

    const float charHeightPx = static_cast<float>(height_) / static_cast<float>(textHeight);
    const float charWidthPx = static_cast<float>(width_) / static_cast<float>(textWidth);

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const detail::HotbarLayoutPx hotbarLayout =
        detail::computeHotbarLayoutPx(width_, height_, textHeight, hotbarRow);
    const bool canDrawSolid = inventoryUiSolidProgramHandle_ != UINT16_MAX;
    const auto drawUiTextureRectUv = [&](
                                         const float x0,
                                         const float y0,
                                         const float x1,
                                         const float y1,
                                         const std::uint16_t textureHandle,
                                         const TextureUvRect& uvRect)
    {
        if (textureHandle == UINT16_MAX)
        {
            return;
        }
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = x0,
                .y = y0,
                .z = 0.0f,
                .nx = 0.0f,
                .ny = 0.0f,
                .nz = 1.0f,
                .u = uvRect.minU,
                .v = uvRect.minV,
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = x1,
                .y = y0,
                .z = 0.0f,
                .nx = 0.0f,
                .ny = 0.0f,
                .nz = 1.0f,
                .u = uvRect.maxU,
                .v = uvRect.minV,
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = x1,
                .y = y1,
                .z = 0.0f,
                .nx = 0.0f,
                .ny = 0.0f,
                .nz = 1.0f,
                .u = uvRect.maxU,
                .v = uvRect.maxV,
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = x0,
                .y = y1,
                .z = 0.0f,
                .nx = 0.0f,
                .ny = 0.0f,
                .nz = 1.0f,
                .u = uvRect.minU,
                .v = uvRect.maxV,
                .abgr = 0xffffffff},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
    };
    const auto drawUiTextureRect = [&](const float x0,
                                       const float y0,
                                       const float x1,
                                       const float y1,
                                       const std::uint16_t textureHandle)
    {
        const TextureUvRect fullUv{.minU = 0.0f, .maxU = 1.0f, .minV = 0.0f, .maxV = 1.0f};
        drawUiTextureRectUv(x0, y0, x1, y1, textureHandle, fullUv);
    };
    const auto itemTextureHandle = [&](const HudItemKind itemKind)
    {
        return hudItemKindTextureHandle(itemKind);
    };
    const auto drawStairAtlasIconInRect = [&](const float x0,
                                              const float y0,
                                              const float x1,
                                              const float y1,
                                              const std::uint8_t tileIndex)
    {
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        const TextureUvRect uvRect{
            .minU = tileX * tileWidth + kTileInsetU,
            .maxU = (tileX + 1.0f) * tileWidth - kTileInsetU,
            .minV = tileY * tileHeight + kTileInsetV,
            .maxV = (tileY + 1.0f) * tileHeight - kTileInsetV,
        };
        const float w = std::max(1.0f, x1 - x0);
        const float h = std::max(1.0f, y1 - y0);
        const float pad = std::max(1.0f, std::round(std::min(w, h) * 0.06f));
        const float iconX0 = x0 + pad;
        const float iconY0 = y0 + pad;
        const float iconX1 = x1 - pad;
        const float iconY1 = y1 - pad;
        const float iconW = iconX1 - iconX0;
        const float iconH = iconY1 - iconY0;
        drawUiTextureRectUv(
            iconX0,
            iconY0 + iconH * 0.50f,
            iconX1,
            iconY1,
            chunkAtlasTextureHandle_,
            uvRect);
        drawUiTextureRectUv(
            iconX0 + iconW * 0.44f,
            iconY0 + iconH * 0.16f,
            iconX1,
            iconY0 + iconH * 0.50f,
            chunkAtlasTextureHandle_,
            uvRect);
    };
    const auto drawHudSlotIconInRect = [&](const FrameDebugData::HotbarSlotHud& slotHud,
                                           const float x0,
                                           const float y0,
                                           const float x1,
                                           const float y1)
    {
        if (slotHud.count == 0)
        {
            return;
        }

        const float slotW = std::max(0.0f, x1 - x0);
        const float slotH = std::max(0.0f, y1 - y0);
        const float iconInset = std::max(2.0f, std::round(std::min(slotW, slotH) * 0.12f));
        const float ix0 = x0 + iconInset;
        const float iy0 = y0 + iconInset;
        const float ix1 = x1 - iconInset;
        const float iy1 = y1 - iconInset;
        const std::uint16_t textureHandle = itemTextureHandle(slotHud.itemKind);
        if (textureHandle != UINT16_MAX)
        {
            drawUiTextureRectUv(ix0, iy0, ix1, iy1, textureHandle, hudItemKindTextureUv(slotHud.itemKind));
            return;
        }
        if (slotHud.blockType == vibecraft::world::BlockType::Air)
        {
            return;
        }

        const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
            slotHud.blockType,
            vibecraft::world::BlockFace::Side);
        if (vibecraft::world::isStairBlock(slotHud.blockType))
        {
            drawStairAtlasIconInRect(ix0, iy0, ix1, iy1, tileIndex);
            return;
        }
        drawAtlasIcon(
            std::floor((ix0 + ix1) * 0.5f),
            std::floor((iy0 + iy1) * 0.5f),
            std::max(1.0f, std::min(ix1 - ix0, iy1 - iy0)),
            tileIndex);
    };
    const auto drawDurabilityBar = [&](const FrameDebugData::HotbarSlotHud& slotHud,
                                       const float x0,
                                       const float y0,
                                       const float x1,
                                       const float y1)
    {
        if (slotHud.durabilityMax == 0 || slotHud.count == 0)
        {
            return;
        }

        const float fraction = std::clamp(
            static_cast<float>(slotHud.durabilityRemaining) / static_cast<float>(slotHud.durabilityMax),
            0.0f,
            1.0f);
        const float trackHeight = std::max(3.0f, std::round((y1 - y0) * 0.08f));
        const float insetX = std::max(2.0f, std::round((x1 - x0) * 0.12f));
        const float barX0 = x0 + insetX;
        const float barX1 = x1 - insetX;
        const float barY1 = y1 - std::max(2.0f, std::round((y1 - y0) * 0.06f));
        const float barY0 = barY1 - trackHeight;
        const std::uint32_t trackAbgr = detail::packAbgr8(glm::vec3(0.05f, 0.05f, 0.06f), 0.92f);
        const glm::vec3 durabilityColor = glm::mix(
            glm::vec3(0.85f, 0.20f, 0.14f),
            glm::vec3(0.32f, 0.90f, 0.22f),
            fraction);
        const std::uint32_t fillAbgr = detail::packAbgr8(durabilityColor, 0.96f);
        drawUiSolidRect(barX0, barY0, barX1, barY1, trackAbgr);
        if (fraction > 0.001f)
        {
            drawUiSolidRect(barX0, barY0, barX0 + (barX1 - barX0) * fraction, barY1, fillAbgr);
        }
    };

    if (canDrawSolid && hotbarLayout.slotSize > 0.0f)
    {
        // Hytale-inspired: cool navy strip, steel-blue slots, bright gold selection ring.
        const std::uint32_t hotbarFrameOuterAbgr = detail::packAbgr8(glm::vec3(0.03f, 0.06f, 0.10f), 0.95f);
        const std::uint32_t hotbarFrameInnerAbgr = detail::packAbgr8(glm::vec3(0.10f, 0.17f, 0.27f), 0.94f);
        const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.03f, 0.06f, 0.10f), 0.98f);
        const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.14f, 0.24f, 0.38f), 0.96f);
        const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.25f, 0.38f, 0.56f), 0.96f);
        const std::uint32_t selectedOuterAbgr = detail::packAbgr8(glm::vec3(0.98f, 0.81f, 0.38f), 0.99f);
        const std::uint32_t selectedInnerAbgr = detail::packAbgr8(glm::vec3(0.84f, 0.63f, 0.18f), 0.99f);
        const std::uint32_t xpTrackAbgr = detail::packAbgr8(glm::vec3(0.03f, 0.07f, 0.11f), 0.94f);
        const std::uint32_t xpFillAbgr = detail::packAbgr8(glm::vec3(0.54f, 0.88f, 0.25f), 0.98f);

        const float slot = hotbarLayout.slotSize;
        const float gap = hotbarLayout.gap;
        const float ox = hotbarLayout.originX;
        const float sy0 = hotbarLayout.slotTopY;

        const float stripPadX = std::max(3.0f, std::round(slot * 0.12f));
        const float stripPadY = std::max(2.0f, std::round(slot * 0.1f));
        drawUiSolidRect(
            ox - stripPadX - 1.0f,
            sy0 - stripPadY - 1.0f,
            ox + hotbarLayout.totalWidth + stripPadX + 1.0f,
            sy0 + slot + stripPadY + 1.0f,
            hotbarFrameOuterAbgr);
        drawUiSolidRect(
            ox - stripPadX,
            sy0 - stripPadY,
            ox + hotbarLayout.totalWidth + stripPadX,
            sy0 + slot + stripPadY,
            hotbarFrameInnerAbgr);

        const float xpBarH = std::max(2.0f, std::round(hotbarLayout.slotSize * 0.09f));
        const float xpGap = std::max(2.0f, std::round(hotbarLayout.slotSize * 0.11f));
        const float xpY1 = hotbarLayout.slotTopY - xpGap;
        const float xpY0 = xpY1 - xpBarH;
        const float xpInset = std::min(2.0f, std::max(0.0f, hotbarLayout.gap * 0.35f));
        drawUiSolidRect(
            hotbarLayout.originX - xpInset,
            xpY0,
            hotbarLayout.originX + hotbarLayout.totalWidth + xpInset,
            xpY1,
            xpTrackAbgr);

        const float xpFill = std::clamp(frameDebugData.experienceFill, 0.0f, 1.0f);
        if (xpFill > 0.001f)
        {
            const float trackW = hotbarLayout.totalWidth + 2.0f * xpInset;
            drawUiSolidRect(
                hotbarLayout.originX - xpInset,
                xpY0,
                hotbarLayout.originX - xpInset + trackW * xpFill,
                xpY1,
                xpFillAbgr);
        }

        if (fullHeartTextureHandle_ != UINT16_MAX
            && halfHeartTextureHandle_ != UINT16_MAX
            && emptyHeartTextureHandle_ != UINT16_MAX)
        {
            const float clampedMaxHealth = std::max(0.0f, frameDebugData.maxHealth);
            const float clampedHealth = std::clamp(frameDebugData.health, 0.0f, clampedMaxHealth);
            const int heartCount = std::max(1, static_cast<int>(std::ceil(clampedMaxHealth * 0.5f)));
            const float heartSize = std::clamp(std::round(slot * 0.72f), 28.0f, 92.0f);
            const float heartGap = std::max(2.0f, std::round(heartSize * 0.08f));
            const float totalHeartsWidth =
                static_cast<float>(heartCount) * heartSize + static_cast<float>(std::max(0, heartCount - 1)) * heartGap;
            const float heartsX = std::floor((static_cast<float>(width_) - totalHeartsWidth) * 0.5f);
            const float heartsY = std::floor(xpY0 - std::max(8.0f, heartSize * 0.56f) - heartSize);
            for (int heartIndex = 0; heartIndex < heartCount; ++heartIndex)
            {
                const float heartHealth = clampedHealth - static_cast<float>(heartIndex * 2);
                std::uint16_t textureHandle = emptyHeartTextureHandle_;
                if (heartHealth >= 2.0f)
                {
                    textureHandle = fullHeartTextureHandle_;
                }
                else if (heartHealth >= 1.0f)
                {
                    textureHandle = halfHeartTextureHandle_;
                }
                const float hx0 = heartsX + static_cast<float>(heartIndex) * (heartSize + heartGap);
                drawUiTextureRect(hx0, heartsY, hx0 + heartSize, heartsY + heartSize, textureHandle);
            }
        }

        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
        {
            const float sx = ox + static_cast<float>(slotIndex) * (slot + gap);
            drawUiSolidRect(
                sx,
                sy0,
                sx + slot,
                sy0 + slot,
                slotBorderAbgr);
            drawUiSolidRect(
                sx + 1.0f,
                sy0 + 1.0f,
                sx + slot - 1.0f,
                sy0 + slot - 1.0f,
                slotFillAbgr);
            drawUiSolidRect(
                sx + 1.0f,
                sy0 + 1.0f,
                sx + slot - 1.0f,
                sy0 + std::max(2.0f, slot * 0.2f),
                slotTopHighlightAbgr);

            if (slotIndex == frameDebugData.hotbarSelectedIndex)
            {
                drawUiSolidRect(sx - 2.0f, sy0 - 2.0f, sx + slot + 2.0f, sy0 + slot + 2.0f, selectedOuterAbgr);
                drawUiSolidRect(sx - 1.0f, sy0 - 1.0f, sx + slot + 1.0f, sy0 + slot + 1.0f, selectedInnerAbgr);
            }
        }
    }

    const float iconBase = hotbarLayout.slotSize > 0.0f
        ? std::clamp(std::floor(hotbarLayout.slotSize * 0.78f), 36.0f, 112.0f)
        : 13.0f;

    if (hotbarLayout.slotSize > 0.0f)
    {
        const float slot = hotbarLayout.slotSize;
        const float gap = hotbarLayout.gap;
        const float ox = hotbarLayout.originX;
        const float sy0 = hotbarLayout.slotTopY;
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
        {
            const FrameDebugData::HotbarSlotHud& slotHud = frameDebugData.hotbarSlots[slotIndex];
            if (slotHud.count == 0)
            {
                continue;
            }
            const float centerX = ox + static_cast<float>(slotIndex) * (slot + gap) + slot * 0.5f;
            const float centerY = sy0 + slot * 0.5f;
            const float iconScale = slotIndex == frameDebugData.hotbarSelectedIndex ? 1.06f : 1.0f;
            const float iconSize = iconBase * iconScale;
            const float iconHalf = iconSize * 0.5f;
            drawHudSlotIconInRect(
                slotHud,
                centerX - iconHalf,
                centerY - iconHalf,
                centerX + iconHalf,
                centerY + iconHalf);
            drawDurabilityBar(slotHud, ox + static_cast<float>(slotIndex) * (slot + gap), sy0, ox + static_cast<float>(slotIndex) * (slot + gap) + slot, sy0 + slot);
        }
    }
    else
    {
        const int hotbarStartCol = detail::computeCenteredHotbarStartColumn();
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
        {
            const FrameDebugData::HotbarSlotHud& slotHud = frameDebugData.hotbarSlots[slotIndex];
            if (slotHud.count == 0)
            {
                continue;
            }
            const float cellCol = static_cast<float>(hotbarStartCol + static_cast<int>(slotIndex) * 6);
            const float centerX = (cellCol + 2.5f) * charWidthPx;
            const float centerY = static_cast<float>(hotbarRow) * charHeightPx + charHeightPx * 0.42f;
            const float iconSize =
                slotIndex == frameDebugData.hotbarSelectedIndex ? 14.0f : 13.0f;
            const float iconHalf = iconSize * 0.5f;
            drawHudSlotIconInRect(
                slotHud,
                centerX - iconHalf,
                centerY - iconHalf,
                centerX + iconHalf,
                centerY + iconHalf);
        }
    }

    constexpr int kBagCellChars = 9;
    constexpr int kBagGapChars = 1;
    constexpr int kBagColumns = 9;
    const int bagStartCol = detail::computeCenteredColumnStart(detail::computeBagGridWidthChars());
    const std::array<std::uint16_t, 3> bagRows{bagRow0, bagRow1, bagRow2};
    for (std::size_t row = 0; row < bagRows.size(); ++row)
    {
        for (int col = 0; col < kBagColumns; ++col)
        {
            const std::size_t slotIndex = row * static_cast<std::size_t>(kBagColumns) + static_cast<std::size_t>(col);
            const FrameDebugData::HotbarSlotHud& slotHud = frameDebugData.bagSlots[slotIndex];
            if (slotHud.count == 0)
            {
                continue;
            }
            const float cellCol = static_cast<float>(bagStartCol + col * (kBagCellChars + kBagGapChars));
            const float centerX = (cellCol + static_cast<float>(kBagCellChars) * 0.5f) * charWidthPx;
            const float centerY = static_cast<float>(bagRows[row]) * charHeightPx + charHeightPx * 0.42f;
            const float iconSize = std::clamp(charHeightPx * 0.95f, 13.0f, 24.0f);
            const float iconHalf = iconSize * 0.5f;
            drawHudSlotIconInRect(
                slotHud,
                centerX - iconHalf,
                centerY - iconHalf,
                centerX + iconHalf,
                centerY + iconHalf);
        }
    }
}

void Renderer::drawCraftingOverlay(const FrameDebugData& frameDebugData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX)
    {
        return;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const detail::CraftingOverlayLayoutPx layout =
        detail::computeCraftingOverlayLayoutPx(
            width_,
            height_,
            frameDebugData.craftingUiMode,
            frameDebugData.craftingUsesWorkbench);
    if (layout.slotSize <= 0.0f)
    {
        return;
    }

    const bgfx::Stats* const stats = bgfx::getStats();
    const std::uint16_t textWidth =
        stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
    const std::uint16_t textHeight = stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;
    const float charWidthPx = static_cast<float>(width_) / static_cast<float>(std::max<std::uint16_t>(1, textWidth));
    const float charHeightPx = static_cast<float>(height_) / static_cast<float>(std::max<std::uint16_t>(1, textHeight));
    const bool inventoryMode = frameDebugData.craftingUiMode == CraftingUiMode::Inventory;
    const bool workbenchMode = frameDebugData.craftingUiMode == CraftingUiMode::Workbench;
    const bool chestMode = frameDebugData.craftingUiMode == CraftingUiMode::Chest;
    const bool furnaceMode = frameDebugData.craftingUiMode == CraftingUiMode::Furnace;
    const auto drawUiTextureRectUv = [&](const float x0,
                                         const float y0,
                                         const float x1,
                                         const float y1,
                                         const std::uint16_t textureHandle,
                                         const TextureUvRect& uvRect)
    {
        if (textureHandle == UINT16_MAX)
        {
            return;
        }
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.minU, .v = uvRect.minV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.maxU, .v = uvRect.minV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.maxU, .v = uvRect.maxV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.minU, .v = uvRect.maxV, .abgr = 0xffffffff},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
    };
    const bool canDrawSolid = inventoryUiSolidProgramHandle_ != UINT16_MAX;
    if (!canDrawSolid)
    {
        return;
    }

    const std::uint32_t dimAbgr = detail::packAbgr8(glm::vec3(0.02f, 0.03f, 0.05f), 0.52f);
    const std::uint32_t panelOuterAbgr = detail::packAbgr8(glm::vec3(0.05f, 0.08f, 0.14f), 0.97f);
    const std::uint32_t panelInnerAbgr = detail::packAbgr8(glm::vec3(0.11f, 0.17f, 0.27f), 0.96f);
    const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.04f, 0.08f, 0.12f), 0.98f);
    const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.16f, 0.25f, 0.38f), 0.97f);
    const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.26f, 0.39f, 0.58f), 0.97f);
    const std::uint32_t resultGlowAbgr = detail::packAbgr8(glm::vec3(0.93f, 0.79f, 0.32f), 0.55f);
    const std::uint32_t cursorOutlineAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.95f);
    const auto drawStairAtlasIconInRect = [&](const float x0,
                                              const float y0,
                                              const float x1,
                                              const float y1,
                                              const std::uint8_t tileIndex)
    {
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        const TextureUvRect uvRect{
            .minU = tileX * tileWidth + kTileInsetU,
            .maxU = (tileX + 1.0f) * tileWidth - kTileInsetU,
            .minV = tileY * tileHeight + kTileInsetV,
            .maxV = (tileY + 1.0f) * tileHeight - kTileInsetV,
        };
        const float w = std::max(1.0f, x1 - x0);
        const float h = std::max(1.0f, y1 - y0);
        const float pad = std::max(1.0f, std::round(std::min(w, h) * 0.06f));
        const float iconX0 = x0 + pad;
        const float iconY0 = y0 + pad;
        const float iconX1 = x1 - pad;
        const float iconY1 = y1 - pad;
        const float iconW = iconX1 - iconX0;
        const float iconH = iconY1 - iconY0;
        drawUiTextureRectUv(
            iconX0,
            iconY0 + iconH * 0.50f,
            iconX1,
            iconY1,
            chunkAtlasTextureHandle_,
            uvRect);
        drawUiTextureRectUv(
            iconX0 + iconW * 0.44f,
            iconY0 + iconH * 0.16f,
            iconX1,
            iconY0 + iconH * 0.50f,
            chunkAtlasTextureHandle_,
            uvRect);
    };

    drawUiSolidRect(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), dimAbgr);
    drawUiSolidRect(layout.panelLeft - 2.0f, layout.panelTop - 2.0f, layout.panelRight + 2.0f, layout.panelBottom + 2.0f, panelOuterAbgr);
    drawUiSolidRect(layout.panelLeft, layout.panelTop, layout.panelRight, layout.panelBottom, panelInnerAbgr);

    const auto drawSlotFrame = [&](const float x, const float y, const bool highlight)
    {
        if (highlight)
        {
            drawUiSolidRect(x - 2.0f, y - 2.0f, x + layout.slotSize + 2.0f, y + layout.slotSize + 2.0f, resultGlowAbgr);
        }
        drawUiSolidRect(x, y, x + layout.slotSize, y + layout.slotSize, slotBorderAbgr);
        drawUiSolidRect(x + 1.0f, y + 1.0f, x + layout.slotSize - 1.0f, y + layout.slotSize - 1.0f, slotFillAbgr);
        drawUiSolidRect(
            x + 1.0f,
            y + 1.0f,
            x + layout.slotSize - 1.0f,
            y + std::max(2.0f, layout.slotSize * 0.22f),
            slotTopHighlightAbgr);
    };

    const auto drawSlotContents = [&](const FrameDebugData::HotbarSlotHud& slotHud, const float x, const float y)
    {
        if (slotHud.count == 0)
        {
            return;
        }

        const float centerX = x + layout.slotSize * 0.5f;
        const float centerY = y + layout.slotSize * 0.5f;
        const float iconInset = std::max(2.0f, std::round(layout.slotSize * 0.09f));
        const float iconMinX = x + iconInset;
        const float iconMinY = y + iconInset;
        const float iconMaxX = x + layout.slotSize - iconInset;
        const float iconMaxY = y + layout.slotSize - iconInset;
        const float iconSize = std::max(1.0f, std::min(iconMaxX - iconMinX, iconMaxY - iconMinY));
        const std::uint16_t textureHandle = hudItemKindTextureHandle(slotHud.itemKind);

        if (textureHandle != UINT16_MAX)
        {
            drawUiTextureRectUv(
                iconMinX,
                iconMinY,
                iconMaxX,
                iconMaxY,
                textureHandle,
                hudItemKindTextureUv(slotHud.itemKind));
        }
        else if (slotHud.blockType != vibecraft::world::BlockType::Air)
        {
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            if (vibecraft::world::isStairBlock(slotHud.blockType))
            {
                drawStairAtlasIconInRect(iconMinX, iconMinY, iconMaxX, iconMaxY, tileIndex);
            }
            else
            {
                drawAtlasIcon(centerX, centerY, iconSize, tileIndex);
            }
        }

        if (slotHud.count > 1)
        {
            const std::string digits = fmt::format("{}", std::min(slotHud.count, 99u));
            const float textX1 = x + layout.slotSize - std::max(2.0f, layout.slotSize * 0.05f);
            const float textY1 = y + layout.slotSize - std::max(2.0f, layout.slotSize * 0.05f);
            int col = static_cast<int>(std::floor((textX1 - charWidthPx * 0.55f) / charWidthPx))
                - static_cast<int>(digits.size()) + 1;
            int row = static_cast<int>(std::floor((textY1 - charHeightPx * 0.95f) / charHeightPx));
            col = std::clamp(col, 0, static_cast<int>(textWidth) - static_cast<int>(digits.size()));
            row = std::clamp(row, 0, static_cast<int>(textHeight) - 1);
            bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), 0x0f, "%s", digits.c_str());
        }

        if (slotHud.durabilityMax > 0)
        {
            const float fraction = std::clamp(
                static_cast<float>(slotHud.durabilityRemaining) / static_cast<float>(slotHud.durabilityMax),
                0.0f,
                1.0f);
            const float trackH = std::max(3.0f, std::round(layout.slotSize * 0.08f));
            const float trackX0 = x + std::max(2.0f, std::round(layout.slotSize * 0.10f));
            const float trackX1 = x + layout.slotSize - std::max(2.0f, std::round(layout.slotSize * 0.10f));
            const float trackY1 = y + layout.slotSize - std::max(2.0f, std::round(layout.slotSize * 0.08f));
            const float trackY0 = trackY1 - trackH;
            const std::uint32_t trackAbgr = detail::packAbgr8(glm::vec3(0.04f, 0.05f, 0.06f), 0.95f);
            const glm::vec3 fillColor = glm::mix(
                glm::vec3(0.86f, 0.19f, 0.14f),
                glm::vec3(0.33f, 0.89f, 0.22f),
                fraction);
            const std::uint32_t fillAbgr = detail::packAbgr8(fillColor, 0.98f);
            drawUiSolidRect(trackX0, trackY0, trackX1, trackY1, trackAbgr);
            if (fraction > 0.001f)
            {
                drawUiSolidRect(trackX0, trackY0, trackX0 + (trackX1 - trackX0) * fraction, trackY1, fillAbgr);
            }
        }
    };

    constexpr std::array<const char*, 4> kEquipmentLabels{"HELM", "CHEST", "LEGS", "BOOTS"};
    if (inventoryMode)
    {
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.equipmentSlots.size(); ++slotIndex)
        {
            const float slotX = layout.equipmentOriginX;
            const float slotY = layout.equipmentOriginY + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
            drawSlotFrame(slotX, slotY, false);
            drawSlotContents(frameDebugData.equipmentSlots[slotIndex], slotX, slotY);

            const int labelCol = std::clamp(
                static_cast<int>(std::floor((slotX + layout.slotSize + layout.slotGap * 0.7f) / charWidthPx)),
                0,
                static_cast<int>(textWidth) - 1);
            const int labelRow = std::clamp(
                static_cast<int>(std::floor((slotY + layout.slotSize * 0.38f) / charHeightPx)),
                0,
                static_cast<int>(textHeight) - 1);
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(labelCol),
                static_cast<std::uint16_t>(labelRow),
                0x0e,
                "%s",
                kEquipmentLabels[slotIndex]);
        }
    }

    const auto drawInventoryPlayerPreview = [&](const float previewCenterX, const float previewTop) -> void
    {
        if (playerMobTextureHandle_ == UINT16_MAX)
        {
            return;
        }
        const float figurePixelHeight = 32.0f;
        const float desiredHeight = layout.slotSize * 4.3f;
        const float scale = desiredHeight / figurePixelHeight;
        const float figurePixelWidth = 16.0f;
        const float previewWidth = figurePixelWidth * scale;
        const float previewLeft = previewCenterX - previewWidth * 0.5f;
        const float previewBottom = previewTop + desiredHeight;
        const float padX = layout.slotSize * 0.35f;
        const float padY = layout.slotSize * 0.25f;
        const std::uint32_t bgAbgr = detail::packAbgr8(glm::vec3(0.05f, 0.06f, 0.11f), 0.88f);
        const std::uint32_t borderAbgr = detail::packAbgr8(glm::vec3(0.29f, 0.46f, 0.78f), 0.94f);
        drawUiSolidRect(
            previewLeft - padX,
            previewTop - padY,
            previewLeft + previewWidth + padX,
            previewBottom + padY * 0.7f,
            bgAbgr);
        drawUiSolidRect(
            previewLeft - padX - 3.0f,
            previewTop - padY - 3.0f,
            previewLeft + previewWidth + padX + 3.0f,
            previewBottom + padY * 0.7f + 3.0f,
            borderAbgr);

        const MobUvLayout playerUv = uvLayoutForMobKind(game::MobKind::Player);
        const auto drawPart = [&](const TextureUvRect& uv, const float px, const float py, const float pw, const float ph)
        {
            drawUiTextureRectUv(
                previewLeft + px * scale,
                previewTop + py * scale,
                previewLeft + (px + pw) * scale,
                previewTop + (py + ph) * scale,
                playerMobTextureHandle_,
                uv);
        };

        drawPart(playerUv.head.front, 4.0f, 0.0f, 8.0f, 8.0f);
        drawPart(playerUv.body.front, 4.0f, 8.0f, 8.0f, 12.0f);
        drawPart(playerUv.arm.front, 0.0f, 8.0f, 4.0f, 12.0f);
        TextureUvRect armRight = playerUv.arm.front;
        std::swap(armRight.minU, armRight.maxU);
        drawPart(armRight, 12.0f, 8.0f, 4.0f, 12.0f);
        drawPart(playerUv.leg.front, 4.0f, 20.0f, 4.0f, 12.0f);
        TextureUvRect legRight = playerUv.leg.front;
        std::swap(legRight.minU, legRight.maxU);
        drawPart(legRight, 8.0f, 20.0f, 4.0f, 12.0f);
    };

    const int craftingColumns = workbenchMode || chestMode ? 3 : 2;
    const int craftingRows = workbenchMode || chestMode ? 3 : 2;
    if (furnaceMode)
    {
        constexpr std::size_t kFurnaceInputGridIndex = 1;
        constexpr std::size_t kFurnaceFuelGridIndex = 7;
        drawSlotFrame(layout.craftingOriginX, layout.craftingOriginY, false);
        drawSlotContents(frameDebugData.craftingGridSlots[kFurnaceInputGridIndex], layout.craftingOriginX, layout.craftingOriginY);
        drawSlotFrame(layout.furnaceFuelSlotX, layout.furnaceFuelSlotY, false);
        drawSlotContents(frameDebugData.craftingGridSlots[kFurnaceFuelGridIndex], layout.furnaceFuelSlotX, layout.furnaceFuelSlotY);
    }
    else
    {
        for (int row = 0; row < craftingRows; ++row)
        {
            for (int col = 0; col < craftingColumns; ++col)
            {
                const std::size_t slotIndex = static_cast<std::size_t>(row * 3 + col);
                const float slotX = layout.craftingOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
                const float slotY = layout.craftingOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
                drawSlotFrame(slotX, slotY, false);
                drawSlotContents(frameDebugData.craftingGridSlots[slotIndex], slotX, slotY);
            }
        }
    }

    if (frameDebugData.craftingMenuActive && inventoryMode && playerMobTextureHandle_ != UINT16_MAX)
    {
        float previewTop = layout.equipmentOriginY - layout.slotSize * 1.25f;
        const float minPreviewTop = layout.panelTop + layout.slotSize * 0.35f;
        if (previewTop < minPreviewTop)
        {
            previewTop = minPreviewTop;
        }
        const float previewCenterX = layout.equipmentOriginX + layout.slotSize * 0.85f;
        drawInventoryPlayerPreview(previewCenterX, previewTop);
    }

    if (furnaceMode)
    {
        const float progressBarX = layout.craftingOriginX + layout.slotSize + layout.slotGap * 0.9f;
        const float progressBarY = layout.resultSlotY + layout.slotSize * 0.42f;
        const float progressBarWidth = std::max(10.0f, layout.resultSlotX - progressBarX - layout.slotGap * 0.9f);
        const float progressBarHeight = std::max(6.0f, std::round(layout.slotSize * 0.16f));
        const float fuelBarX = layout.craftingOriginX + layout.slotSize * 0.38f;
        const float fuelBarY = layout.craftingOriginY + layout.slotSize + layout.slotGap * 0.18f;
        const float fuelBarWidth = std::max(6.0f, std::round(layout.slotSize * 0.24f));
        const float fuelBarHeight = std::max(10.0f, layout.furnaceFuelSlotY - fuelBarY - layout.slotGap * 0.18f);
        const std::uint32_t progressBackAbgr = detail::packAbgr8(glm::vec3(0.14f, 0.16f, 0.20f), 0.98f);
        const std::uint32_t progressFillAbgr = detail::packAbgr8(glm::vec3(0.86f, 0.86f, 0.84f), 0.98f);
        const std::uint32_t fuelFillAbgr = detail::packAbgr8(glm::vec3(0.95f, 0.58f, 0.17f), 0.98f);
        drawUiSolidRect(progressBarX, progressBarY, progressBarX + progressBarWidth, progressBarY + progressBarHeight, progressBackAbgr);
        drawUiSolidRect(
            progressBarX + 1.0f,
            progressBarY + 1.0f,
            progressBarX + 1.0f + std::max(0.0f, (progressBarWidth - 2.0f) * std::clamp(frameDebugData.craftingProgressFraction, 0.0f, 1.0f)),
            progressBarY + progressBarHeight - 1.0f,
            progressFillAbgr);
        drawUiSolidRect(fuelBarX, fuelBarY, fuelBarX + fuelBarWidth, fuelBarY + fuelBarHeight, progressBackAbgr);
        const float clampedFuel = std::clamp(frameDebugData.craftingFuelFraction, 0.0f, 1.0f);
        const float fuelFillTop = fuelBarY + 1.0f + (fuelBarHeight - 2.0f) * (1.0f - clampedFuel);
        drawUiSolidRect(
            fuelBarX + 1.0f,
            fuelFillTop,
            fuelBarX + fuelBarWidth - 1.0f,
            fuelBarY + fuelBarHeight - 1.0f,
            fuelFillAbgr);
        drawSlotFrame(layout.resultSlotX, layout.resultSlotY, frameDebugData.craftingResultSlot.count > 0);
        drawSlotContents(frameDebugData.craftingResultSlot, layout.resultSlotX, layout.resultSlotY);
    }
    else if (!chestMode)
    {
        const float arrowY = layout.resultSlotY + layout.slotSize * 0.5f;
        const float arrowX0 = layout.craftingOriginX
            + static_cast<float>(craftingColumns) * layout.slotSize
            + static_cast<float>(craftingColumns - 1) * layout.slotGap
            + layout.slotGap * 0.9f;
        const float arrowX1 = layout.resultSlotX - layout.slotGap * 0.9f;
        const float arrowMidX = arrowX1 - layout.slotSize * 0.18f;
        drawUiSolidRect(arrowX0, arrowY - 2.0f, arrowMidX, arrowY + 2.0f, cursorOutlineAbgr);
        drawUiSolidRect(arrowMidX, arrowY - layout.slotSize * 0.16f, arrowX1, arrowY, cursorOutlineAbgr);
        drawUiSolidRect(arrowMidX, arrowY, arrowX1, arrowY + layout.slotSize * 0.16f, cursorOutlineAbgr);

        drawSlotFrame(layout.resultSlotX, layout.resultSlotY, frameDebugData.craftingResultSlot.count > 0);
        drawSlotContents(frameDebugData.craftingResultSlot, layout.resultSlotX, layout.resultSlotY);
    }

    constexpr int kVisibleBagRows = 3;
    constexpr std::size_t kBagColumns = 9;
    const std::size_t maxBagStartRow =
        (FrameDebugData::kBagHudSlotCount / kBagColumns) > kVisibleBagRows
        ? (FrameDebugData::kBagHudSlotCount / kBagColumns) - kVisibleBagRows
        : 0;
    const std::size_t bagStartRow = std::min(static_cast<std::size_t>(frameDebugData.craftingBagStartRow), maxBagStartRow);
    for (int row = 0; row < kVisibleBagRows; ++row)
    {
        for (int col = 0; col < 9; ++col)
        {
            const std::size_t slotIndex =
                (bagStartRow + static_cast<std::size_t>(row)) * kBagColumns
                + static_cast<std::size_t>(col);
            const float slotX = layout.inventoryOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
            const float slotY = layout.inventoryOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
            drawSlotFrame(slotX, slotY, false);
            drawSlotContents(frameDebugData.bagSlots[slotIndex], slotX, slotY);
        }
    }

    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        const float slotX = layout.inventoryOriginX + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
        const float slotY = layout.inventoryOriginY + static_cast<float>(kVisibleBagRows) * (layout.slotSize + layout.slotGap);
        const bool selected = static_cast<std::size_t>(slotIndex) == frameDebugData.hotbarSelectedIndex;
        drawSlotFrame(slotX, slotY, selected);
        drawSlotContents(frameDebugData.hotbarSlots[static_cast<std::size_t>(slotIndex)], slotX, slotY);
    }

    if (frameDebugData.craftingCursorSlot.count > 0)
    {
        const float cursorX = frameDebugData.uiCursorX - layout.slotSize * 0.34f;
        const float cursorY = frameDebugData.uiCursorY - layout.slotSize * 0.34f;
        drawSlotContents(frameDebugData.craftingCursorSlot, cursorX, cursorY);
    }
}

void Renderer::drawWorldPickupSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    for (const FrameDebugData::WorldPickupHud& pickup : frameDebugData.worldPickups)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            break;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            break;
        }

        std::uint16_t textureHandle = UINT16_MAX;
        float minU = 0.0f;
        float maxU = 1.0f;
        float minV = 0.0f;
        float maxV = 1.0f;

        if (pickup.itemKind != HudItemKind::None)
        {
            textureHandle = hudItemKindTextureHandle(pickup.itemKind);
            const TextureUvRect uvRect = hudItemKindTextureUv(pickup.itemKind);
            minU = uvRect.minU;
            maxU = uvRect.maxU;
            minV = uvRect.minV;
            maxV = uvRect.maxV;
        }
        if (textureHandle == UINT16_MAX && pickup.blockType != vibecraft::world::BlockType::Air)
        {
            if (chunkAtlasTextureHandle_ == UINT16_MAX)
            {
                continue;
            }
            textureHandle = chunkAtlasTextureHandle_;
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                pickup.blockType,
                vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
            minU = tileX * tileWidth;
            maxU = minU + tileWidth;
            minV = tileY * tileHeight;
            maxV = minV + tileHeight;
        }
        if (textureHandle == UINT16_MAX)
        {
            continue;
        }

        const glm::vec3 center = pickup.worldPosition;
        glm::vec3 toCamera = cameraFrameData.position - center;
        const float distance = std::max(0.001f, glm::length(toCamera));
        if (distance > 0.001f)
        {
            toCamera /= distance;
        }
        else
        {
            toCamera = -cameraFrameData.forward;
        }

        glm::vec3 right = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), toCamera);
        if (glm::dot(right, right) < 1.0e-6f)
        {
            right = glm::cross(cameraFrameData.up, toCamera);
        }
        if (glm::dot(right, right) < 1.0e-6f)
        {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(toCamera, right);
        if (glm::dot(up, up) < 1.0e-6f)
        {
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        up = glm::normalize(up);

        const float cosA = std::cos(pickup.spinRadians);
        const float sinA = std::sin(pickup.spinRadians);
        const glm::vec3 rolledRight = glm::normalize(right * cosA + up * sinA);
        const glm::vec3 rolledUp = glm::normalize(-right * sinA + up * cosA);
        const float scale = glm::clamp(0.19f + 2.1f / std::max(distance, 3.2f), 0.19f, 0.28f);
        right = rolledRight * scale;
        up = rolledUp * (scale * 1.06f);
        const std::uint32_t pickupAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = center.x - right.x - up.x,
                .y = center.y - right.y - up.y,
                .z = center.z - right.z - up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = minU,
                .v = maxV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x + right.x - up.x,
                .y = center.y + right.y - up.y,
                .z = center.z + right.z - up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = maxU,
                .v = maxV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x + right.x + up.x,
                .y = center.y + right.y + up.y,
                .z = center.z + right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = maxU,
                .v = minV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x - right.x + up.x,
                .y = center.y - right.y + up.y,
                .z = center.z - right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = minU,
                .v = minV,
                .abgr = pickupAbgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
    }
}

void Renderer::drawWorldBirdSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || ambientBirdTextureHandle_ == UINT16_MAX)
    {
        return;
    }

    for (const FrameDebugData::WorldBirdHud& bird : frameDebugData.worldBirds)
    {
        const glm::vec3 toBird = bird.worldPosition - cameraFrameData.position;
        const float distanceSq = glm::dot(toBird, toBird);
        if (distanceSq < 16.0f * 16.0f || distanceSq > 220.0f * 220.0f)
        {
            continue;
        }
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4
            || bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            break;
        }

        const float dist = std::sqrt(distanceSq);
        // Softer pop-in / fade so flocks feel embedded in the scene depth.
        float alpha = bird.alpha;
        alpha *= glm::smoothstep(16.5f, 36.0f, dist);
        alpha *= 1.0f - glm::smoothstep(168.0f, 218.0f, dist);
        alpha = glm::clamp(alpha, 0.0f, 1.0f);
        const float distScale = glm::clamp(0.86f + 38.0f / std::max(dist, 14.0f), 0.86f, 1.14f);

        const float sp = std::sin(bird.flapPhase);
        const float wingAsym = (sp < 0.0f) ? 1.14f : 0.93f;
        const float wingSpan = std::abs(sp) * wingAsym;
        const float halfWidth = std::max(
            0.28f,
            bird.halfWidth * distScale * (0.90f + wingSpan * 0.22f));
        const float halfHeight = std::max(
            0.14f,
            bird.halfHeight * distScale * (1.04f - std::abs(std::cos(bird.flapPhase)) * 0.11f));

        glm::vec3 fwd(bird.flightForwardXZ.x, 0.0f, bird.flightForwardXZ.y);
        const float fwdLenSq = glm::dot(fwd, fwd);
        if (fwdLenSq > 1.0e-10f)
        {
            fwd /= std::sqrt(fwdLenSq);
        }
        else
        {
            fwd = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::vec3 toCamera = cameraFrameData.position - bird.worldPosition;
        toCamera.y = 0.0f;
        if (glm::dot(toCamera, toCamera) > 1.0e-6f)
        {
            toCamera = glm::normalize(toCamera);
            if (glm::dot(toCamera, fwd) < 0.0f)
            {
                fwd = -fwd;
            }
        }

        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 wingAxis = glm::cross(worldUp, fwd);
        if (glm::dot(wingAxis, wingAxis) < 1.0e-10f)
        {
            wingAxis = glm::cross(fwd, glm::vec3(1.0f, 0.0f, 0.0f));
        }
        wingAxis = glm::normalize(wingAxis);
        const glm::vec3 bodyUp = glm::normalize(glm::cross(fwd, wingAxis));

        const float roll = glm::clamp(
            bird.bankAngle + 0.52f * std::sin(bird.flapPhase),
            -0.92f,
            0.92f);
        const float cr = std::cos(roll);
        const float sr = std::sin(roll);
        const glm::vec3 wingR = wingAxis * cr + bodyUp * sr;
        const glm::vec3 upR = -wingAxis * sr + bodyUp * cr;

        const glm::vec3 right = wingR * halfWidth;
        const glm::vec3 up = upR * halfHeight;
        const std::uint32_t abgr = detail::packAbgr8(bird.tint, alpha);

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = bird.worldPosition.x - right.x - up.x,
                .y = bird.worldPosition.y - right.y - up.y,
                .z = bird.worldPosition.z - right.z - up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = ambientBirdTextureUv_.minU,
                .v = ambientBirdTextureUv_.maxV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = bird.worldPosition.x + right.x - up.x,
                .y = bird.worldPosition.y + right.y - up.y,
                .z = bird.worldPosition.z + right.z - up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = ambientBirdTextureUv_.maxU,
                .v = ambientBirdTextureUv_.maxV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = bird.worldPosition.x + right.x + up.x,
                .y = bird.worldPosition.y + right.y + up.y,
                .z = bird.worldPosition.z + right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = ambientBirdTextureUv_.maxU,
                .v = ambientBirdTextureUv_.minV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = bird.worldPosition.x - right.x + up.x,
                .y = bird.worldPosition.y - right.y + up.y,
                .z = bird.worldPosition.z - right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = ambientBirdTextureUv_.minU,
                .v = ambientBirdTextureUv_.minV,
                .abgr = abgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(ambientBirdTextureHandle_));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
    }
}

void Renderer::drawWorldMobSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    const auto toAbgrShade = [](const float shade)
    {
        const float clamped = std::clamp(shade, 0.0f, 1.0f);
        const std::uint8_t g = static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
        return (0xffu << 24u) | (static_cast<std::uint32_t>(g) << 16u)
            | (static_cast<std::uint32_t>(g) << 8u) | static_cast<std::uint32_t>(g);
    };

    const auto submitFace = [&](const glm::vec3& a,
                                const glm::vec3& b,
                                const glm::vec3& c,
                                const glm::vec3& d,
                                const TextureUvRect& uv,
                                const std::uint32_t abgr,
                                const std::uint16_t textureHandle)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return false;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return false;
        }

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = a.x,
                .y = a.y,
                .z = a.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = uv.minU,
                .v = uv.maxV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = b.x,
                .y = b.y,
                .z = b.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = uv.maxU,
                .v = uv.maxV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = c.x,
                .y = c.y,
                .z = c.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = uv.maxU,
                .v = uv.minV,
                .abgr = abgr},
            detail::ChunkVertex{
                .x = d.x,
                .y = d.y,
                .z = d.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = uv.minU,
                .v = uv.minV,
                .abgr = abgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };
    const auto resolveHeldSpriteTexture = [&](const FrameDebugData::WorldMobHud& mob,
                                              std::uint16_t& textureHandle,
                                              float& minU,
                                              float& maxU,
                                              float& minV,
                                              float& maxV)
    {
        textureHandle = UINT16_MAX;
        minU = 0.0f;
        maxU = 1.0f;
        minV = 0.0f;
        maxV = 1.0f;

        if (mob.heldItemKind != HudItemKind::None)
        {
            textureHandle = hudItemKindTextureHandle(mob.heldItemKind);
            const TextureUvRect uvRect = hudItemKindTextureUv(mob.heldItemKind);
            minU = uvRect.minU;
            maxU = uvRect.maxU;
            minV = uvRect.minV;
            maxV = uvRect.maxV;
        }
        if (textureHandle == UINT16_MAX && mob.heldBlockType != vibecraft::world::BlockType::Air)
        {
            if (chunkAtlasTextureHandle_ == UINT16_MAX)
            {
                return false;
            }
            textureHandle = chunkAtlasTextureHandle_;
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                mob.heldBlockType,
                vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
            minU = tileX * tileWidth;
            maxU = minU + tileWidth;
            minV = tileY * tileHeight;
            maxV = minV + tileHeight;
        }
        return textureHandle != UINT16_MAX;
    };

    for (const FrameDebugData::WorldMobHud& mob : frameDebugData.worldMobs)
    {
        const std::uint16_t mobTextureHandle = mobTextureHandleForKind(mob.mobKind);
        if (mobTextureHandle == UINT16_MAX)
        {
            continue;
        }
        const MobUvLayout uv = uvLayoutForMobKind(mob.mobKind);
        const float halfWidth = std::max(0.14f, mob.halfWidth);
        const float height = std::max(0.24f, mob.height);

        const float yawSin = std::sin(mob.yawRadians);
        const float yawCos = std::cos(mob.yawRadians);
        const glm::vec3 forward(yawSin, 0.0f, yawCos);
        const glm::vec3 right(yawCos, 0.0f, -yawSin);
        const float pitchSin = std::sin(mob.pitchRadians);
        const float pitchCos = std::cos(mob.pitchRadians);
        const glm::vec3 headForward = glm::normalize(forward * pitchCos + glm::vec3(0.0f, pitchSin, 0.0f));
        const glm::vec3 headUp = glm::normalize(glm::cross(headForward, right));
        const float sx = halfWidth / referenceHalfWidthPxForMobKind(mob.mobKind);
        const float sy = height / referenceHeightPxForMobKind(mob.mobKind);
        const float sz = sx;
        const auto submitHeldItemSprite = [&](const glm::vec3& centerOffsetPx,
                                              const float halfWidthPx,
                                              const float halfHeightPx)
        {
            std::uint16_t textureHandle = UINT16_MAX;
            float minU = 0.0f;
            float maxU = 1.0f;
            float minV = 0.0f;
            float maxV = 1.0f;
            if (!resolveHeldSpriteTexture(mob, textureHandle, minU, maxU, minV, maxV))
            {
                return true;
            }

            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 spriteRight = right * (halfWidthPx * sx);
            const glm::vec3 spriteUp(0.0f, halfHeightPx * sy, 0.0f);
            return submitFace(
                center - spriteRight - spriteUp,
                center + spriteRight - spriteUp,
                center + spriteRight + spriteUp,
                center - spriteRight + spriteUp,
                TextureUvRect{
                    .minU = minU,
                    .maxU = maxU,
                    .minV = minV,
                    .maxV = maxV,
                },
                0xffffffff,
                textureHandle);
        };

        const auto submitOrientedCuboid = [&](const glm::vec3& centerOffsetPx,
                                              const glm::vec3& halfExtentsWorld,
                                              const glm::vec3& axisX,
                                              const glm::vec3& axisY,
                                              const glm::vec3& axisZ,
                                              const CuboidUvSet& uvSet)
        {
            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 dx = axisX * halfExtentsWorld.x;
            const glm::vec3 dy = axisY * halfExtentsWorld.y;
            const glm::vec3 dz = axisZ * halfExtentsWorld.z;

            const glm::vec3 lbf = center - dx - dy + dz;
            const glm::vec3 rbf = center + dx - dy + dz;
            const glm::vec3 lbb = center - dx - dy - dz;
            const glm::vec3 rbb = center + dx - dy - dz;
            const glm::vec3 ltf = center - dx + dy + dz;
            const glm::vec3 rtf = center + dx + dy + dz;
            const glm::vec3 ltb = center - dx + dy - dz;
            const glm::vec3 rtb = center + dx + dy - dz;

            if (!submitFace(lbf, rbf, rtf, ltf, uvSet.front, toAbgrShade(1.0f), mobTextureHandle)) return false;
            if (!submitFace(rbb, lbb, ltb, rtb, uvSet.back, toAbgrShade(0.82f), mobTextureHandle)) return false;
            if (!submitFace(lbb, lbf, ltf, ltb, uvSet.left, toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(rbf, rbb, rtb, rtf, uvSet.right, toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(ltf, rtf, rtb, ltb, uvSet.top, toAbgrShade(0.92f), mobTextureHandle)) return false;
            if (!submitFace(lbb, rbb, rbf, lbf, uvSet.bottom, toAbgrShade(0.62f), mobTextureHandle)) return false;
            return true;
        };

        const auto submitCuboid = [&](const glm::vec3& centerOffsetPx,
                                      const glm::vec3& halfExtentsPx,
                                      const CuboidUvSet& uvSet)
        {
            return submitOrientedCuboid(
                centerOffsetPx,
                glm::vec3(halfExtentsPx.x * sx, halfExtentsPx.y * sy, halfExtentsPx.z * sz),
                right,
                glm::vec3(0.0f, 1.0f, 0.0f),
                forward,
                uvSet);
        };
        const auto submitHorizontalBody = [&](const glm::vec3& centerOffsetPx,
                                              const glm::vec3& halfExtentsPx,
                                              const CuboidUvSet& uvSet)
        {
            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 dx = right * (halfExtentsPx.x * sx);
            const glm::vec3 dy(0.0f, halfExtentsPx.z * sy, 0.0f);
            const glm::vec3 dz = forward * (halfExtentsPx.y * sz);

            const glm::vec3 lbf = center - dx - dy + dz;
            const glm::vec3 rbf = center + dx - dy + dz;
            const glm::vec3 lbb = center - dx - dy - dz;
            const glm::vec3 rbb = center + dx - dy - dz;
            const glm::vec3 ltf = center - dx + dy + dz;
            const glm::vec3 rtf = center + dx + dy + dz;
            const glm::vec3 ltb = center - dx + dy - dz;
            const glm::vec3 rtb = center + dx + dy - dz;

            if (!submitFace(lbf, rbf, rtf, ltf, uvSet.top, toAbgrShade(1.0f), mobTextureHandle)) return false;
            if (!submitFace(rbb, lbb, ltb, rtb, uvSet.bottom, toAbgrShade(0.82f), mobTextureHandle)) return false;
            if (!submitFace(lbb, lbf, ltf, ltb, uvSet.left, toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(rbf, rbb, rtb, rtf, uvSet.right, toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(ltf, rtf, rtb, ltb, uvSet.back, toAbgrShade(0.92f), mobTextureHandle)) return false;
            if (!submitFace(lbb, rbb, rbf, lbf, uvSet.front, toAbgrShade(0.62f), mobTextureHandle)) return false;
            return true;
        };

        using MK = vibecraft::game::MobKind;
        switch (mob.mobKind)
        {
        case MK::Player:
        {
            // Player silhouette/proportions match classic Minecraft character dimensions.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 3.1f
                + mob.feetPosition.x * 0.19f
                + mob.feetPosition.z * 0.16f;
            const float armSwing = std::sin(gaitPhase) * 1.25f;
            const float legSwing = std::sin(gaitPhase + kPi) * 1.1f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.2f;

            if (!submitCuboid(glm::vec3(0.0f, 18.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 28.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right,
                    headUp,
                    headForward,
                    uv.head))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f + bodyBob, armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f + bodyBob, -armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, -legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (mob.heldItemUsesSwordPose)
            {
                if (!submitHeldItemSprite(glm::vec3(8.7f, 15.5f + bodyBob, -armSwing - 1.0f), 1.35f, 4.1f)) break;
            }
            else if (mob.heldItemKind != HudItemKind::None || mob.heldBlockType != vibecraft::world::BlockType::Air)
            {
                if (!submitHeldItemSprite(glm::vec3(8.1f, 15.8f + bodyBob, -armSwing - 0.3f), 2.1f, 2.1f)) break;
            }
            break;
        }
        case MK::Zombie:
        {
            // Zombie keeps classic humanoid proportions, close to the player silhouette.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 3.2f
                + mob.feetPosition.x * 0.37f
                + mob.feetPosition.z * 0.29f;
            const float armSwing = std::sin(gaitPhase) * 1.25f;
            const float legSwing = std::sin(gaitPhase + kPi) * 1.05f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.2f;

            if (!submitCuboid(glm::vec3(0.0f, 18.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 28.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right,
                    headUp,
                    headForward,
                    uv.head))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f + bodyBob, armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f + bodyBob, -armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, -legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            break;
        }
        case MK::Cow:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 13.2f, 0.0f),
                    glm::vec3(6.4f, 8.4f, 4.8f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 15.0f, 8.4f), glm::vec3(3.0f, 3.0f, 4.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 13.5f, 12.7f), glm::vec3(1.5f, 1.4f, 2.0f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 17.3f, 8.9f), glm::vec3(0.5f, 0.8f, 0.7f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(2.0f, 17.3f, 8.9f), glm::vec3(0.5f, 0.8f, 0.7f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            break;
        case MK::Pig:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 7.8f, -0.8f),
                    glm::vec3(5.4f, 7.8f, 3.6f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 8.3f, 7.4f), glm::vec3(3.6f, 3.0f, 3.6f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 7.8f, 11.2f), glm::vec3(1.6f, 1.0f, 1.2f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-2.6f, 2.8f, 4.4f), glm::vec3(1.2f, 2.8f, 1.2f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.6f, 2.8f, 4.4f), glm::vec3(1.2f, 2.8f, 1.2f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-3.5f, 2.8f, -3.8f), glm::vec3(1.2f, 2.8f, 1.2f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(3.5f, 2.8f, -3.8f), glm::vec3(1.2f, 2.8f, 1.2f), uv.leg)) break;
            break;
        case MK::Sheep:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 10.0f, 0.0f),
                    glm::vec3(6.0f, 8.0f, 4.0f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 11.0f, 7.4f), glm::vec3(2.5f, 3.1f, 3.5f), uv.head)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            break;
        case MK::Chicken:
            if (!submitCuboid(glm::vec3(0.0f, 7.0f, -0.2f), glm::vec3(2.4f, 3.2f, 3.0f), uv.body)) break;
            if (!submitCuboid(glm::vec3(0.0f, 10.4f, 3.5f), glm::vec3(1.8f, 2.3f, 1.7f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 7.8f, -3.7f), glm::vec3(0.7f, 0.7f, 1.6f), uv.wattle)) break;
            if (!submitCuboid(glm::vec3(0.0f, 9.8f, 5.5f), glm::vec3(1.0f, 0.6f, 1.1f), uv.beak)) break;
            if (!submitCuboid(glm::vec3(-2.8f, 7.6f, -0.1f), glm::vec3(0.45f, 1.7f, 2.8f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(2.8f, 7.6f, -0.1f), glm::vec3(0.45f, 1.7f, 2.8f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(-0.9f, 2.1f, 0.9f), glm::vec3(0.28f, 2.1f, 0.28f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(0.9f, 2.1f, 0.9f), glm::vec3(0.28f, 2.1f, 0.28f), uv.leg)) break;
            break;
        }

        constexpr float kMobHealthBarDamagedEpsilon = 0.08f;
        const bool mobHealthBarVisible = mob.mobKind != MK::Player && mob.mobHealthMax > 1e-3f
            && mob.mobHealthCurrent > 1e-3f
            && mob.mobHealthCurrent < mob.mobHealthMax - kMobHealthBarDamagedEpsilon;
        if (mobHealthBarVisible && chunkAtlasTextureHandle_ != UINT16_MAX)
        {
            glm::vec3 camForward = cameraFrameData.forward;
            if (glm::dot(camForward, camForward) > 1e-8f)
            {
                camForward = glm::normalize(camForward);
            }
            else
            {
                camForward = glm::vec3(0.0f, 0.0f, -1.0f);
            }
            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 camRight = glm::cross(camForward, worldUp);
            if (glm::dot(camRight, camRight) < 1e-8f)
            {
                camRight = glm::cross(camForward, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            camRight = glm::normalize(camRight);

            const float ratio = std::clamp(mob.mobHealthCurrent / mob.mobHealthMax, 0.0f, 1.0f);
            const glm::vec3 headTop = mob.feetPosition + glm::vec3(0.0f, mob.height + 0.14f, 0.0f);
            const float barHalfW = std::max(0.28f, mob.halfWidth * 1.25f);
            const float barHalfH = 0.045f;
            const glm::vec3 barCenter = headTop + worldUp * 0.05f;

            const std::uint32_t bgAbgr = toAbgrShade(0.14f);
            const std::uint32_t fillAbgr = toAbgrShade(0.3f + 0.62f * ratio);
            const glm::vec3 zBias = camForward * 0.02f;

            const std::uint8_t stoneTileIndex = vibecraft::world::textureTileIndex(
                vibecraft::world::BlockType::Stone,
                vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(stoneTileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(stoneTileIndex / kChunkAtlasTileColumns);
            TextureUvRect stoneUv{
                .minU = tileX * tileWidth,
                .maxU = tileX * tileWidth + tileWidth,
                .minV = tileY * tileHeight,
                .maxV = tileY * tileHeight + tileHeight,
            };

            const glm::vec3 bgA = barCenter - camRight * barHalfW - worldUp * barHalfH;
            const glm::vec3 bgB = barCenter + camRight * barHalfW - worldUp * barHalfH;
            const glm::vec3 bgC = barCenter + camRight * barHalfW + worldUp * barHalfH;
            const glm::vec3 bgD = barCenter - camRight * barHalfW + worldUp * barHalfH;
            static_cast<void>(submitFace(bgA, bgB, bgC, bgD, stoneUv, bgAbgr, chunkAtlasTextureHandle_));

            const float fillHalfW = barHalfW * ratio;
            if (fillHalfW > 1e-4f)
            {
                const glm::vec3 fillCenter = barCenter - camRight * (barHalfW * (1.0f - ratio));
                const float innerH = barHalfH * 0.88f;
                const glm::vec3 fA = fillCenter - camRight * fillHalfW - worldUp * innerH + zBias;
                const glm::vec3 fB = fillCenter + camRight * fillHalfW - worldUp * innerH + zBias;
                const glm::vec3 fC = fillCenter + camRight * fillHalfW + worldUp * innerH + zBias;
                const glm::vec3 fD = fillCenter - camRight * fillHalfW + worldUp * innerH + zBias;
                static_cast<void>(submitFace(fA, fB, fC, fD, stoneUv, fillAbgr, chunkAtlasTextureHandle_));
            }
        }
    }
}

void Renderer::drawAtlasIcon(
    const float centerX,
    const float centerY,
    const float iconSizePx,
    const std::uint8_t tileIndex)
{
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    const float halfSize = iconSizePx * 0.5f;
    const float x0 = std::floor(centerX - halfSize);
    const float y0 = std::floor(centerY - halfSize);
    const float x1 = x0 + iconSizePx;
    const float y1 = y0 + iconSizePx;

    const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
    const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
    const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
    const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
    const float minU = tileX * tileWidth;
    const float maxU = minU + tileWidth;
    const float minV = tileY * tileHeight;
    const float maxV = minV + tileHeight;

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = minV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = minV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = maxV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = maxV, .abgr = 0xffffffff},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
    std::memcpy(tvb.data, vertices, sizeof(vertices));

    bgfx::TransientIndexBuffer tib{};
    bgfx::allocTransientIndexBuffer(&tib, 6);
    auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(
        0,
        detail::toUniformHandle(inventoryUiSamplerHandle_),
        detail::toTextureHandle(chunkAtlasTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
}

void Renderer::drawBlockBreakingOverlay(const FrameDebugData& frameDebugData)
{
    if (!frameDebugData.hasTarget || !frameDebugData.miningTargetActive
        || inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    const float clampedProgress = std::clamp(frameDebugData.miningTargetProgress, 0.0f, 0.999f);
    const int stage = std::clamp(static_cast<int>(std::floor(clampedProgress * 10.0f)), 0, 9);
    const std::uint16_t textureHandle = blockBreakStageTextureHandles_[static_cast<std::size_t>(stage)];
    if (textureHandle == UINT16_MAX)
    {
        return;
    }

    const auto submitFace = [&](const glm::vec3& a,
                                const glm::vec3& b,
                                const glm::vec3& c,
                                const glm::vec3& d)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return false;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return false;
        }

        constexpr std::uint32_t kOverlayAbgr = 0xd8ffffff;
        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{.x = a.x, .y = a.y, .z = a.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 0.0f, .v = 1.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = b.x, .y = b.y, .z = b.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 1.0f, .v = 1.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = c.x, .y = c.y, .z = c.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 1.0f, .v = 0.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = d.x, .y = d.y, .z = d.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 0.0f, .v = 0.0f, .abgr = kOverlayAbgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LEQUAL);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };

    constexpr float kInset = 0.0015f;
    constexpr float kExpand = 0.0025f;
    const float minX = static_cast<float>(frameDebugData.targetBlock.x) - kInset;
    const float minY = static_cast<float>(frameDebugData.targetBlock.y) - kInset;
    const float minZ = static_cast<float>(frameDebugData.targetBlock.z) - kInset;
    const float maxX = static_cast<float>(frameDebugData.targetBlock.x + 1) + kInset;
    const float maxY = static_cast<float>(frameDebugData.targetBlock.y + 1) + kInset;
    const float maxZ = static_cast<float>(frameDebugData.targetBlock.z + 1) + kInset;

    const glm::vec3 lbf(minX - kExpand, minY - kExpand, maxZ + kExpand);
    const glm::vec3 rbf(maxX + kExpand, minY - kExpand, maxZ + kExpand);
    const glm::vec3 lbb(minX - kExpand, minY - kExpand, minZ - kExpand);
    const glm::vec3 rbb(maxX + kExpand, minY - kExpand, minZ - kExpand);
    const glm::vec3 ltf(minX - kExpand, maxY + kExpand, maxZ + kExpand);
    const glm::vec3 rtf(maxX + kExpand, maxY + kExpand, maxZ + kExpand);
    const glm::vec3 ltb(minX - kExpand, maxY + kExpand, minZ - kExpand);
    const glm::vec3 rtb(maxX + kExpand, maxY + kExpand, minZ - kExpand);

    if (!submitFace(lbf, rbf, rtf, ltf)) return;
    if (!submitFace(rbb, lbb, ltb, rtb)) return;
    if (!submitFace(lbb, lbf, ltf, ltb)) return;
    if (!submitFace(rbf, rbb, rtb, rtf)) return;
    if (!submitFace(ltf, rtf, rtb, ltb)) return;
    static_cast<void>(submitFace(lbb, rbb, rbf, lbf));
}

void Renderer::drawMainMenuChrome(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const int mainMenuTitleContentRowOffset)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0)
    {
        return;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const TextGridMetrics grid = makeTextGridMetrics(width_, height_, textWidth, textHeight);
    const auto drawPanel = [&](const PixelRect& rect,
                               const glm::vec3& fillTint,
                               const glm::vec3& edgeTint,
                               const glm::vec3& glowTint,
                               const float fillAlpha)
    {
        const PixelRect shadow = expandedRect(rect, 12.0f);
        const PixelRect outer = expandedRect(rect, 5.0f);
        const PixelRect inner = expandedRect(rect, 2.0f);
        drawUiSolidRect(shadow.x0, shadow.y0 + 6.0f, shadow.x1, shadow.y1 + 16.0f, detail::packAbgr8(glm::vec3(0.01f, 0.02f, 0.03f), 0.34f));
        drawUiSolidRect(outer.x0, outer.y0, outer.x1, outer.y1, detail::packAbgr8(glowTint, 0.72f));
        drawUiSolidRect(inner.x0, inner.y0, inner.x1, inner.y1, detail::packAbgr8(edgeTint, 0.96f));
        drawUiSolidRect(rect.x0, rect.y0, rect.x1, rect.y1, detail::packAbgr8(fillTint, fillAlpha));
        drawUiSolidRect(
            rect.x0 + 3.0f,
            rect.y0 + 3.0f,
            rect.x1 - 3.0f,
            rect.y0 + std::max(7.0f, (rect.y1 - rect.y0) * 0.18f),
            detail::packAbgr8(glm::mix(fillTint, glm::vec3(1.0f), 0.14f), 0.30f));
    };
    const auto drawButtonPanel = [&](const PixelRect& rect, const bool hovered)
    {
        const glm::vec3 fillTint = hovered ? glm::vec3(0.36f, 0.28f, 0.12f) : glm::vec3(0.09f, 0.14f, 0.20f);
        const glm::vec3 edgeTint = hovered ? glm::vec3(0.92f, 0.76f, 0.40f) : glm::vec3(0.32f, 0.46f, 0.58f);
        const glm::vec3 glowTint = hovered ? glm::vec3(0.70f, 0.56f, 0.20f) : glm::vec3(0.14f, 0.24f, 0.34f);
        drawPanel(rect, fillTint, edgeTint, glowTint, hovered ? 0.92f : 0.84f);
    };

    drawUiSolidRect(
        0.0f,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.02f, 0.07f, 0.10f), 0.16f));
    drawUiSolidRect(
        0.0f,
        static_cast<float>(height_) * 0.72f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.03f, 0.05f, 0.03f), 0.20f));

    if (frameDebugData.mainMenuLoadingActive)
    {
        const int tw = static_cast<int>(textWidth);
        const int th = static_cast<int>(textHeight);
        const int panelWidth = std::clamp(tw - 12, 40, std::max(40, tw - 4));
        const int panelHeight = std::min(13, std::max(11, th - 2));
        const int panelCol = std::max(0, (tw - panelWidth) / 2);
        const int panelRow = std::max(1, (th - panelHeight) / 2);
        drawPanel(
            gridRectFromChars(grid, panelCol, panelRow, panelWidth, panelHeight),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.36f, 0.50f, 0.60f),
            glm::vec3(0.84f, 0.74f, 0.34f),
            0.90f);
        return;
    }

    if (frameDebugData.mainMenuSoundSettingsActive)
    {
        constexpr int kWide = 96;
        const int soundCenterCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
        drawPanel(
            gridRectFromChars(grid, soundCenterCol - 3, 4, kWide + 6, 28),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.34f, 0.46f, 0.56f),
            glm::vec3(0.82f, 0.70f, 0.32f),
            0.92f);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 10, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 1 || frameDebugData.mainMenuSoundSettingsHoveredControl == 2);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 17, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 3 || frameDebugData.mainMenuSoundSettingsHoveredControl == 4);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 26, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 0);
        return;
    }

    if (frameDebugData.mainMenuSingleplayerPanelActive)
    {
        constexpr int kWide = 72;
        constexpr int kPanelHeight = 39;
        const int panelCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
        const int panelRow = std::max(1, (static_cast<int>(textHeight) - kPanelHeight) / 2);
        drawPanel(
            gridRectFromChars(grid, panelCol, panelRow, kWide, kPanelHeight),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.33f, 0.44f, 0.54f),
            glm::vec3(0.80f, 0.70f, 0.34f),
            0.92f);
        for (int buttonRow : {panelRow + 13, panelRow + 20, panelRow + 27, panelRow + 34})
        {
            const int buttonId = (buttonRow - (panelRow + 13)) / 7;
            drawButtonPanel(
                gridRectFromChars(grid, panelCol + 3, buttonRow + 1, kWide - 6, 3),
                frameDebugData.mainMenuSingleplayerHoveredControl == buttonId);
        }
        return;
    }

    if (frameDebugData.mainMenuMultiplayerPanel != FrameDebugData::MainMenuMultiplayerPanel::None)
    {
        const detail::MainMenuComputedLayout menu =
            detail::computeMainMenuLayout(static_cast<int>(textWidth), static_cast<int>(textHeight), mainMenuTitleContentRowOffset);
        const int joinPresetSlots = static_cast<int>(std::min<std::size_t>(frameDebugData.mainMenuJoinPresetButtonLabels.size(), 3));
        const int rowShift = detail::MultiplayerMenuLayout::multiplayerMenuRowShift(
            static_cast<int>(textHeight), frameDebugData.mainMenuMultiplayerPanel, joinPresetSlots, mainMenuTitleContentRowOffset);
        int panelTopRow = 4 + rowShift;
        int panelBottomRow = static_cast<int>(textHeight) - 3;
        switch (frameDebugData.mainMenuMultiplayerPanel)
        {
        case FrameDebugData::MainMenuMultiplayerPanel::Hub:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::kHubBackRow + menu.buttonLineCount + rowShift + 1;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::Host:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::kHostBackRow + menu.buttonLineCount + rowShift + 1;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::Join:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::multiplayerJoinBottomRow(joinPresetSlots) + rowShift;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::None:
        default:
            break;
        }
        drawPanel(
            gridRectFromChars(
                grid,
                menu.centerCol - 4,
                panelTopRow,
                menu.outerWidth + 8,
                std::max(8, panelBottomRow - panelTopRow + 1)),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.32f, 0.43f, 0.54f),
            glm::vec3(0.78f, 0.72f, 0.36f),
            0.92f);
        return;
    }

    const detail::MainMenuComputedLayout menu =
        detail::computeMainMenuLayout(static_cast<int>(textWidth), static_cast<int>(textHeight), mainMenuTitleContentRowOffset);
    drawPanel(
        gridRectFromChars(
            grid,
            menu.centerCol - 4,
            menu.subtitleRow - 2,
            menu.outerWidth + 8,
            std::max(8, menu.iconHintsRow - menu.subtitleRow + 4)),
        glm::vec3(0.08f, 0.12f, 0.18f),
        glm::vec3(0.34f, 0.47f, 0.58f),
        glm::vec3(0.82f, 0.72f, 0.34f),
        0.90f);
    drawPanel(
        gridRectFromChars(
            grid,
            menu.centerCol + menu.outerWidth / 2 - 16,
            menu.subtitleRow - 1,
            32,
            2),
        glm::vec3(0.14f, 0.20f, 0.28f),
        glm::vec3(0.56f, 0.68f, 0.78f),
        glm::vec3(0.88f, 0.76f, 0.34f),
        0.94f);

    for (std::size_t buttonIndex = 0; buttonIndex < menu.buttonTopRows.size(); ++buttonIndex)
    {
        drawButtonPanel(
            gridRectFromChars(
                grid,
                menu.centerCol + 3,
                menu.buttonTopRows[buttonIndex] + 1,
                menu.outerWidth - 6,
                std::max(2, menu.buttonLineCount - 2)),
            frameDebugData.mainMenuHoveredButton == static_cast<int>(buttonIndex));
    }

    if (menu.centerCol >= 8)
    {
        drawButtonPanel(
            gridRectFromChars(grid, menu.centerCol - 8, menu.iconHintsRow - 1, 6, 2),
            frameDebugData.mainMenuHoveredButton == 5);
    }
    drawButtonPanel(
        gridRectFromChars(grid, menu.centerCol + menu.outerWidth + 2, menu.iconHintsRow - 1, 6, 2),
        frameDebugData.mainMenuHoveredButton == 6);
}

void Renderer::drawPauseMenuChrome(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0)
    {
        return;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const TextGridMetrics grid = makeTextGridMetrics(width_, height_, textWidth, textHeight);
    const int th = static_cast<int>(textHeight);
    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
    const auto drawPanel = [&](const PixelRect& rect,
                               const glm::vec3& fillTint,
                               const glm::vec3& edgeTint,
                               const glm::vec3& glowTint,
                               const float fillAlpha)
    {
        const PixelRect shadow = expandedRect(rect, 14.0f);
        const PixelRect outer = expandedRect(rect, 5.0f);
        const PixelRect inner = expandedRect(rect, 2.0f);
        drawUiSolidRect(shadow.x0, shadow.y0 + 8.0f, shadow.x1, shadow.y1 + 18.0f, detail::packAbgr8(glm::vec3(0.01f, 0.01f, 0.02f), 0.42f));
        drawUiSolidRect(outer.x0, outer.y0, outer.x1, outer.y1, detail::packAbgr8(glowTint, 0.74f));
        drawUiSolidRect(inner.x0, inner.y0, inner.x1, inner.y1, detail::packAbgr8(edgeTint, 0.97f));
        drawUiSolidRect(rect.x0, rect.y0, rect.x1, rect.y1, detail::packAbgr8(fillTint, fillAlpha));
        drawUiSolidRect(
            rect.x0 + 3.0f,
            rect.y0 + 3.0f,
            rect.x1 - 3.0f,
            rect.y0 + std::max(7.0f, (rect.y1 - rect.y0) * 0.16f),
            detail::packAbgr8(glm::mix(fillTint, glm::vec3(1.0f), 0.12f), 0.28f));
    };
    const auto drawButtonPanel = [&](const int rowTop, const bool hovered)
    {
        const PixelRect rect = gridRectFromChars(
            grid,
            centerCol + 3,
            rowTop + 1,
            kWide - 6,
            std::max(2, detail::PauseMenuLayout::kButtonRowSpan - 2));
        const glm::vec3 fillTint = hovered ? glm::vec3(0.36f, 0.28f, 0.12f) : glm::vec3(0.09f, 0.14f, 0.21f);
        const glm::vec3 edgeTint = hovered ? glm::vec3(0.92f, 0.76f, 0.40f) : glm::vec3(0.33f, 0.46f, 0.60f);
        const glm::vec3 glowTint = hovered ? glm::vec3(0.72f, 0.54f, 0.20f) : glm::vec3(0.15f, 0.23f, 0.34f);
        drawPanel(rect, fillTint, edgeTint, glowTint, hovered ? 0.94f : 0.86f);
    };

    drawUiSolidRect(
        0.0f,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.02f, 0.05f, 0.09f), 0.44f));

    int panelTopRow = 3;
    int panelBottomRow = th - 3;
    if (frameDebugData.pauseSoundSettingsActive)
    {
        panelTopRow = detail::PauseMenuLayout::pauseSoundTitleRow(th) - 2;
        panelBottomRow = detail::PauseMenuLayout::pauseSoundBackButtonRow(th) + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }
    else if (frameDebugData.pauseGameSettingsActive)
    {
        panelTopRow = detail::PauseMenuLayout::pauseGameTitleRow(th) - 2;
        panelBottomRow = detail::PauseMenuLayout::pauseGameBackButtonRow(th) + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }
    else
    {
        const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
        panelTopRow = std::max(2, firstButtonRow - 4);
        panelBottomRow = firstButtonRow + 4 * detail::PauseMenuLayout::kButtonPitch + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }

    drawPanel(
        gridRectFromChars(
            grid,
            centerCol - 4,
            panelTopRow,
            kWide + 8,
            std::max(10, panelBottomRow - panelTopRow + 1)),
        glm::vec3(0.08f, 0.12f, 0.18f),
        glm::vec3(0.34f, 0.47f, 0.62f),
        glm::vec3(0.84f, 0.74f, 0.36f),
        0.93f);

    if (frameDebugData.pauseSoundSettingsActive)
    {
        const int hovered = frameDebugData.pauseSoundSettingsHoveredControl;
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundMusicButtonRow(th), hovered == 1 || hovered == 2);
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundSfxButtonRow(th), hovered == 3 || hovered == 4);
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundBackButtonRow(th), hovered == 0);
        return;
    }

    if (frameDebugData.pauseGameSettingsActive)
    {
        const int hovered = frameDebugData.pauseGameSettingsHoveredControl;
        drawButtonPanel(detail::PauseMenuLayout::pauseGameMobButtonRow(th), hovered == 1);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameCreativeButtonRow(th), hovered == 2);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameDifficultyButtonRow(th), hovered == 3);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameBiomeButtonRow(th), hovered == 4);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameTravelButtonRow(th), hovered == 5);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameWeatherButtonRow(th), hovered == 6);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameBackButtonRow(th), hovered == 0);
        return;
    }

    const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
    const int hovered = frameDebugData.pauseMenuHoveredButton;
    constexpr int kPitch = detail::PauseMenuLayout::kButtonPitch;
    for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex)
    {
        drawButtonPanel(firstButtonRow + buttonIndex * kPitch, hovered == buttonIndex);
    }
}

void Renderer::drawMainMenuBackground()
{
    static bool loggedReady = false;
    static bool loggedSkipped = false;
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || mainMenuBackgroundTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0
        || mainMenuBackgroundWidthPx_ == 0 || mainMenuBackgroundHeightPx_ == 0)
    {
        if (!loggedSkipped)
        {
            core::logWarning(fmt::format(
                "[menu-bg] skipped: program={} sampler={} texture={} win={}x{} tex={}x{}",
                inventoryUiProgramHandle_,
                inventoryUiSamplerHandle_,
                mainMenuBackgroundTextureHandle_,
                width_,
                height_,
                mainMenuBackgroundWidthPx_,
                mainMenuBackgroundHeightPx_));
            loggedSkipped = true;
        }
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        if (!loggedSkipped)
        {
            core::logWarning("[menu-bg] skipped: transient vertex buffer unavailable");
            loggedSkipped = true;
        }
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        if (!loggedSkipped)
        {
            core::logWarning("[menu-bg] skipped: transient index buffer unavailable");
            loggedSkipped = true;
        }
        return;
    }
    if (!loggedReady)
    {
        core::logInfo(fmt::format(
            "[menu-bg] drawing texture={} size={}x{} in window={}x{}",
            mainMenuBackgroundTextureHandle_,
            mainMenuBackgroundWidthPx_,
            mainMenuBackgroundHeightPx_,
            width_,
            height_));
        loggedReady = true;
    }

    const float winW = static_cast<float>(width_);
    const float winH = static_cast<float>(height_);
    const float imgW = static_cast<float>(mainMenuBackgroundWidthPx_);
    const float imgH = static_cast<float>(mainMenuBackgroundHeightPx_);
    const float imgAspect = imgW / imgH;
    const float winAspect = winW / winH;

    float u0 = 0.0f;
    float u1 = 1.0f;
    float v0 = 0.0f;
    float v1 = 1.0f;
    if (imgAspect > winAspect)
    {
        const float halfU = 0.5f * (winAspect / imgAspect);
        u0 = 0.5f - halfU;
        u1 = 0.5f + halfU;
    }
    else if (imgAspect < winAspect)
    {
        const float halfV = 0.5f * (imgAspect / winAspect);
        v0 = 0.5f - halfV;
        v1 = 0.5f + halfV;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        winW,
        winH,
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    constexpr std::uint32_t kWhiteAbgr = 0xffffffff;
    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = 0.0f, .y = 0.0f, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = u0, .v = v0, .abgr = kWhiteAbgr},
        detail::ChunkVertex{.x = winW, .y = 0.0f, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = u1, .v = v0, .abgr = kWhiteAbgr},
        detail::ChunkVertex{.x = winW, .y = winH, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = u1, .v = v1, .abgr = kWhiteAbgr},
        detail::ChunkVertex{.x = 0.0f, .y = winH, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = u0, .v = v1, .abgr = kWhiteAbgr},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
    std::memcpy(tvb.data, vertices, sizeof(vertices));

    bgfx::TransientIndexBuffer tib{};
    bgfx::allocTransientIndexBuffer(&tib, 6);
    auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(
        0,
        detail::toUniformHandle(inventoryUiSamplerHandle_),
        detail::toTextureHandle(mainMenuBackgroundTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
}

void Renderer::drawMainMenuLogo()
{
    if (logoProgramHandle_ == UINT16_MAX || logoTextureHandle_ == UINT16_MAX || logoSamplerHandle_ == UINT16_MAX)
    {
        return;
    }
    if (logoWidthPx_ == 0 || logoHeightPx_ == 0 || width_ == 0 || height_ == 0)
    {
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    const float aspect =
        static_cast<float>(logoWidthPx_) / static_cast<float>(logoHeightPx_);
    using namespace detail::MainMenuLogoDraw;
    const float maxWidth = std::min(kMaxWidthCapPx, static_cast<float>(width_) * kMaxWidthFrac);
    float drawW = maxWidth;
    float drawH = drawW / aspect;
    const float maxHeight = std::min(static_cast<float>(height_) * kMaxHeightFrac, kMaxHeightCapPx);
    if (drawH > maxHeight)
    {
        drawH = maxHeight;
        drawW = drawH * aspect;
    }

    const float x0 = (static_cast<float>(width_) - drawW) * 0.5f;
    const float y0 = kMarginTopPx;
    const float x1 = x0 + drawW;
    const float y1 = y0 + drawH;

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 0.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 0.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 1.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 1.0f, .abgr = 0xffffffff},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
    std::memcpy(tvb.data, vertices, sizeof(vertices));

    bgfx::TransientIndexBuffer tib{};
    bgfx::allocTransientIndexBuffer(&tib, 6);
    auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, detail::toUniformHandle(logoSamplerHandle_), detail::toTextureHandle(logoTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kUiView, detail::toProgramHandle(logoProgramHandle_));
}

void Renderer::drawHeldItemOverlay(const FrameDebugData& frameDebugData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || width_ == 0 || height_ == 0)
    {
        return;
    }

    const std::size_t selectedIndex = std::min<std::size_t>(
        frameDebugData.hotbarSelectedIndex,
        frameDebugData.hotbarSlots.size() - 1U);
    const FrameDebugData::HotbarSlotHud selectedSlot = frameDebugData.hotbarSlots[selectedIndex];
    if (selectedSlot.count == 0)
    {
        return;
    }

    std::uint16_t textureHandle = UINT16_MAX;
    float minU = 0.0f;
    float maxU = 1.0f;
    float minV = 0.0f;
    float maxV = 1.0f;
    float halfW = 0.0f;
    float halfH = 0.0f;
    float baseRotationDeg = 0.0f;

    textureHandle = hudItemKindTextureHandle(selectedSlot.itemKind);
    if (textureHandle != UINT16_MAX)
    {
        const TextureUvRect uvRect = hudItemKindTextureUv(selectedSlot.itemKind);
        minU = uvRect.minU;
        maxU = uvRect.maxU;
        minV = uvRect.minV;
        maxV = uvRect.maxV;
        if (selectedSlot.heldItemUsesSwordPose)
        {
            halfH = std::min(static_cast<float>(height_) * 0.46f, 420.0f);
            halfW = halfH * 0.58f;
            baseRotationDeg = -40.0f;
        }
        else
        {
            halfH = std::min(static_cast<float>(height_) * 0.24f, 192.0f);
            halfW = halfH;
            baseRotationDeg = -24.0f;
        }
    }
    else if (
        selectedSlot.itemKind == HudItemKind::None
        && selectedSlot.blockType != vibecraft::world::BlockType::Air)
    {
        if (chunkAtlasTextureHandle_ == UINT16_MAX)
        {
            return;
        }
        textureHandle = chunkAtlasTextureHandle_;
        const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
            selectedSlot.blockType,
            vibecraft::world::BlockFace::Side);
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        minU = tileX * tileWidth;
        maxU = minU + tileWidth;
        minV = tileY * tileHeight;
        maxV = minV + tileHeight;
        halfH = std::min(static_cast<float>(height_) * 0.24f, 192.0f);
        halfW = halfH;
        baseRotationDeg = -24.0f;
    }
    else
    {
        return;
    }

    const float swing = std::clamp(frameDebugData.heldItemSwing, 0.0f, 1.0f);
    const float swingEase = std::sin(swing * 3.1415926535f);

    const float centerX =
        static_cast<float>(width_) - halfW * 0.34f + swingEase * 28.0f;
    const float centerY =
        static_cast<float>(height_) - halfH * 0.26f + swingEase * 26.0f;
    const float rotationRadians = (baseRotationDeg - swingEase * 28.0f) * (3.1415926535f / 180.0f);
    const float cosA = std::cos(rotationRadians);
    const float sinA = std::sin(rotationRadians);

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);
    const auto submitLayer = [&](const float offsetX,
                                 const float offsetY,
                                 const float scaleX,
                                 const float scaleY,
                                 const std::uint32_t abgr)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return false;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return false;
        }

        const auto rotatePoint = [centerX, centerY, cosA, sinA, offsetX, offsetY](const float x, const float y)
        {
            return glm::vec2{
                centerX + offsetX + x * cosA - y * sinA,
                centerY + offsetY + x * sinA + y * cosA,
            };
        };

        const glm::vec2 p0 = rotatePoint(-halfW * scaleX, -halfH * scaleY);
        const glm::vec2 p1 = rotatePoint(+halfW * scaleX, -halfH * scaleY);
        const glm::vec2 p2 = rotatePoint(+halfW * scaleX, +halfH * scaleY);
        const glm::vec2 p3 = rotatePoint(-halfW * scaleX, +halfH * scaleY);

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{.x = p0.x, .y = p0.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = minV, .abgr = abgr},
            detail::ChunkVertex{.x = p1.x, .y = p1.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = minV, .abgr = abgr},
            detail::ChunkVertex{.x = p2.x, .y = p2.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = maxV, .abgr = abgr},
            detail::ChunkVertex{.x = p3.x, .y = p3.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = maxV, .abgr = abgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };

    const bool usesSwordPose = selectedSlot.heldItemUsesSwordPose;
    const float squashX = usesSwordPose ? 1.01f : 0.98f;
    const float squashY = usesSwordPose ? 1.02f : 1.01f;
    static_cast<void>(submitLayer(0.0f, 0.0f, squashX, squashY, 0xffffffff));
}

void Renderer::drawCrosshairOverlay()
{
    if (crosshairProgramHandle_ == UINT16_MAX
        || crosshairTextureHandle_ == UINT16_MAX
        || crosshairSamplerHandle_ == UINT16_MAX)
    {
        return;
    }
    if (width_ == 0 || height_ == 0)
    {
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    constexpr float kCrosshairSizePx = 15.0f;
    const float x0 = std::floor((static_cast<float>(width_) - kCrosshairSizePx) * 0.5f);
    const float y0 = std::floor((static_cast<float>(height_) - kCrosshairSizePx) * 0.5f);
    const float x1 = x0 + kCrosshairSizePx;
    const float y1 = y0 + kCrosshairSizePx;

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 0.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 0.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 1.0f, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 1.0f, .abgr = 0xffffffff},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
    std::memcpy(tvb.data, vertices, sizeof(vertices));

    bgfx::TransientIndexBuffer tib{};
    bgfx::allocTransientIndexBuffer(&tib, 6);
    auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(
        0,
        detail::toUniformHandle(crosshairSamplerHandle_),
        detail::toTextureHandle(crosshairTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kUiView, detail::toProgramHandle(crosshairProgramHandle_));
}

} // namespace vibecraft::render
