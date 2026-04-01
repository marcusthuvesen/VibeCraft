#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <string>

#include "debugdraw.h"
#include "stb_image.h"

namespace vibecraft::render
{
namespace
{
struct GlassShieldProfile
{
    glm::vec3 tint{0.36f, 0.84f, 1.0f};
};

[[nodiscard]] const GlassShieldProfile& glassShieldProfile()
{
    static const GlassShieldProfile kProfile = []()
    {
        GlassShieldProfile profile{};
        const std::filesystem::path path = detail::runtimeAssetPath("textures/effects/oxygen_glass.png");
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            if (pixels != nullptr)
            {
                stbi_image_free(pixels);
            }
            return profile;
        }

        double sumR = 0.0;
        double sumG = 0.0;
        double sumB = 0.0;
        double sumA = 0.0;
        std::uint64_t sampleCount = 0;
        const std::uint64_t total = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
        for (std::uint64_t i = 0; i < total; ++i)
        {
            const std::size_t idx = static_cast<std::size_t>(i) * 4U;
            const double a = static_cast<double>(pixels[idx + 3]) / 255.0;
            if (a <= 1e-5)
            {
                continue;
            }
            sumR += static_cast<double>(pixels[idx + 0]) * a;
            sumG += static_cast<double>(pixels[idx + 1]) * a;
            sumB += static_cast<double>(pixels[idx + 2]) * a;
            sumA += a;
            ++sampleCount;
        }
        stbi_image_free(pixels);
        if (sampleCount == 0 || sumA <= 1e-6)
        {
            return profile;
        }

        profile.tint = glm::vec3(
            static_cast<float>(sumR / (sumA * 255.0)),
            static_cast<float>(sumG / (sumA * 255.0)),
            static_cast<float>(sumB / (sumA * 255.0)));
        return profile;
    }();
    return kProfile;
}

[[nodiscard]] std::string oxygenBarString(const FrameDebugData& frameDebugData)
{
    constexpr int kBarSegments = 16;
    const float clampedMaxOxygen = std::max(0.0f, frameDebugData.maxOxygen);
    const float clampedOxygen = std::clamp(frameDebugData.oxygen, 0.0f, clampedMaxOxygen);
    const float fillRatio = clampedMaxOxygen > 0.0f ? clampedOxygen / clampedMaxOxygen : 0.0f;
    const int filledSegments = std::clamp(static_cast<int>(std::round(fillRatio * kBarSegments)), 0, kBarSegments);

    std::string bar = "[";
    for (int i = 0; i < kBarSegments; ++i)
    {
        bar += i < filledSegments ? '=' : '.';
    }
    bar += "]";

    return fmt::format(
        "Oxygen {} {:>3.0f}%  {}",
        bar,
        fillRatio * 100.0f,
        frameDebugData.oxygenInsideSafeZone ? "safe zone" : "exposed");
}

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

void drawThinAtmosphereRing(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr int kPanelCount = 14;
    constexpr float kRingDistance = 176.0f;
    constexpr float kRingHeightOffset = -18.0f;
    constexpr float kRingSize = 150.0f;

    const glm::vec3 horizonBase = glm::mix(cameraFrameData.horizonTint, cameraFrameData.skyTint, 0.30f);
    const glm::vec3 sunGlowTint = glm::mix(horizonBase, cameraFrameData.sunLightTint, 0.38f);
    const glm::vec3 sunHorizDir = glm::normalize(glm::vec3(
        cameraFrameData.sunDirection.x,
        cameraFrameData.sunDirection.y * 0.28f,
        cameraFrameData.sunDirection.z));

    for (int panelIndex = 0; panelIndex < kPanelCount; ++panelIndex)
    {
        const float angle = (static_cast<float>(panelIndex) / static_cast<float>(kPanelCount)) * kTwoPi;
        const glm::vec3 ringDirection = glm::normalize(glm::vec3(std::cos(angle), 0.06f, std::sin(angle)));
        const float sunAlignment =
            std::clamp(glm::dot(ringDirection, sunHorizDir) * 0.5f + 0.5f, 0.0f, 1.0f);
        const glm::vec3 tint = glm::mix(horizonBase, sunGlowTint, sunAlignment * 0.55f);
        const float alpha = 0.08f + sunAlignment * 0.09f + cameraFrameData.sunVisibility * 0.06f;
        const glm::vec3 center =
            cameraFrameData.position + ringDirection * kRingDistance + glm::vec3(0.0f, kRingHeightOffset, 0.0f);
        drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, center, kRingSize, tint, alpha);
    }
}

