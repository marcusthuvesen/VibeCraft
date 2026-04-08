#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec4.hpp>
#include <string>

#include "debugdraw.h"
#include "vibecraft/core/Logger.hpp"

namespace vibecraft::render
{
namespace
{
[[nodiscard]] float hashUnitFloat(std::uint32_t seed)
{
    seed ^= 0x9e3779b9u;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    seed ^= seed >> 16;
    return static_cast<float>(seed) * (1.0f / 4294967295.0f);
}

[[nodiscard]] glm::vec3 celestialBillboardNormal(const glm::vec3& cameraPosition, const glm::vec3& center)
{
    const glm::vec3 toCamera = cameraPosition - center;
    if (glm::dot(toCamera, toCamera) <= 1.0e-6f)
    {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }

    return glm::normalize(toCamera);
}

void drawCelestialQuad(
    DebugDrawEncoder& debugDrawEncoder,
    const glm::vec3& cameraPosition,
    const glm::vec3& center,
    const float size,
    const glm::vec3& color,
    const float alpha = 1.0f)
{
    const glm::vec3 normal = celestialBillboardNormal(cameraPosition, center);
    debugDrawEncoder.setColor(detail::packAbgr8(color, alpha));
    debugDrawEncoder.drawQuad(
        bx::Vec3(normal.x, normal.y, normal.z),
        bx::Vec3(center.x, center.y, center.z),
        size);
}

void drawDirectionalQuad(
    DebugDrawEncoder& debugDrawEncoder,
    const CameraFrameData& cameraFrameData,
    const glm::vec3& direction,
    const float distance,
    const float size,
    const glm::vec3& color,
    const float alpha = 1.0f)
{
    const glm::vec3 center = cameraFrameData.position + glm::normalize(direction) * distance;
    drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, center, size, color, alpha);
}

[[nodiscard]] bool submitTexturedCelestialSprite(
    const CameraFrameData& cameraFrameData,
    const glm::vec3& direction,
    const float distance,
    const float size,
    const TextureUvRect& uvRect,
    const glm::vec3& tint,
    const float alpha,
    const std::uint16_t textureHandle,
    const std::uint16_t samplerHandle,
    const std::uint16_t programHandle)
{
    if (textureHandle == UINT16_MAX || samplerHandle == UINT16_MAX || programHandle == UINT16_MAX)
    {
        return false;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4
        || bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return false;
    }

    const glm::vec3 center = cameraFrameData.position + glm::normalize(direction) * distance;
    glm::vec3 toCamera = cameraFrameData.position - center;
    if (glm::dot(toCamera, toCamera) <= 1.0e-6f)
    {
        toCamera = -cameraFrameData.forward;
    }
    toCamera = glm::normalize(toCamera);

    glm::vec3 right = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), toCamera);
    if (glm::dot(right, right) <= 1.0e-6f)
    {
        right = glm::cross(cameraFrameData.up, toCamera);
    }
    if (glm::dot(right, right) <= 1.0e-6f)
    {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    right = glm::normalize(right) * (size * 0.5f);
    glm::vec3 up = glm::cross(toCamera, right);
    if (glm::dot(up, up) <= 1.0e-6f)
    {
        up = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    up = glm::normalize(up) * (size * 0.5f);

    const std::uint32_t abgr = detail::packAbgr8(tint, alpha);
    detail::ChunkVertex vertices[4] = {
        detail::ChunkVertex{
            .x = center.x - right.x - up.x,
            .y = center.y - right.y - up.y,
            .z = center.z - right.z - up.z,
            .nx = 0.0f,
            .ny = 1.0f,
            .nz = 0.0f,
            .u = uvRect.minU,
            .v = uvRect.maxV,
            .abgr = abgr},
        detail::ChunkVertex{
            .x = center.x + right.x - up.x,
            .y = center.y + right.y - up.y,
            .z = center.z + right.z - up.z,
            .nx = 0.0f,
            .ny = 1.0f,
            .nz = 0.0f,
            .u = uvRect.maxU,
            .v = uvRect.maxV,
            .abgr = abgr},
        detail::ChunkVertex{
            .x = center.x + right.x + up.x,
            .y = center.y + right.y + up.y,
            .z = center.z + right.z + up.z,
            .nx = 0.0f,
            .ny = 1.0f,
            .nz = 0.0f,
            .u = uvRect.maxU,
            .v = uvRect.minV,
            .abgr = abgr},
        detail::ChunkVertex{
            .x = center.x - right.x + up.x,
            .y = center.y - right.y + up.y,
            .z = center.z - right.z + up.z,
            .nx = 0.0f,
            .ny = 1.0f,
            .nz = 0.0f,
            .u = uvRect.minU,
            .v = uvRect.minV,
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
    bgfx::setTexture(0, detail::toUniformHandle(samplerHandle), detail::toTextureHandle(textureHandle));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
        | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
    bgfx::submit(detail::kMainView, detail::toProgramHandle(programHandle));
    return true;
}

void drawStarField(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    const float starVisibility = std::clamp(1.02f - cameraFrameData.sunVisibility * 1.7f, 0.0f, 1.0f);
    if (starVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr int kStarCount = 140;
    constexpr float kStarDistance = 230.0f;

    for (int starIndex = 0; starIndex < kStarCount; ++starIndex)
    {
        const std::uint32_t seed = static_cast<std::uint32_t>(starIndex) * 747796405u + 2891336453u;
        const float azimuth = hashUnitFloat(seed) * kTwoPi;
        const float elevation = glm::mix(0.08f, 0.995f, hashUnitFloat(seed ^ 0xa511e9b3u));
        const float radial = std::sqrt(std::max(0.0f, 1.0f - elevation * elevation));
        const glm::vec3 direction(
            std::cos(azimuth) * radial,
            elevation,
            std::sin(azimuth) * radial);
        const glm::vec3 starTint = glm::mix(glm::vec3(0.94f, 0.96f, 1.0f), glm::vec3(1.0f), hashUnitFloat(seed ^ 0x68bc21ebu) * 0.35f);
        const float prominence = hashUnitFloat(seed ^ 0x94d049bbu);
        const float twinkle =
            0.84f
            + 0.28f
                * (0.5f
                   + 0.5f
                       * std::sin(
                           cameraFrameData.weatherTimeSeconds * (0.85f + prominence * 1.8f)
                           + static_cast<float>(starIndex) * 12.73f));
        const float alpha = starVisibility * (0.14f + prominence * prominence * 0.52f) * twinkle;
        const float size = 0.40f + prominence * prominence * 1.05f;
        drawDirectionalQuad(debugDrawEncoder, cameraFrameData, direction, kStarDistance, size, starTint, alpha);
    }
}

void drawSun(
    DebugDrawEncoder& debugDrawEncoder,
    const CameraFrameData& cameraFrameData,
    const std::uint16_t sunTextureHandle,
    const std::uint16_t uiSamplerHandle,
    const std::uint16_t uiProgramHandle)
{
    if (cameraFrameData.sunVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kSunDistance = 236.0f;
    constexpr float kSunRadius = 18.5f;
    const glm::vec3 discTint = glm::mix(glm::vec3(1.0f, 0.95f, 0.62f), cameraFrameData.sunLightTint, 0.55f);

    if (submitTexturedCelestialSprite(
            cameraFrameData,
            cameraFrameData.sunDirection,
            kSunDistance,
            kSunRadius * 1.15f,
            TextureUvRect{},
            discTint,
            glm::clamp(cameraFrameData.sunVisibility, 0.0f, 1.0f),
            sunTextureHandle,
            uiSamplerHandle,
            uiProgramHandle))
    {
        return;
    }

    drawDirectionalQuad(debugDrawEncoder, cameraFrameData, cameraFrameData.sunDirection, kSunDistance, kSunRadius * 1.15f, discTint);
}

void drawMoon(
    DebugDrawEncoder& debugDrawEncoder,
    const CameraFrameData& cameraFrameData,
    const std::uint16_t moonPhasesTextureHandle,
    const std::uint16_t uiSamplerHandle,
    const std::uint16_t uiProgramHandle)
{
    if (cameraFrameData.moonVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kMoonDistance = 234.0f;
    constexpr float kMoonRadius = 17.0f;
    const glm::vec3 moonDirection = glm::normalize(cameraFrameData.moonDirection);
    const glm::vec3 moonCenter = cameraFrameData.position + moonDirection * kMoonDistance;
    glm::vec3 lightAxis =
        cameraFrameData.sunDirection - moonDirection * glm::dot(cameraFrameData.sunDirection, moonDirection);
    if (glm::dot(lightAxis, lightAxis) <= 1.0e-6f)
    {
        lightAxis = glm::cross(moonDirection, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (glm::dot(lightAxis, lightAxis) <= 1.0e-6f)
    {
        lightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    lightAxis = glm::normalize(lightAxis);
    const float phase = std::clamp(
        glm::dot(-moonDirection, glm::normalize(cameraFrameData.sunDirection)) * 0.5f + 0.5f,
        0.0f,
        1.0f);
    const glm::vec3 moonLitTint = glm::mix(glm::vec3(0.86f, 0.87f, 0.92f), cameraFrameData.moonLightTint, 0.30f);

    // Minecraft moon phases are packed as a 4x2 texture grid.
    const int phaseIndex = std::clamp(static_cast<int>(std::floor((1.0f - phase) * 8.0f)), 0, 7);
    constexpr float kMoonPhaseUSize = 1.0f / 4.0f;
    constexpr float kMoonPhaseVSize = 1.0f / 2.0f;
    const float minU = static_cast<float>(phaseIndex % 4) * kMoonPhaseUSize;
    const float minV = static_cast<float>(phaseIndex / 4) * kMoonPhaseVSize;
    const TextureUvRect moonUv{
        .minU = minU,
        .maxU = minU + kMoonPhaseUSize,
        .minV = minV,
        .maxV = minV + kMoonPhaseVSize,
    };
    if (submitTexturedCelestialSprite(
            cameraFrameData,
            moonDirection,
            kMoonDistance,
            kMoonRadius * 1.06f,
            moonUv,
            moonLitTint,
            glm::clamp(cameraFrameData.moonVisibility, 0.0f, 1.0f),
            moonPhasesTextureHandle,
            uiSamplerHandle,
            uiProgramHandle))
    {
        return;
    }

    const glm::vec3 moonShadowTint(0.10f, 0.11f, 0.15f);

    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        moonCenter,
        kMoonRadius * 1.06f,
        moonLitTint,
        0.96f);

    // Fallback moon phase when textured moon assets are unavailable.
    const float shadowOffset = (1.0f - phase) * (kMoonRadius * 1.1f);
    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        moonCenter - lightAxis * shadowOffset,
        kMoonRadius * 1.02f,
        moonShadowTint,
        0.90f);
}
}  // namespace


void Renderer::renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData)
{
    if (!initialized_ || width_ == 0 || height_ == 0)
    {
        return;
    }

    const bool shouldEnableDbgText = dbgTextForceOn_
        || (!dbgTextForceOff_
            && (frameDebugData.mainMenuActive || frameDebugData.pauseMenuActive
                || frameDebugData.craftingMenuActive || frameDebugData.chatOpen));
    if (shouldEnableDbgText != dbgTextCurrentlyEnabled_)
    {
        bgfx::setDebug(shouldEnableDbgText ? BGFX_DEBUG_TEXT : 0U);
        dbgTextCurrentlyEnabled_ = shouldEnableDbgText;
    }

    if (frameDebugData.mainMenuActive)
    {
        bgfx::setViewClear(detail::kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, detail::kMainMenuClearColor, 1.0f, 0);
        bgfx::setViewRect(detail::kMainView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
        bgfx::setViewRect(detail::kUiView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
        bgfx::touch(detail::kMainView);
        bgfx::touch(detail::kUiView);
        drawMainMenuBackground();
        bgfx::dbgTextClear();
        const bgfx::Stats* const menuStats = bgfx::getStats();
        const std::uint16_t rawMenuTextHeight =
            menuStats != nullptr && menuStats->textHeight > 0 ? menuStats->textHeight : 30;
        const std::uint16_t rawMenuTextWidth =
            menuStats != nullptr && menuStats->textWidth > 0 ? menuStats->textWidth : 100;
        // Keep main-menu dbg-text layout on the exact same grid returned by bgfx stats.
        // Mixing app-scaled grid metrics with bgfx raw stats causes horizontal drift.
        const std::uint16_t menuTextWidth = rawMenuTextWidth;
        const std::uint16_t menuTextHeight = rawMenuTextHeight;
        const std::uint32_t menuWindowWidth = width_;
        const std::uint32_t menuWindowHeight = height_;
        const int titleMenuRowBias = detail::mainMenuLogoReservedDbgRows(
            menuWindowWidth, menuWindowHeight, menuTextHeight, logoWidthPx_, logoHeightPx_);
        drawMainMenuChrome(frameDebugData, menuTextWidth, menuTextHeight, titleMenuRowBias);
        drawMainMenuLogo();
        detail::drawMainMenuOverlay(frameDebugData, menuTextWidth, menuTextHeight, titleMenuRowBias);
        bgfx::frame();
        return;
    }

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
        cameraFrameData.skyTint * 0.31f
            + cameraFrameData.horizonTint * 0.13f
            + cameraFrameData.sunLightTint * (0.06f * cameraFrameData.sunVisibility)
            + cameraFrameData.moonLightTint * (0.03f * cameraFrameData.moonVisibility),
        glm::vec3(0.03f),
        glm::vec3(0.56f));
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

        // Keep rendering local, but leave enough horizontal headroom for a Minecraft-style distance fog ramp
        // instead of hard-cutting terrain just beyond the player's view.
        constexpr float kChunkHorizontalRenderDistance = 160.0f;
        constexpr float kBelowCameraDistanceWeight = 2.3f;
        constexpr float kAboveCameraDistanceWeight = 1.0f;
        // Hard vertical budget: sections entirely outside this window around the camera are skipped.
        // These values cover full cave depth below and tall mountains above while culling invisible sections.
        constexpr float kVerticalRenderBelowBlocks = 96.0f;
        constexpr float kVerticalRenderAboveBlocks = 160.0f;
        const float chunkMaxDrawDistance = std::min(cameraFrameData.farClip * 1.08f, kChunkHorizontalRenderDistance);
        const float chunkMaxDrawDistanceSq = chunkMaxDrawDistance * chunkMaxDrawDistance;
        const float fogStart = chunkMaxDrawDistance * 0.55f;
        const float fogEnd = chunkMaxDrawDistance * 0.92f;
        std::array<glm::vec4, FrameDebugData::kMaxTorchLightEmitters> torchLights{};
        for (glm::vec4& torchLight : torchLights)
        {
            torchLight = glm::vec4(0.0f, -10000.0f, 0.0f, 0.0f);
        }
        const std::size_t torchEmitterCount = std::min(
            frameDebugData.torchLightEmitters.size(),
            static_cast<std::size_t>(FrameDebugData::kMaxTorchLightEmitters));
        for (std::size_t torchIndex = 0; torchIndex < torchEmitterCount; ++torchIndex)
        {
            torchLights[torchIndex] = glm::vec4(frameDebugData.torchLightEmitters[torchIndex], 1.0f);
        }
        detail::setVec4Uniform(chunkCameraPosUniformHandle_, cameraFrameData.position, 1.0f);
        detail::setVec4Uniform(
            chunkFogUniformHandle_, glm::vec4(fogStart, fogEnd, chunkMaxDrawDistance, 0.0f));

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

            if (detail::isAabbOutsideVerticalRange(
                    sceneMesh.boundsMin,
                    sceneMesh.boundsMax,
                    cameraFrameData.position.y,
                    kVerticalRenderBelowBlocks,
                    kVerticalRenderAboveBlocks))
            {
                continue;
            }

            if (detail::distanceSqCameraToAabbXZ(cameraFrameData.position, sceneMesh.boundsMin, sceneMesh.boundsMax)
                > chunkMaxDrawDistanceSq)
            {
                continue;
            }
            if (detail::distanceSqCameraToAabbDownWeighted(
                    cameraFrameData.position,
                    sceneMesh.boundsMin,
                    sceneMesh.boundsMax,
                    kBelowCameraDistanceWeight,
                    kAboveCameraDistanceWeight)
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
            // .w = raw visibility (rgb already includes tint * visibility) for sky bounce / night blend in fs_chunk.
            detail::setVec4Uniform(
                chunkSunLightColorUniformHandle_, sunLightColor, cameraFrameData.sunVisibility);
            detail::setVec4Uniform(chunkMoonDirectionUniformHandle_, cameraFrameData.moonDirection, 0.0f);
            detail::setVec4Uniform(
                chunkMoonLightColorUniformHandle_, moonLightColor, cameraFrameData.moonVisibility);
            detail::setVec4Uniform(chunkAmbientLightUniformHandle_, ambientLight, 0.0f);
            detail::setVec4Uniform(
                chunkAnimUniformHandle_,
                glm::vec3(
                    cameraFrameData.weatherTimeSeconds,
                    cameraFrameData.rainIntensity,
                    cameraFrameData.weatherWindSpeed),
                0.0f);
            detail::setVec4Uniform(
                chunkBiomeHazeUniformHandle_,
                cameraFrameData.terrainHazeColor,
                cameraFrameData.terrainHazeStrength);
            detail::setVec4Uniform(
                chunkBiomeGradeUniformHandle_,
                cameraFrameData.terrainBounceTint,
                cameraFrameData.terrainSaturation);
            if (chunkTorchLightsUniformHandle_ != UINT16_MAX)
            {
                bgfx::setUniform(
                    detail::toUniformHandle(chunkTorchLightsUniformHandle_),
                    torchLights.data(),
                    static_cast<std::uint16_t>(torchLights.size()));
            }
            detail::setVec4Uniform(
                chunkTorchParamsUniformHandle_,
                glm::vec4(static_cast<float>(torchEmitterCount), 8.0f, 0.72f, 0.0f));
            bgfx::setState(detail::kChunkRenderState);
            bgfx::submit(detail::kMainView, detail::toProgramHandle(chunkProgramHandle_));
        }
    }

    drawWorldPickupSprites(frameDebugData, cameraFrameData);
    drawWorldProjectileSprites(frameDebugData, cameraFrameData);

    DebugDrawEncoder debugDrawEncoder;
    debugDrawEncoder.begin(detail::kMainView);
    detail::drawWeatherClouds(debugDrawEncoder, cameraFrameData);
    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(false);
    drawStarField(debugDrawEncoder, cameraFrameData);
    drawSun(
        debugDrawEncoder,
        cameraFrameData,
        skySunTextureHandle_,
        inventoryUiSamplerHandle_,
        inventoryUiProgramHandle_);
    drawMoon(
        debugDrawEncoder,
        cameraFrameData,
        skyMoonPhasesTextureHandle_,
        inventoryUiSamplerHandle_,
        inventoryUiProgramHandle_);
    debugDrawEncoder.pop();
    detail::drawWeatherRain(debugDrawEncoder, cameraFrameData);
    // Celestial pass uses reversed depth-test for sky quads; restore normal depth for world overlays.
    debugDrawEncoder.setDepthTestLess(true);
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
    const std::uint16_t rawTextHeight =
        bgfxStats != nullptr && bgfxStats->textHeight > 0 ? bgfxStats->textHeight : 30;
    const std::uint16_t rawTextWidthForHud =
        bgfxStats != nullptr && bgfxStats->textWidth > 0 ? bgfxStats->textWidth : 100;
    const std::uint16_t menuTextWidth =
        frameDebugData.uiMenuTextWidth > 0 ? frameDebugData.uiMenuTextWidth : rawTextWidthForHud;
    const std::uint16_t menuTextHeight =
        frameDebugData.uiMenuTextHeight > 0 ? frameDebugData.uiMenuTextHeight : rawTextHeight;

    if (frameDebugData.pauseMenuActive)
    {
        drawPauseMenuChrome(frameDebugData, menuTextWidth, menuTextHeight);
        detail::drawPauseMenuOverlay(frameDebugData, menuTextWidth, menuTextHeight);
        const std::uint16_t pauseHealthRow = menuTextHeight > 2 ? static_cast<std::uint16_t>(menuTextHeight - 3) : 0;
        detail::drawHealthHud(pauseHealthRow, frameDebugData);
        bgfx::frame();
        return;
    }

    if (frameDebugData.craftingMenuActive)
    {
        drawCraftingOverlay(frameDebugData);
        const std::string craftingTitle =
            frameDebugData.craftingTitle.empty()
            ? (frameDebugData.craftingUsesWorkbench ? "Crafting Table" : "Inventory Crafting")
            : frameDebugData.craftingTitle;
        bgfx::dbgTextPrintf(0, 1, 0x0f, "%s", craftingTitle.c_str());
        bgfx::dbgTextPrintf(
            0,
            3,
            0x0a,
            "%s",
            frameDebugData.craftingHint.empty()
                ? "Left-click to move stacks. Press E or Esc to close."
                : frameDebugData.craftingHint.c_str());
        bgfx::frame();
        return;
    }

    drawCrosshairOverlay();
    drawHeldItemOverlay(frameDebugData);

    const std::uint16_t textHeight = rawTextHeight;
    const std::uint16_t textWidthForHud = rawTextWidthForHud;
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
    drawSurvivalStatusHud(frameDebugData);

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

    {
        using PerfClock = std::chrono::steady_clock;
        const auto now = PerfClock::now();
        struct RenderPerfAccumulator
        {
            PerfClock::time_point windowStart = PerfClock::now();
            int frameCount = 0;
            double sumCpuMs = 0.0;
            double sumGpuMs = 0.0;
            double sumDrawCalls = 0.0;
            double sumTriangles = 0.0;
            double sumVisibleChunks = 0.0;
            double sumSceneMeshes = 0.0;
        };
        static RenderPerfAccumulator perf{};

        const double cpuFrameMs = bgfxStats != nullptr && bgfxStats->cpuTimerFreq > 0
            ? static_cast<double>(bgfxStats->cpuTimeFrame) * 1000.0
                / static_cast<double>(bgfxStats->cpuTimerFreq)
            : 0.0;
        const double gpuFrameMs =
            (bgfxStats != nullptr && bgfxStats->gpuTimerFreq > 0 && bgfxStats->gpuTimeEnd >= bgfxStats->gpuTimeBegin)
            ? static_cast<double>(bgfxStats->gpuTimeEnd - bgfxStats->gpuTimeBegin) * 1000.0
                / static_cast<double>(bgfxStats->gpuTimerFreq)
            : 0.0;
        const std::uint32_t drawCalls = bgfxStats != nullptr ? bgfxStats->numDraw : 0;
        const std::uint32_t triangles =
            bgfxStats != nullptr ? bgfxStats->numPrims[bgfx::Topology::TriList] : 0;

        ++perf.frameCount;
        perf.sumCpuMs += cpuFrameMs;
        perf.sumGpuMs += gpuFrameMs;
        perf.sumDrawCalls += static_cast<double>(drawCalls);
        perf.sumTriangles += static_cast<double>(triangles);
        perf.sumVisibleChunks += static_cast<double>(visibleChunkCount);
        perf.sumSceneMeshes += static_cast<double>(sceneMeshes_.size());
        if (std::chrono::duration<double>(now - perf.windowStart).count() >= 1.0 && perf.frameCount > 0)
        {
            const double frameCount = static_cast<double>(perf.frameCount);
            core::logInfo(fmt::format(
                "[perf-render] cpu_ms={:.2f} gpu_ms={:.2f} draw_calls={:.0f} tris={:.0f} "
                "visible_chunk_sections={:.1f} scene_chunk_sections={:.1f}",
                perf.sumCpuMs / frameCount,
                perf.sumGpuMs / frameCount,
                perf.sumDrawCalls / frameCount,
                perf.sumTriangles / frameCount,
                perf.sumVisibleChunks / frameCount,
                perf.sumSceneMeshes / frameCount));
            perf.windowStart = now;
            perf.frameCount = 0;
            perf.sumCpuMs = 0.0;
            perf.sumGpuMs = 0.0;
            perf.sumDrawCalls = 0.0;
            perf.sumTriangles = 0.0;
            perf.sumVisibleChunks = 0.0;
            perf.sumSceneMeshes = 0.0;
        }
    }

    if (frameDebugData.showWorldOriginGuides)
    {
        detail::drawCoordinateOverlay(frameDebugData);
    }
    else
    {
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
    }
    const std::uint16_t selectedLabelRow = 10;
    if (!frameDebugData.selectedHotbarLabel.empty())
    {
        bgfx::dbgTextPrintf(0, selectedLabelRow, 0x0f, "Selected: %s", frameDebugData.selectedHotbarLabel.c_str());
    }
    if (!frameDebugData.selectedHotbarActionHint.empty())
    {
        bgfx::dbgTextPrintf(0, selectedLabelRow + 1, 0x0e, "%s", frameDebugData.selectedHotbarActionHint.c_str());
    }
    if (!frameDebugData.survivalTipLine.empty())
    {
        bgfx::dbgTextPrintf(0, selectedLabelRow + 2, 0x0b, "%s", frameDebugData.survivalTipLine.c_str());
    }

    if (inventoryUiSolidProgramHandle_ == UINT16_MAX)
    {
        detail::drawHotbarHud(hotbarRow, frameDebugData);
        detail::drawHotbarKeyHintsRow(hotbarKeyRow, width_, height_, textWidthForHud, textHeight, hotbarRow);
    }
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX)
    {
        detail::drawHotbarStackCounts(hotbarRow, frameDebugData, width_, height_, textWidthForHud, textHeight, hotbarRow);
    }
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
    detail::drawChatOverlay(frameDebugData, textWidthForHud, textHeight);
    bgfx::dbgTextPrintf(
        0,
        controlsRow0,
        0x0e,
        "Controls: WASD move, Shift sneak, Ctrl sprint, Space jump, T chat, E inventory, mouse look");
    bgfx::dbgTextPrintf(
        0,
        controlsRow1,
        0x0e,
        "LMB mine, RMB place, / command chat, 1-9 select hotbar, Tab capture, Esc pause menu");

    bgfx::frame();
}

} // namespace vibecraft::render
