#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
namespace
{
constexpr float kTileInsetU = 0.5f / static_cast<float>(kChunkAtlasWidthPx);
constexpr float kTileInsetV = 0.5f / static_cast<float>(kChunkAtlasHeightPx);

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
        // Minecraft-like gray HUD treatment with subtle contrast and white selection.
        const std::uint32_t hotbarFrameOuterAbgr = detail::packAbgr8(glm::vec3(0.05f, 0.05f, 0.05f), 0.94f);
        const std::uint32_t hotbarFrameInnerAbgr = detail::packAbgr8(glm::vec3(0.18f, 0.18f, 0.18f), 0.93f);
        const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.08f, 0.08f, 0.08f), 0.98f);
        const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.34f, 0.34f, 0.34f), 0.95f);
        const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.46f, 0.46f, 0.46f), 0.90f);
        const std::uint32_t selectedOuterAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.98f);
        const std::uint32_t selectedInnerAbgr = detail::packAbgr8(glm::vec3(0.76f, 0.76f, 0.76f), 0.96f);
        const std::uint32_t xpTrackAbgr = detail::packAbgr8(glm::vec3(0.10f, 0.10f, 0.10f), 0.94f);
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

    if (!canDrawSolid)
    {
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
}

void Renderer::drawInventoryPlayerPreview(
    const float previewCenterX,
    const float previewTop,
    const float slotSize)
{
    if (playerMobTextureHandle_ == UINT16_MAX)
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

    const float figurePixelHeight = 32.0f;
    const float desiredHeight = slotSize * 4.3f;
    const float scale = desiredHeight / figurePixelHeight;
    const float figurePixelWidth = 16.0f;
    const float previewWidth = figurePixelWidth * scale;
    const float previewLeft = previewCenterX - previewWidth * 0.5f;
    const float previewBottom = previewTop + desiredHeight;
    const float padX = slotSize * 0.35f;
    const float padY = slotSize * 0.25f;
    const std::uint32_t bgAbgr = detail::packAbgr8(glm::vec3(0.12f, 0.12f, 0.12f), 0.88f);
    const std::uint32_t borderAbgr = detail::packAbgr8(glm::vec3(0.72f, 0.72f, 0.72f), 0.94f);
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

    const auto drawUiTextureRectUv = [this](const float x0,
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

    // Player texture layout (64x32): front faces derived from makeCuboidUvSet(u,v,w,h,d).front
    // = uvRectFromPixels(texW, texH, u+d, v+d, w, h)
    constexpr float kTexW = 64.0f;
    constexpr float kTexH = 32.0f;
    const TextureUvRect headFront = uvRectFromPixels(kTexW, kTexH,  8.0f,  8.0f, 8.0f,  8.0f);
    const TextureUvRect bodyFront = uvRectFromPixels(kTexW, kTexH, 20.0f, 20.0f, 8.0f, 12.0f);
    const TextureUvRect armFront  = uvRectFromPixels(kTexW, kTexH, 44.0f, 20.0f, 4.0f, 12.0f);
    const TextureUvRect legFront  = uvRectFromPixels(kTexW, kTexH,  4.0f, 20.0f, 4.0f, 12.0f);

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

    drawPart(headFront, 4.0f, 0.0f, 8.0f, 8.0f);
    drawPart(bodyFront, 4.0f, 8.0f, 8.0f, 12.0f);
    drawPart(armFront, 0.0f, 8.0f, 4.0f, 12.0f);
    TextureUvRect armRight = armFront;
    std::swap(armRight.minU, armRight.maxU);
    drawPart(armRight, 12.0f, 8.0f, 4.0f, 12.0f);
    drawPart(legFront, 4.0f, 20.0f, 4.0f, 12.0f);
    TextureUvRect legRight = legFront;
    std::swap(legRight.minU, legRight.maxU);
    drawPart(legRight, 8.0f, 20.0f, 4.0f, 12.0f);
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

} // namespace vibecraft::render
