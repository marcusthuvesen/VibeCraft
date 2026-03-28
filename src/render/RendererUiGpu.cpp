#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{

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

    const detail::HotbarLayoutPx hotbarLayout = detail::computeHotbarLayoutPx(width_, height_, textHeight, hotbarRow);
    const std::array<std::uint16_t, 3> bagRows = {bagRow0, bagRow1, bagRow2};
    const bool canDrawSolid = inventoryUiSolidProgramHandle_ != UINT16_MAX;

    if (canDrawSolid && hotbarLayout.slotSize > 0.0f)
    {
        const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.09f, 0.09f, 0.11f), 0.62f);
        const std::uint32_t slotFillSelectedAbgr = detail::packAbgr8(glm::vec3(0.07f, 0.07f, 0.1f), 0.72f);
        const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.92f);
        const std::uint32_t xpTrackAbgr = detail::packAbgr8(glm::vec3(0.06f, 0.14f, 0.05f), 0.88f);
        const std::uint32_t xpFillAbgr = detail::packAbgr8(glm::vec3(0.35f, 0.95f, 0.32f), 0.96f);

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

        const float slot = hotbarLayout.slotSize;
        const float gap = hotbarLayout.gap;
        const float ox = hotbarLayout.originX;
        const float sy0 = hotbarLayout.slotTopY;

        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
        {
            const float sx = ox + static_cast<float>(slotIndex) * (slot + gap);
            if (slotIndex == frameDebugData.hotbarSelectedIndex)
            {
                drawUiSolidRect(sx - 2.0f, sy0 - 2.0f, sx + slot + 2.0f, sy0 + slot + 2.0f, slotBorderAbgr);
            }
        }

        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
        {
            const float sx = ox + static_cast<float>(slotIndex) * (slot + gap);
            const bool selected = slotIndex == frameDebugData.hotbarSelectedIndex;
            drawUiSolidRect(
                sx,
                sy0,
                sx + slot,
                sy0 + slot,
                selected ? slotFillSelectedAbgr : slotFillAbgr);
        }
    }

    const float iconBase = hotbarLayout.slotSize > 0.0f
        ? std::clamp(std::floor(hotbarLayout.slotSize * 0.7f), 14.0f, 36.0f)
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
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            const float iconScale = slotIndex == frameDebugData.hotbarSelectedIndex ? 1.06f : 1.0f;
            drawAtlasIcon(centerX, centerY, iconBase * iconScale, tileIndex);
        }
    }
    else
    {
        const float charWidthPx = static_cast<float>(width_) / static_cast<float>(textWidth);
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
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            const float iconSize =
                slotIndex == frameDebugData.hotbarSelectedIndex ? 14.0f : 13.0f;
            drawAtlasIcon(centerX, centerY, iconSize, tileIndex);
        }
    }

    constexpr float kBagIconScale = 0.94f;
    const float bagIconSize = std::max(11.0f, iconBase * kBagIconScale);
    const float charWidthPx = static_cast<float>(width_) / static_cast<float>(textWidth);
    const int bagStartCol = detail::computeCenteredColumnStart(detail::computeBagGridWidthChars());
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        for (int colIndex = 0; colIndex < 9; ++colIndex)
        {
            const std::size_t slotIdx = static_cast<std::size_t>(rowIndex * 9 + colIndex);
            const FrameDebugData::HotbarSlotHud& slotHud = frameDebugData.bagSlots[slotIdx];
            if (slotHud.count == 0)
            {
                continue;
            }
            const float centerX =
                static_cast<float>(bagStartCol + colIndex * 8) * charWidthPx + charWidthPx * 3.5f;
            const float centerY = static_cast<float>(bagRows[static_cast<std::size_t>(rowIndex)]) * charHeightPx
                + charHeightPx * 0.42f;
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            drawAtlasIcon(centerX, centerY, bagIconSize, tileIndex);
        }
    }
}

void Renderer::drawWorldPickupSprites(const FrameDebugData& frameDebugData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX)
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

        const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
            pickup.blockType,
            vibecraft::world::BlockFace::Side);
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        const float minU = tileX * tileWidth;
        const float maxU = minU + tileWidth;
        const float minV = tileY * tileHeight;
        const float maxV = minV + tileHeight;

        constexpr float kHalfWidth = 0.18f;
        constexpr float kHalfHeight = 0.18f;
        const float cosA = std::cos(pickup.spinRadians);
        const float sinA = std::sin(pickup.spinRadians);
        const glm::vec3 right(cosA * kHalfWidth, 0.0f, sinA * kHalfWidth);
        const glm::vec3 up(0.0f, kHalfHeight, 0.0f);
        const glm::vec3 center = pickup.worldPosition;

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
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = center.x + right.x - up.x,
                .y = center.y + right.y - up.y,
                .z = center.z + right.z - up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = maxU,
                .v = maxV,
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = center.x + right.x + up.x,
                .y = center.y + right.y + up.y,
                .z = center.z + right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = maxU,
                .v = minV,
                .abgr = 0xffffffff},
            detail::ChunkVertex{
                .x = center.x - right.x + up.x,
                .y = center.y - right.y + up.y,
                .z = center.z - right.z + up.z,
                .nx = 0.0f,
                .ny = 1.0f,
                .nz = 0.0f,
                .u = minU,
                .v = minV,
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
            detail::toTextureHandle(chunkAtlasTextureHandle_));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
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
    constexpr float kMarginTop = 32.0f;
    const float maxWidth = std::min(640.0f, static_cast<float>(width_) * 0.82f);
    float drawW = maxWidth;
    float drawH = drawW / aspect;
    const float maxHeight = std::min(static_cast<float>(height_) * 0.17f, 200.0f);
    if (drawH > maxHeight)
    {
        drawH = maxHeight;
        drawW = drawH * aspect;
    }

    const float x0 = (static_cast<float>(width_) - drawW) * 0.5f;
    const float y0 = kMarginTop;
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
