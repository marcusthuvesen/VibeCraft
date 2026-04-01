#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vibecraft::render
{
void Renderer::drawTextureIcon(
    const float centerX,
    const float centerY,
    const float iconSizePx,
    const std::uint16_t textureHandle,
    const TextureUvRect& uvRect)
{
    if (textureHandle == UINT16_MAX
        || inventoryUiProgramHandle_ == UINT16_MAX
        || inventoryUiSamplerHandle_ == UINT16_MAX)
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

    const float halfSize = iconSizePx * 0.5f;
    const float x0 = std::floor(centerX - halfSize);
    const float y0 = std::floor(centerY - halfSize);
    const float x1 = x0 + iconSizePx;
    const float y1 = y0 + iconSizePx;

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

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

void Renderer::drawSurvivalStatusHud(const FrameDebugData& frameDebugData)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX
        || inventoryUiProgramHandle_ == UINT16_MAX
        || inventoryUiSamplerHandle_ == UINT16_MAX
        || width_ == 0
        || height_ == 0)
    {
        return;
    }

    const bgfx::Stats* const stats = bgfx::getStats();
    const std::uint16_t textHeight = stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;
    const std::uint16_t hotbarRow = textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    const detail::HotbarLayoutPx hotbarLayout =
        detail::computeHotbarLayoutPx(width_, height_, textHeight, hotbarRow);
    if (hotbarLayout.slotSize <= 0.0f)
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

    const float heartSize = std::clamp(std::round(hotbarLayout.slotSize * 0.72f), 28.0f, 92.0f);
    const float xpBarH = std::max(2.0f, std::round(hotbarLayout.slotSize * 0.09f));
    const float xpGap = std::max(2.0f, std::round(hotbarLayout.slotSize * 0.11f));
    const float xpY1 = hotbarLayout.slotTopY - xpGap;
    const float xpY0 = xpY1 - xpBarH;
    const float heartsY = std::floor(xpY0 - std::max(8.0f, heartSize * 0.56f) - heartSize);

    const float panelWidth = std::min(
        std::clamp(hotbarLayout.totalWidth * 0.56f, 220.0f, 320.0f),
        std::max(180.0f, static_cast<float>(width_) - 24.0f));
    const float panelHeight = std::clamp(std::round(hotbarLayout.slotSize * 0.58f), 32.0f, 48.0f);
    const float panelX = std::floor((static_cast<float>(width_) - panelWidth) * 0.5f);
    const float panelY = std::max(12.0f, std::floor(heartsY - panelHeight - std::max(10.0f, heartSize * 0.4f)));

    const std::uint32_t outerAbgr = detail::packAbgr8(
        frameDebugData.oxygenLowWarning ? glm::vec3(0.44f, 0.12f, 0.12f) : glm::vec3(0.05f, 0.08f, 0.12f),
        0.94f);
    const std::uint32_t innerAbgr = detail::packAbgr8(
        frameDebugData.oxygenLowWarning ? glm::vec3(0.20f, 0.07f, 0.07f) : glm::vec3(0.10f, 0.16f, 0.22f),
        0.96f);
    const std::uint32_t iconFrameAbgr = detail::packAbgr8(glm::vec3(0.20f, 0.30f, 0.40f), 0.98f);
    const std::uint32_t iconFillAbgr = detail::packAbgr8(glm::vec3(0.07f, 0.11f, 0.15f), 0.98f);
    const std::uint32_t trackAbgr = detail::packAbgr8(glm::vec3(0.03f, 0.05f, 0.08f), 0.98f);
    const std::uint32_t fillAbgr = detail::packAbgr8(
        frameDebugData.oxygenLowWarning
            ? glm::vec3(0.92f, 0.40f, 0.24f)
            : (frameDebugData.oxygenInsideSafeZone ? glm::vec3(0.30f, 0.87f, 0.88f) : glm::vec3(0.22f, 0.67f, 0.96f)),
        0.99f);
    const std::uint32_t markerAbgr = detail::packAbgr8(glm::vec3(0.80f, 0.90f, 1.0f), 0.18f);
    const std::uint32_t statusAbgr = detail::packAbgr8(
        frameDebugData.oxygenInsideSafeZone ? glm::vec3(0.55f, 0.98f, 0.72f) : glm::vec3(0.70f, 0.82f, 0.96f),
        0.98f);
    const std::uint32_t zoneAccentAbgr = detail::packAbgr8(
        frameDebugData.oxygenLowWarning
            ? glm::vec3(0.92f, 0.42f, 0.20f)
            : (frameDebugData.oxygenInsideSafeZone ? glm::vec3(0.34f, 0.92f, 0.66f) : glm::vec3(0.44f, 0.66f, 0.92f)),
        0.98f);

    drawUiSolidRect(panelX, panelY, panelX + panelWidth, panelY + panelHeight, outerAbgr);
    drawUiSolidRect(panelX + 1.0f, panelY + 1.0f, panelX + panelWidth - 1.0f, panelY + panelHeight - 1.0f, innerAbgr);

    const float inset = std::max(4.0f, std::round(panelHeight * 0.14f));
    const float iconSize = panelHeight - inset * 2.0f;
    const float iconX0 = panelX + inset;
    const float iconY0 = panelY + inset;
    drawUiSolidRect(iconX0, iconY0, iconX0 + iconSize, iconY0 + iconSize, iconFrameAbgr);
    drawUiSolidRect(iconX0 + 1.0f, iconY0 + 1.0f, iconX0 + iconSize - 1.0f, iconY0 + iconSize - 1.0f, iconFillAbgr);

    const std::uint16_t tankTextureHandle = hudItemKindTextureHandle(frameDebugData.oxygenTankItemKind);
    if (tankTextureHandle != UINT16_MAX)
    {
        drawTextureIcon(
            iconX0 + iconSize * 0.5f,
            iconY0 + iconSize * 0.5f,
            std::max(8.0f, iconSize * 0.76f),
            tankTextureHandle,
            hudItemKindTextureUv(frameDebugData.oxygenTankItemKind));
    }

    const float statusSize = std::max(8.0f, std::round(panelHeight * 0.22f));
    const float trackX0 = iconX0 + iconSize + inset;
    const float trackX1 = panelX + panelWidth - inset - statusSize - inset * 0.6f;
    const float trackY0 = panelY + panelHeight * 0.32f;
    const float trackY1 = panelY + panelHeight * 0.68f;
    drawUiSolidRect(trackX0, panelY + 2.0f, trackX1, panelY + 4.0f, zoneAccentAbgr);
    drawUiSolidRect(trackX0, trackY0, trackX1, trackY1, trackAbgr);

    const float clampedMaxOxygen = std::max(0.0f, frameDebugData.maxOxygen);
    const float clampedOxygen = std::clamp(frameDebugData.oxygen, 0.0f, clampedMaxOxygen);
    const float fillRatio = clampedMaxOxygen > 0.0f ? clampedOxygen / clampedMaxOxygen : 0.0f;
    if (fillRatio > 0.001f)
    {
        drawUiSolidRect(trackX0, trackY0, trackX0 + (trackX1 - trackX0) * fillRatio, trackY1, fillAbgr);
    }

    constexpr int kBarMarkers = 4;
    for (int marker = 1; marker < kBarMarkers; ++marker)
    {
        const float markerX = trackX0 + (trackX1 - trackX0) * (static_cast<float>(marker) / static_cast<float>(kBarMarkers));
        drawUiSolidRect(markerX, trackY0 + 2.0f, markerX + 1.0f, trackY1 - 2.0f, markerAbgr);
    }

    const float statusX0 = panelX + panelWidth - inset - statusSize;
    const float statusY0 = panelY + std::floor((panelHeight - statusSize) * 0.5f);
    drawUiSolidRect(statusX0, statusY0, statusX0 + statusSize, statusY0 + statusSize, statusAbgr);
}
}  // namespace vibecraft::render
