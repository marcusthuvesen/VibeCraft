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
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
namespace
{
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
    case MK::HostileStalker:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 8.0f, 12.0f, 4.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .arm = makeCuboidUvSet(kTexW, kTexH, 40.0f, 16.0f, 4.0f, 12.0f, 4.0f),
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
            .body = makeCuboidUvSet(kTexW, kTexH, 28.0f, 8.0f, 8.0f, 16.0f, 6.0f),
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
    case MK::HostileStalker:
        return 32.0f;
    case MK::Cow:
        return 20.0f;
    case MK::Pig:
        return 14.0f;
    case MK::Sheep:
        return 14.0f;
    case MK::Chicken:
        return 14.0f;
    }
    return 16.0f;
}

[[nodiscard]] float referenceHalfWidthPxForMobKind(const vibecraft::game::MobKind mobKind)
{
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::HostileStalker:
        return 6.0f;
    case MK::Cow:
        return 6.0f;
    case MK::Pig:
        return 5.0f;
    case MK::Sheep:
        return 4.0f;
    case MK::Chicken:
        return 4.0f;
    }
    return 4.0f;
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
    const auto drawUiTextureRect = [&](const float x0,
                                       const float y0,
                                       const float x1,
                                       const float y1,
                                       const std::uint16_t textureHandle)
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
    const auto itemTextureHandle = [&](const HudItemKind itemKind)
    {
        return hudItemKindTextureHandle(itemKind);
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
            drawUiTextureRect(ix0, iy0, ix1, iy1, textureHandle);
            return;
        }
        if (slotHud.blockType == vibecraft::world::BlockType::Air)
        {
            return;
        }

        const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
            slotHud.blockType,
            vibecraft::world::BlockFace::Side);
        drawAtlasIcon(
            std::floor((ix0 + ix1) * 0.5f),
            std::floor((iy0 + iy1) * 0.5f),
            std::max(1.0f, std::min(ix1 - ix0, iy1 - iy0)),
            tileIndex);
    };

    if (canDrawSolid && hotbarLayout.slotSize > 0.0f)
    {
        const std::uint32_t hotbarFrameOuterAbgr = detail::packAbgr8(glm::vec3(0.10f, 0.10f, 0.10f), 0.94f);
        const std::uint32_t hotbarFrameInnerAbgr = detail::packAbgr8(glm::vec3(0.22f, 0.22f, 0.22f), 0.93f);
        const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.06f, 0.06f, 0.06f), 0.97f);
        const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.20f, 0.20f, 0.20f), 0.95f);
        const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.30f, 0.30f, 0.30f), 0.96f);
        const std::uint32_t selectedOuterAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.98f);
        const std::uint32_t selectedInnerAbgr = detail::packAbgr8(glm::vec3(0.88f, 0.88f, 0.88f), 0.98f);
        const std::uint32_t xpTrackAbgr = detail::packAbgr8(glm::vec3(0.08f, 0.08f, 0.08f), 0.92f);
        const std::uint32_t xpFillAbgr = detail::packAbgr8(glm::vec3(0.45f, 0.83f, 0.18f), 0.98f);

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
        detail::computeCraftingOverlayLayoutPx(width_, height_, frameDebugData.craftingUsesWorkbench);
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
    const auto drawUiTextureRect = [&](const float x0,
                                       const float y0,
                                       const float x1,
                                       const float y1,
                                       const std::uint16_t textureHandle)
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

    const std::uint32_t dimAbgr = detail::packAbgr8(glm::vec3(0.02f, 0.03f, 0.04f), 0.52f);
    const std::uint32_t panelOuterAbgr = detail::packAbgr8(glm::vec3(0.10f, 0.10f, 0.10f), 0.96f);
    const std::uint32_t panelInnerAbgr = detail::packAbgr8(glm::vec3(0.19f, 0.19f, 0.19f), 0.96f);
    const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.06f, 0.06f, 0.06f), 0.98f);
    const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.24f, 0.24f, 0.24f), 0.97f);
    const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.33f, 0.33f, 0.33f), 0.97f);
    const std::uint32_t resultGlowAbgr = detail::packAbgr8(glm::vec3(0.40f, 0.65f, 0.24f), 0.55f);
    const std::uint32_t cursorOutlineAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.95f);

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
        const float iconSize = std::clamp(std::round(layout.slotSize * 0.75f), 18.0f, 34.0f);
        const std::uint16_t textureHandle = hudItemKindTextureHandle(slotHud.itemKind);

        if (textureHandle != UINT16_MAX)
        {
            drawUiTextureRect(
                centerX - iconSize * 0.5f,
                centerY - iconSize * 0.5f,
                centerX + iconSize * 0.5f,
                centerY + iconSize * 0.5f,
                textureHandle);
        }
        else if (slotHud.blockType != vibecraft::world::BlockType::Air)
        {
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            drawAtlasIcon(centerX, centerY, iconSize, tileIndex);
        }

        if (slotHud.count > 1)
        {
            const std::string digits = fmt::format("{}", std::min(slotHud.count, 99u));
            int col = static_cast<int>(std::floor((x + layout.slotSize - charWidthPx * 0.45f) / charWidthPx))
                - static_cast<int>(digits.size()) + 1;
            int row = static_cast<int>(std::floor((y + layout.slotSize - charHeightPx * 1.05f) / charHeightPx));
            col = std::clamp(col, 0, static_cast<int>(textWidth) - static_cast<int>(digits.size()));
            row = std::clamp(row, 0, static_cast<int>(textHeight) - 1);
            bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), 0x0f, "%s", digits.c_str());
        }
    };

    const int craftingColumns = frameDebugData.craftingUsesWorkbench ? 3 : 2;
    const int craftingRows = frameDebugData.craftingUsesWorkbench ? 3 : 2;
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
        drawUiSolidRect(cursorX - 1.0f, cursorY - 1.0f, cursorX + layout.slotSize * 0.68f + 1.0f, cursorY + layout.slotSize * 0.68f + 1.0f, cursorOutlineAbgr);
        drawSlotContents(frameDebugData.craftingCursorSlot, cursorX, cursorY);
    }
}