void drawStarField(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    const float starVisibility = std::clamp(1.08f - cameraFrameData.sunVisibility * 1.55f, 0.0f, 1.0f);
    if (starVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr int kStarCount = 220;
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
        const glm::vec3 coolTint(0.64f, 0.76f, 1.0f);
        const glm::vec3 warmTint(1.0f, 0.88f, 0.76f);
        const glm::vec3 starTint = glm::mix(
            coolTint,
            warmTint,
            hashUnitFloat(seed ^ 0x68bc21ebu) * 0.45f + hashUnitFloat(seed ^ 0x02e5be93u) * 0.25f);
        const float prominence = hashUnitFloat(seed ^ 0x94d049bbu);
        const float twinkle =
            0.72f
            + 0.28f
                * (0.5f
                   + 0.5f
                       * std::sin(
                           cameraFrameData.weatherTimeSeconds * (0.85f + prominence * 1.8f)
                           + static_cast<float>(starIndex) * 12.73f));
        const float alpha = starVisibility * (0.22f + prominence * prominence * 0.78f) * twinkle;
        const float size = 0.45f + prominence * prominence * 1.65f;
        drawDirectionalQuad(debugDrawEncoder, cameraFrameData, direction, kStarDistance, size, starTint, alpha);
    }
}

void drawSun(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.sunVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kSunDistance = 240.0f;
    constexpr float kSunRadius = 17.0f;
    const glm::vec3 haloTint = glm::mix(cameraFrameData.sunLightTint, cameraFrameData.horizonTint, 0.26f);
    const glm::vec3 outerHaloTint = glm::mix(haloTint, cameraFrameData.skyTint, 0.34f);
    const glm::vec3 discTint = glm::mix(cameraFrameData.horizonTint, cameraFrameData.sunLightTint, 0.82f);

    drawDirectionalQuad(
        debugDrawEncoder,
        cameraFrameData,
        cameraFrameData.sunDirection,
        kSunDistance,
        kSunRadius * 5.8f,
        outerHaloTint,
        0.12f + cameraFrameData.sunVisibility * 0.10f);
    drawDirectionalQuad(
        debugDrawEncoder,
        cameraFrameData,
        cameraFrameData.sunDirection,
        kSunDistance,
        kSunRadius * 3.8f,
        haloTint,
        0.22f + cameraFrameData.sunVisibility * 0.16f);
    drawDirectionalQuad(
        debugDrawEncoder,
        cameraFrameData,
        cameraFrameData.sunDirection,
        kSunDistance,
        kSunRadius * 2.2f,
        discTint);
}

