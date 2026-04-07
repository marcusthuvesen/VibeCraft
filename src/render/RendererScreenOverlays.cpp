#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include "vibecraft/core/Logger.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
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
            halfH = std::min(static_cast<float>(height_) * 0.52f, 480.0f);
            halfW = halfH * 0.58f;
            baseRotationDeg = -40.0f;
        }
        else
        {
            halfH = std::min(static_cast<float>(height_) * 0.34f, 280.0f);
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
        halfH = std::min(static_cast<float>(height_) * 0.34f, 280.0f);
        halfW = halfH;
        baseRotationDeg = -24.0f;
    }
    else
    {
        return;
    }

    const float swing = std::clamp(frameDebugData.heldItemSwing, 0.0f, 1.0f);
    // Phase 0..1: one forward arc then return (matches Application heldItemSwing_ advance).
    const float swingEase = std::sin(swing * 3.1415926535f);

    const float centerX =
        static_cast<float>(width_) - halfW * 0.34f + swingEase * 42.0f;
    const float centerY =
        static_cast<float>(height_) - halfH * 0.26f + swingEase * 38.0f;
    const float rotationRadians = (baseRotationDeg - swingEase * 38.0f) * (3.1415926535f / 180.0f);
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
                                 const std::uint32_t abgr,
                                 const std::uint16_t layerTextureHandle,
                                 const float layerMinU,
                                 const float layerMaxU,
                                 const float layerMinV,
                                 const float layerMaxV)
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
            detail::ChunkVertex{.x = p0.x, .y = p0.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = layerMinU, .v = layerMinV, .abgr = abgr},
            detail::ChunkVertex{.x = p1.x, .y = p1.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = layerMaxU, .v = layerMinV, .abgr = abgr},
            detail::ChunkVertex{.x = p2.x, .y = p2.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = layerMaxU, .v = layerMaxV, .abgr = abgr},
            detail::ChunkVertex{.x = p3.x, .y = p3.y, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = layerMinU, .v = layerMaxV, .abgr = abgr},
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
            detail::toTextureHandle(layerTextureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };

    if (playerMobTextureHandle_ != UINT16_MAX)
    {
        constexpr float kPlayerTexW = 64.0f;
        constexpr float kPlayerTexH = 32.0f;
        const float skinInsetU = 0.5f / kPlayerTexW;
        const float skinInsetV = 0.5f / kPlayerTexH;
        // Right arm front face from classic player skin, mirrored for first-person right hand.
        const float armMinU = (44.0f + skinInsetU) / kPlayerTexW;
        const float armMaxU = (48.0f - skinInsetU) / kPlayerTexW;
        const float armMinV = (20.0f + skinInsetV) / kPlayerTexH;
        const float armMaxV = (32.0f - skinInsetV) / kPlayerTexH;

        const float armScaleX = selectedSlot.heldItemUsesSwordPose ? 0.46f : 0.50f;
        const float armScaleY = selectedSlot.heldItemUsesSwordPose ? 0.88f : 0.82f;
        static_cast<void>(submitLayer(
            -halfW * 0.76f,
            +halfH * 0.62f,
            armScaleX,
            armScaleY,
            0xffffffff,
            playerMobTextureHandle_,
            armMaxU,
            armMinU,
            armMinV,
            armMaxV));
    }

    const bool usesSwordPose = selectedSlot.heldItemUsesSwordPose;
    const float squashX = usesSwordPose ? 1.01f : 0.98f;
    const float squashY = usesSwordPose ? 1.02f : 1.01f;
    static_cast<void>(submitLayer(
        0.0f,
        0.0f,
        squashX,
        squashY,
        0xffffffff,
        textureHandle,
        minU,
        maxU,
        minV,
        maxV));
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
}  // namespace vibecraft::render