void Renderer::drawWorldPickupSprites(const FrameDebugData& frameDebugData)
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

        std::uint16_t textureHandle = chunkAtlasTextureHandle_;
        float minU = 0.0f;
        float maxU = 1.0f;
        float minV = 0.0f;
        float maxV = 1.0f;
        if (pickup.itemKind != HudItemKind::None)
        {
            textureHandle = hudItemKindTextureHandle(pickup.itemKind);
        }
        if (textureHandle == UINT16_MAX && pickup.blockType != vibecraft::world::BlockType::Air)
        {
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
            detail::toTextureHandle(textureHandle));
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
    static_cast<void>(cameraFrameData);
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
        const float sx = halfWidth / referenceHalfWidthPxForMobKind(mob.mobKind);
        const float sy = height / referenceHeightPxForMobKind(mob.mobKind);
        const float sz = sx;

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
        case MK::HostileStalker:
            if (!submitCuboid(glm::vec3(0.0f, 18.0f, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitCuboid(glm::vec3(0.0f, 28.0f, 0.0f), glm::vec3(4.0f, 4.0f, 4.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f, 0.0f), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f, 0.0f), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, 0.0f), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, 0.0f), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            break;
        case MK::Cow:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 13.8f, 0.3f),
                    glm::vec3(6.0f, 9.0f, 5.0f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 13.9f, 8.8f), glm::vec3(4.0f, 4.0f, 3.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 13.0f, 12.3f), glm::vec3(2.2f, 1.6f, 1.4f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 19.2f, 8.6f), glm::vec3(0.5f, 1.3f, 0.5f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(2.5f, 19.2f, 8.6f), glm::vec3(0.5f, 1.3f, 0.5f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            break;
        case MK::Pig:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 8.8f, 0.0f),
                    glm::vec3(5.0f, 8.0f, 4.0f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 8.7f, 8.6f), glm::vec3(4.0f, 4.0f, 4.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 8.1f, 12.8f), glm::vec3(2.1f, 1.5f, 1.2f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-3.0f, 3.0f, 4.8f), glm::vec3(1.5f, 3.0f, 1.5f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(3.0f, 3.0f, 4.8f), glm::vec3(1.5f, 3.0f, 1.5f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-3.0f, 3.0f, -4.8f), glm::vec3(1.5f, 3.0f, 1.5f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(3.0f, 3.0f, -4.8f), glm::vec3(1.5f, 3.0f, 1.5f), uv.leg)) break;
            break;
        case MK::Sheep:
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 9.0f, 0.0f),
                    glm::vec3(4.0f * sx, 8.0f * sz, 3.0f * sy),
                    right,
                    forward,
                    glm::vec3(0.0f, -1.0f, 0.0f),
                    uv.body))
            {
                break;
            }
            if (!submitCuboid(glm::vec3(0.0f, 9.0f, 6.0f), glm::vec3(3.0f, 3.0f, 4.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            break;
        case MK::Chicken:
            if (!submitCuboid(glm::vec3(0.0f, 7.0f, 0.0f), glm::vec3(3.0f, 4.0f, 3.0f), uv.body)) break;
            if (!submitCuboid(glm::vec3(0.0f, 11.0f, 4.5f), glm::vec3(2.0f, 3.0f, 1.5f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 10.0f, 7.0f), glm::vec3(2.0f, 1.0f, 1.0f), uv.beak)) break;
            if (!submitCuboid(glm::vec3(0.0f, 8.0f, 6.3f), glm::vec3(1.0f, 1.0f, 0.8f), uv.wattle)) break;
            if (!submitCuboid(glm::vec3(-3.4f, 7.0f, 0.0f), glm::vec3(0.5f, 2.2f, 2.6f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(3.4f, 7.0f, 0.0f), glm::vec3(0.5f, 2.2f, 2.6f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(-1.0f, 2.5f, 0.0f), glm::vec3(0.45f, 2.5f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(1.0f, 2.5f, 0.0f), glm::vec3(0.45f, 2.5f, 0.45f), uv.leg)) break;
            break;
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

void Renderer::drawMainMenuBackground()
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || mainMenuBackgroundTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0
        || mainMenuBackgroundWidthPx_ == 0 || mainMenuBackgroundHeightPx_ == 0)
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
    bgfx::setViewTransform(detail::kMainView, view, proj);

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
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
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
    const auto rotatePoint = [centerX, centerY, cosA, sinA](const float x, const float y)
    {
        return glm::vec2{
            centerX + x * cosA - y * sinA,
            centerY + x * sinA + y * cosA,
        };
    };

    const glm::vec2 p0 = rotatePoint(-halfW, -halfH);
    const glm::vec2 p1 = rotatePoint(+halfW, -halfH);
    const glm::vec2 p2 = rotatePoint(+halfW, +halfH);
    const glm::vec2 p3 = rotatePoint(-halfW, +halfH);

    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
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

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{.x = p0.x, .y = p0.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = minV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = p1.x, .y = p1.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = minV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = p2.x, .y = p2.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = maxU, .v = maxV, .abgr = 0xffffffff},
        detail::ChunkVertex{.x = p3.x, .y = p3.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = minU, .v = maxV, .abgr = 0xffffffff},
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
