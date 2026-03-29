#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <glm/common.hpp>
#include <string>

#include "debugdraw.h"

namespace vibecraft::render
{


void Renderer::renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData)
{
    if (!initialized_ || width_ == 0 || height_ == 0)
    {
        return;
    }

    if (frameDebugData.mainMenuActive)
    {
        bgfx::setViewClear(detail::kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, detail::kMainMenuClearColor, 1.0f, 0);
        bgfx::setViewRect(detail::kMainView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
        bgfx::setViewRect(detail::kUiView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
        bgfx::touch(detail::kMainView);
        bgfx::touch(detail::kUiView);
        drawMainMenuBackground();
        drawMainMenuLogo();
        bgfx::dbgTextClear();
        const bgfx::Stats* const menuStats = bgfx::getStats();
        const std::uint16_t menuTextHeight = menuStats != nullptr ? menuStats->textHeight : 30;
        const std::uint16_t menuTextWidth =
            menuStats != nullptr && menuStats->textWidth > 0 ? menuStats->textWidth : 100;
        detail::drawMainMenuOverlay(frameDebugData, menuTextWidth, menuTextHeight);
        bgfx::frame();
        return;
    }

    bgfx::setViewClear(detail::kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x263238ff, 1.0f, 0);

    const bx::Vec3 eye(cameraFrameData.position.x, cameraFrameData.position.y, cameraFrameData.position.z);
    const bx::Vec3 at(
        cameraFrameData.position.x + cameraFrameData.forward.x,
        cameraFrameData.position.y + cameraFrameData.forward.y,
        cameraFrameData.position.z + cameraFrameData.forward.z);
    const bx::Vec3 up(cameraFrameData.up.x, cameraFrameData.up.y, cameraFrameData.up.z);

    float view[16];
    float projection[16];
    bx::mtxLookAt(view, eye, at, up);
    bx::mtxProj(
        projection,
        cameraFrameData.verticalFovDegrees,
        static_cast<float>(width_) / static_cast<float>(height_),
        cameraFrameData.nearClip,
        cameraFrameData.farClip,
        bgfx::getCaps()->homogeneousDepth);

    const std::uint32_t clearColor = detail::packRgba8(cameraFrameData.skyTint);
    bgfx::setViewClear(detail::kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::setViewRect(detail::kMainView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
    bgfx::setViewRect(detail::kUiView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
    bgfx::setViewTransform(detail::kMainView, view, projection);
    bgfx::touch(detail::kMainView);
    bgfx::touch(detail::kUiView);

    const glm::vec3 ambientLight = glm::clamp(
        cameraFrameData.skyTint * 0.45f
            + cameraFrameData.horizonTint * 0.15f
            + cameraFrameData.sunLightTint * (0.08f * cameraFrameData.sunVisibility)
            + cameraFrameData.moonLightTint * (0.05f * cameraFrameData.moonVisibility),
        glm::vec3(0.04f),
        glm::vec3(0.72f));
    const glm::vec3 sunLightColor =
        cameraFrameData.sunLightTint * glm::max(cameraFrameData.sunVisibility, 0.0f);
    const glm::vec3 moonLightColor =
        cameraFrameData.moonLightTint * glm::max(cameraFrameData.moonVisibility * 0.35f, 0.0f);

    std::uint32_t visibleChunkCount = 0;
    if (chunkProgramHandle_ != UINT16_MAX)
    {
        const float identityTransform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        float viewProjection[16];
        bx::mtxMul(viewProjection, view, projection);
        bx::Plane frustumPlanes[6] = {
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
        };
        bx::buildFrustumPlanes(frustumPlanes, viewProjection);

        const float chunkMaxDrawDistance = cameraFrameData.farClip * 1.08f;
        const float chunkMaxDrawDistanceSq = chunkMaxDrawDistance * chunkMaxDrawDistance;

        for (const auto& [sceneMeshId, sceneMesh] : sceneMeshes_)
        {
            static_cast<void>(sceneMeshId);
            const bx::Aabb aabb{
                bx::Vec3(sceneMesh.boundsMin.x, sceneMesh.boundsMin.y, sceneMesh.boundsMin.z),
                bx::Vec3(sceneMesh.boundsMax.x, sceneMesh.boundsMax.y, sceneMesh.boundsMax.z),
            };

            if (!detail::isAabbInsideFrustum(frustumPlanes, aabb))
            {
                continue;
            }

            if (detail::distanceSqCameraToAabb(cameraFrameData.position, sceneMesh.boundsMin, sceneMesh.boundsMax)
                > chunkMaxDrawDistanceSq)
            {
                continue;
            }

            ++visibleChunkCount;
            bgfx::setTransform(identityTransform);
            bgfx::setVertexBuffer(0, detail::toVertexBufferHandle(sceneMesh.vertexBufferHandle));
            bgfx::setIndexBuffer(
                detail::toIndexBufferHandle(sceneMesh.indexBufferHandle),
                0,
                sceneMesh.indexCount);
            if (chunkAtlasTextureHandle_ != UINT16_MAX && chunkAtlasSamplerHandle_ != UINT16_MAX)
            {
                bgfx::setTexture(
                    0,
                    detail::toUniformHandle(chunkAtlasSamplerHandle_),
                    detail::toTextureHandle(chunkAtlasTextureHandle_));
            }
            detail::setVec4Uniform(chunkSunDirectionUniformHandle_, cameraFrameData.sunDirection, 0.0f);
            detail::setVec4Uniform(chunkSunLightColorUniformHandle_, sunLightColor, 0.0f);
            detail::setVec4Uniform(chunkMoonDirectionUniformHandle_, cameraFrameData.moonDirection, 0.0f);
            detail::setVec4Uniform(chunkMoonLightColorUniformHandle_, moonLightColor, 0.0f);
            detail::setVec4Uniform(chunkAmbientLightUniformHandle_, ambientLight, 0.0f);
            bgfx::setState(detail::kChunkRenderState);
            bgfx::submit(detail::kMainView, detail::toProgramHandle(chunkProgramHandle_));
        }
    }

    drawWorldPickupSprites(frameDebugData);

    DebugDrawEncoder debugDrawEncoder;
    debugDrawEncoder.begin(detail::kMainView);
    detail::drawWeatherClouds(debugDrawEncoder, cameraFrameData);
    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(false);
    constexpr float kCelestialDistance = 240.0f;
    constexpr float kSunRadius = 17.0f;
    constexpr float kMoonRadius = 11.0f;
    if (cameraFrameData.sunVisibility > 0.01f)
    {
        const glm::vec3 sunPosition = cameraFrameData.position + cameraFrameData.sunDirection * kCelestialDistance;
        const bx::Vec3 sunCenter(sunPosition.x, sunPosition.y, sunPosition.z);
        const bx::Vec3 sunFacingNormal(
            cameraFrameData.position.x - sunPosition.x,
            cameraFrameData.position.y - sunPosition.y,
            cameraFrameData.position.z - sunPosition.z);
        const glm::vec3 sunTint = glm::mix(cameraFrameData.horizonTint, cameraFrameData.sunLightTint, 0.78f);
        debugDrawEncoder.setColor(detail::packAbgr8(sunTint, 1.0f));
        debugDrawEncoder.drawQuad(sunFacingNormal, sunCenter, kSunRadius * 2.2f);
    }
    if (cameraFrameData.moonVisibility > 0.01f)
    {
        const glm::vec3 moonPosition = cameraFrameData.position + cameraFrameData.moonDirection * kCelestialDistance;
        const bx::Vec3 moonCenter(moonPosition.x, moonPosition.y, moonPosition.z);
        const bx::Vec3 moonFacingNormal(
            cameraFrameData.position.x - moonPosition.x,
            cameraFrameData.position.y - moonPosition.y,
            cameraFrameData.position.z - moonPosition.z);
        const glm::vec3 moonTint = glm::mix(cameraFrameData.skyTint, cameraFrameData.moonLightTint, 0.72f);
        debugDrawEncoder.setColor(detail::packAbgr8(moonTint, 1.0f));
        debugDrawEncoder.drawQuad(moonFacingNormal, moonCenter, kMoonRadius * 2.0f);
    }
    debugDrawEncoder.pop();
    detail::drawWeatherRain(debugDrawEncoder, cameraFrameData);
    if (frameDebugData.showWorldOriginGuides)
    {
        debugDrawEncoder.setColor(0xff455a64);
        debugDrawEncoder.drawGrid(Axis::Y, bx::Vec3(0.0f, 0.0f, 0.0f), 48, 1.0f);
        debugDrawEncoder.drawAxis(0.0f, 0.0f, 0.0f, 2.0f);
    }

    if (frameDebugData.hasTarget)
    {
        const bx::Aabb targetAabb{
            bx::Vec3(
                static_cast<float>(frameDebugData.targetBlock.x),
                static_cast<float>(frameDebugData.targetBlock.y),
                static_cast<float>(frameDebugData.targetBlock.z)),
            bx::Vec3(
                static_cast<float>(frameDebugData.targetBlock.x + 1),
                static_cast<float>(frameDebugData.targetBlock.y + 1),
                static_cast<float>(frameDebugData.targetBlock.z + 1)),
        };

        debugDrawEncoder.setWireframe(true);
        debugDrawEncoder.setColor(0xff26c6da);
        debugDrawEncoder.draw(targetAabb);
        debugDrawEncoder.setWireframe(false);
    }

    debugDrawEncoder.end();
    drawWorldMobSprites(frameDebugData, cameraFrameData);
    drawBlockBreakingOverlay(frameDebugData);

    bgfx::dbgTextClear();
    const bgfx::Stats* const bgfxStats = bgfx::getStats();
    const std::uint16_t textHeight = bgfxStats != nullptr ? bgfxStats->textHeight : 30;
    const std::uint16_t textWidthForHud =
        bgfxStats != nullptr && bgfxStats->textWidth > 0 ? bgfxStats->textWidth : 100;

    if (frameDebugData.pauseMenuActive)
    {
        detail::drawPauseMenuOverlay(frameDebugData, textWidthForHud, textHeight);
        const std::uint16_t pauseHealthRow = textHeight > 2 ? static_cast<std::uint16_t>(textHeight - 3) : 0;
        detail::drawHealthHud(pauseHealthRow, frameDebugData);
        bgfx::frame();
        return;
    }

    drawCrosshairOverlay();
    drawHeldItemOverlay(frameDebugData);

    const std::uint16_t hotbarRow = textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    const std::uint16_t hotbarKeyRow = textHeight > 1 ? static_cast<std::uint16_t>(textHeight - 2) : 0;
    const std::uint16_t healthRow = textHeight > 2 ? static_cast<std::uint16_t>(textHeight - 3) : 0;
    const std::uint16_t bagRow2 = textHeight > 3 ? static_cast<std::uint16_t>(textHeight - 4) : 0;
    const std::uint16_t bagRow1 = textHeight > 4 ? static_cast<std::uint16_t>(textHeight - 5) : 0;
    const std::uint16_t bagRow0 = textHeight > 5 ? static_cast<std::uint16_t>(textHeight - 6) : 0;
    const std::uint16_t bagSepRow = textHeight > 6 ? static_cast<std::uint16_t>(textHeight - 7) : 0;
    const std::uint16_t bagTitleRow = textHeight > 7 ? static_cast<std::uint16_t>(textHeight - 8) : 0;
    const std::uint16_t controlsRow1 = textHeight > 8 ? static_cast<std::uint16_t>(textHeight - 9) : 0;
    const std::uint16_t controlsRow0 = textHeight > 9 ? static_cast<std::uint16_t>(textHeight - 10) : 0;

    drawInventoryItemIcons(frameDebugData, textWidthForHud, textHeight, hotbarRow, bagRow0, bagRow1, bagRow2);

    bgfx::dbgTextPrintf(0, 1, 0x0f, "VibeCraft foundation slice");
    bgfx::dbgTextPrintf(0, 3, 0x0a, "%s", frameDebugData.statusLine.c_str());
    bgfx::dbgTextPrintf(
        0,
        5,
        0x0f,
        "Chunks: %u  Dirty: %u  Resident: %u",
        frameDebugData.chunkCount,
        frameDebugData.dirtyChunkCount,
        frameDebugData.residentChunkCount);
    bgfx::dbgTextPrintf(0, 6, 0x0f, "Faces: %u  Visible chunks: %u", frameDebugData.totalFaces, visibleChunkCount);
    if (bgfxStats != nullptr)
    {
        const double cpuFrameMs = bgfxStats->cpuTimerFreq > 0
            ? static_cast<double>(bgfxStats->cpuTimeFrame) * 1000.0
                / static_cast<double>(bgfxStats->cpuTimerFreq)
            : 0.0;
        const double gpuFrameMs =
            (bgfxStats->gpuTimerFreq > 0 && bgfxStats->gpuTimeEnd >= bgfxStats->gpuTimeBegin)
            ? static_cast<double>(bgfxStats->gpuTimeEnd - bgfxStats->gpuTimeBegin) * 1000.0
                / static_cast<double>(bgfxStats->gpuTimerFreq)
            : 0.0;
        bgfx::dbgTextPrintf(
            0,
            7,
            0x0f,
            "bgfx: cpu %.2f ms  gpu %.2f ms  draw %u  tri %u",
            cpuFrameMs,
            gpuFrameMs,
            bgfxStats->numDraw,
            bgfxStats->numPrims[bgfx::Topology::TriList]);
    }
    else
    {
        bgfx::dbgTextPrintf(0, 7, 0x0f, "bgfx: stats unavailable");
    }

    const std::string cameraLine = fmt::format(
        "Camera: ({:.1f}, {:.1f}, {:.1f})",
        frameDebugData.cameraPosition.x,
        frameDebugData.cameraPosition.y,
        frameDebugData.cameraPosition.z);
    bgfx::dbgTextPrintf(0, 8, 0x0f, "%s", cameraLine.c_str());

    if (frameDebugData.hasTarget)
    {
        const std::string targetLine = fmt::format(
            "Target block: ({}, {}, {})",
            frameDebugData.targetBlock.x,
            frameDebugData.targetBlock.y,
            frameDebugData.targetBlock.z);
        bgfx::dbgTextPrintf(0, 9, 0x0f, "%s", targetLine.c_str());
    }
    else
    {
        bgfx::dbgTextPrintf(0, 9, 0x0f, "Target block: none");
    }

    if (inventoryUiSolidProgramHandle_ == UINT16_MAX)
    {
        detail::drawHotbarHud(hotbarRow, frameDebugData);
        detail::drawHotbarKeyHintsRow(hotbarKeyRow, width_, height_, textWidthForHud, textHeight, hotbarRow);
    }
    detail::drawHotbarStackCounts(hotbarRow, frameDebugData, width_, height_, textWidthForHud, textHeight, hotbarRow);
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX
        || fullHeartTextureHandle_ == UINT16_MAX
        || halfHeartTextureHandle_ == UINT16_MAX
        || emptyHeartTextureHandle_ == UINT16_MAX)
    {
        detail::drawHealthHud(healthRow, frameDebugData);
    }
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX && textHeight >= 10)
    {
        detail::drawBagHud(
            bagTitleRow,
            bagSepRow,
            bagRow0,
            bagRow1,
            bagRow2,
            frameDebugData);
    }
    bgfx::dbgTextPrintf(
        0,
        controlsRow0,
        0x0e,
        "Controls: WASD move, Shift sneak, Ctrl sprint, Space jump, mouse look");
    bgfx::dbgTextPrintf(
        0,
        controlsRow1,
        0x0e,
        "LMB mine, RMB place, 1-9 select hotbar, Tab capture, Esc pause menu");

    bgfx::frame();
}

} // namespace vibecraft::render