void drawDistantEarth(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.moonVisibility <= 0.01f)
    {
        return;
    }

    constexpr float kEarthDistance = 232.0f;
    constexpr float kEarthRadius = 12.0f;
    const glm::vec3 earthDirection = glm::normalize(cameraFrameData.moonDirection);
    const glm::vec3 earthCenter = cameraFrameData.position + earthDirection * kEarthDistance;
    glm::vec3 lightAxis =
        cameraFrameData.sunDirection - earthDirection * glm::dot(cameraFrameData.sunDirection, earthDirection);
    if (glm::dot(lightAxis, lightAxis) <= 1.0e-6f)
    {
        lightAxis = glm::cross(earthDirection, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (glm::dot(lightAxis, lightAxis) <= 1.0e-6f)
    {
        lightAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    lightAxis = glm::normalize(lightAxis);
    glm::vec3 polarAxis = glm::cross(earthDirection, lightAxis);
    if (glm::dot(polarAxis, polarAxis) <= 1.0e-6f)
    {
        polarAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        polarAxis = glm::normalize(polarAxis);
    }

    const float phase = std::clamp(
        glm::dot(-earthDirection, glm::normalize(cameraFrameData.sunDirection)) * 0.5f + 0.5f,
        0.0f,
        1.0f);
    const glm::vec3 oceanTint = glm::mix(glm::vec3(0.10f, 0.18f, 0.30f), glm::vec3(0.20f, 0.48f, 0.82f), phase);
    const glm::vec3 cloudTint = glm::mix(glm::vec3(0.70f, 0.78f, 0.86f), glm::vec3(0.96f, 0.98f, 1.0f), phase);
    const glm::vec3 atmosphereTint = glm::mix(oceanTint, cloudTint, 0.58f);
    const glm::vec3 landTint = glm::mix(glm::vec3(0.16f, 0.28f, 0.12f), glm::vec3(0.30f, 0.44f, 0.18f), phase);

    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        earthCenter,
        kEarthRadius * 3.2f,
        atmosphereTint,
        0.18f + phase * 0.12f);
    drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, earthCenter, kEarthRadius * 2.15f, oceanTint);
    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        earthCenter - lightAxis * (kEarthRadius * 0.18f) + polarAxis * (kEarthRadius * 0.08f),
        kEarthRadius * 1.02f,
        landTint,
        0.62f);
    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        earthCenter + lightAxis * (kEarthRadius * 0.26f) + polarAxis * (kEarthRadius * 0.12f),
        kEarthRadius * 1.24f,
        cloudTint,
        0.72f);
    drawCelestialQuad(
        debugDrawEncoder,
        cameraFrameData.position,
        earthCenter - lightAxis * (kEarthRadius * 0.44f),
        kEarthRadius * 1.95f,
        glm::vec3(0.01f, 0.02f, 0.04f),
        0.22f + (1.0f - phase) * 0.48f);
}
}  // namespace


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
        const std::uint16_t rawMenuTextHeight =
            menuStats != nullptr && menuStats->textHeight > 0 ? menuStats->textHeight : 30;
        const std::uint16_t rawMenuTextWidth =
            menuStats != nullptr && menuStats->textWidth > 0 ? menuStats->textWidth : 100;
        const std::uint16_t menuTextWidth =
            frameDebugData.uiMenuTextWidth > 0 ? frameDebugData.uiMenuTextWidth : rawMenuTextWidth;
        const std::uint16_t menuTextHeight =
            frameDebugData.uiMenuTextHeight > 0 ? frameDebugData.uiMenuTextHeight : rawMenuTextHeight;
        const std::uint32_t menuWindowWidth =
            frameDebugData.uiMenuWindowWidth > 0 ? frameDebugData.uiMenuWindowWidth : width_;
        const std::uint32_t menuWindowHeight =
            frameDebugData.uiMenuWindowHeight > 0 ? frameDebugData.uiMenuWindowHeight : height_;
        const int titleMenuRowBias = detail::mainMenuLogoReservedDbgRows(
            menuWindowWidth, menuWindowHeight, menuTextHeight, logoWidthPx_, logoHeightPx_);
        detail::drawMainMenuOverlay(frameDebugData, menuTextWidth, menuTextHeight, titleMenuRowBias);
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
    detail::drawSkyHorizonBloom(debugDrawEncoder, cameraFrameData);
    detail::drawSkyAtmosphereVeils(debugDrawEncoder, cameraFrameData);
    drawThinAtmosphereRing(debugDrawEncoder, cameraFrameData);
    detail::drawSkyCirrusBands(debugDrawEncoder, cameraFrameData);
    detail::drawSkyNebulaCanopy(debugDrawEncoder, cameraFrameData);
    drawStarField(debugDrawEncoder, cameraFrameData);
    drawSun(debugDrawEncoder, cameraFrameData);
    drawDistantEarth(debugDrawEncoder, cameraFrameData);
    debugDrawEncoder.pop();
    detail::drawWeatherRain(debugDrawEncoder, cameraFrameData);
    // Celestial pass uses reversed depth-test for sky quads; restore normal depth for world overlays.
    debugDrawEncoder.setDepthTestLess(true);
    // Draw relay safe-zones as layered translucent shields so they read like protective glass domes.
    for (const FrameDebugData::WorldSafeZoneHud& safeZone : frameDebugData.worldSafeZones)
    {
        const glm::vec3 toPlayer = cameraFrameData.position - safeZone.worldCenter;
        const float distanceSq = glm::dot(toPlayer, toPlayer);
        const float radiusWithPadding = safeZone.radius + 156.0f;
        if (distanceSq > radiusWithPadding * radiusWithPadding)
        {
            continue;
        }

        const glm::vec3 activeColor = safeZone.preview
            ? (safeZone.valid ? glm::vec3(0.48f, 1.0f, 0.84f) : glm::vec3(1.0f, 0.46f, 0.52f))
            : glm::vec3(0.30f, 0.86f, 1.0f);
        const GlassShieldProfile& shieldProfile = glassShieldProfile();
        const glm::vec3 glassColor = glm::clamp(glm::mix(activeColor, shieldProfile.tint, 0.68f), glm::vec3(0.0f), glm::vec3(1.0f));
        const float shellAlpha = safeZone.preview ? 0.62f : 0.52f;
        const bx::Vec3 center(safeZone.worldCenter.x, safeZone.worldCenter.y, safeZone.worldCenter.z);
        const float shellRadius = safeZone.radius;

        debugDrawEncoder.push();
        // Transparent material behavior: test against depth but do not write depth.
        debugDrawEncoder.setWireframe(false);
        const auto drawTwoSidedShell = [&](const float radius, const std::uint32_t color)
        {
            debugDrawEncoder.setState(true, false, false);
            debugDrawEncoder.setColor(color);
            debugDrawEncoder.draw(bx::Sphere(center, radius));
            // Render opposite winding so the shell is visible from both inside and outside.
            debugDrawEncoder.setState(true, false, true);
            debugDrawEncoder.setColor(color);
            debugDrawEncoder.draw(bx::Sphere(center, radius));
        };
        drawTwoSidedShell(shellRadius, detail::packAbgr8(glassColor, shellAlpha));

        // Emphasize the grid so shield boundaries are obvious both near and far.
        const glm::vec3 gridTint = glm::clamp(glm::mix(glassColor, glm::vec3(1.0f), 0.35f), glm::vec3(0.0f), glm::vec3(1.0f));
        const float gridAlpha = safeZone.preview ? 0.82f : 0.70f;
        const std::uint32_t gridColor = detail::packAbgr8(gridTint, gridAlpha);
        const auto drawShieldGrid = [&]()
        {
            debugDrawEncoder.setColor(gridColor);
            debugDrawEncoder.draw(bx::Sphere(center, shellRadius));
            debugDrawEncoder.drawCircle(Axis::X, center.x, center.y, center.z, shellRadius, 0.0f);
            debugDrawEncoder.drawCircle(Axis::Y, center.x, center.y, center.z, shellRadius, 0.0f);
            debugDrawEncoder.drawCircle(Axis::Z, center.x, center.y, center.z, shellRadius, 0.0f);
            // Extra lat/long rings to make the grid denser and easier to read.
            debugDrawEncoder.drawCircle(Axis::X, center.x, center.y, center.z, shellRadius * 0.70f, 0.0f);
            debugDrawEncoder.drawCircle(Axis::Y, center.x, center.y, center.z, shellRadius * 0.70f, 0.0f);
            debugDrawEncoder.drawCircle(Axis::Z, center.x, center.y, center.z, shellRadius * 0.70f, 0.0f);
        };
        debugDrawEncoder.setWireframe(true);
        debugDrawEncoder.setState(true, false, false);
        drawShieldGrid();
        debugDrawEncoder.setState(true, false, true);
        drawShieldGrid();
        debugDrawEncoder.setWireframe(false);
        debugDrawEncoder.pop();
    }
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
    drawWorldBirdSprites(frameDebugData, cameraFrameData);
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
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX && !frameDebugData.oxygenStatusLine.empty())
    {
        bgfx::dbgTextPrintf(0, 4, 0x0b, "%s", frameDebugData.oxygenStatusLine.c_str());
    }
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
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX)
    {
        const std::uint16_t oxygenAttr = frameDebugData.oxygenLowWarning
            ? 0x0c
            : (frameDebugData.oxygenInsideSafeZone ? 0x0a : 0x0b);
        const std::string oxygenBar = oxygenBarString(frameDebugData);
        bgfx::dbgTextPrintf(0, 10, oxygenAttr, "%s", oxygenBar.c_str());
        if (frameDebugData.oxygenLowWarning)
        {
            bgfx::dbgTextPrintf(0, 11, 0x0c, "Warning: oxygen low. Reach a generator or safe biome.");
        }
    }
    else
    {
        const std::string zoneLine = fmt::format("O2 zone: {}", frameDebugData.oxygenZoneLabel);
        const int zoneCol = std::max(0, static_cast<int>(textWidthForHud) / 2 - static_cast<int>(zoneLine.size()) / 2);
        const std::uint16_t zoneAttr = frameDebugData.oxygenLowWarning
            ? 0x0c
            : (frameDebugData.oxygenInsideSafeZone ? 0x0a : 0x0b);
        const std::uint16_t zoneRow = healthRow > 2 ? static_cast<std::uint16_t>(healthRow - 2) : 0;
        bgfx::dbgTextPrintf(zoneCol, zoneRow, zoneAttr, "%s", zoneLine.c_str());

        if (frameDebugData.relayPlacementPreviewActive)
        {
            const std::string previewLine = fmt::format(
                "Atmos relay: {}",
                frameDebugData.relayPlacementPreviewLabel.empty()
                    ? (frameDebugData.relayPlacementPreviewValid ? "ready" : "blocked")
                    : frameDebugData.relayPlacementPreviewLabel);
            const int previewCol =
                std::max(0, static_cast<int>(textWidthForHud) / 2 - static_cast<int>(previewLine.size()) / 2);
            const std::uint16_t previewRow = zoneRow > 0 ? static_cast<std::uint16_t>(zoneRow - 1) : zoneRow;
            bgfx::dbgTextPrintf(
                previewCol,
                previewRow,
                frameDebugData.relayPlacementPreviewValid ? 0x0a : 0x0c,
                "%s",
                previewLine.c_str());
        }
    }
    const std::uint16_t selectedLabelRow =
        inventoryUiSolidProgramHandle_ == UINT16_MAX ? (frameDebugData.oxygenLowWarning ? 12 : 11) : 10;
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
        "Controls: WASD move, Shift sneak, Ctrl sprint, Space jump, E inventory, mouse look");
    bgfx::dbgTextPrintf(
        0,
        controlsRow1,
        0x0e,
        "LMB mine, RMB place, 1-9 select hotbar, Tab capture, Esc pause menu");

    bgfx::frame();
}

} // namespace vibecraft::render
