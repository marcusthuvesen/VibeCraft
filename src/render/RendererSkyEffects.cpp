#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "debugdraw.h"

namespace vibecraft::render::detail
{
namespace
{
[[nodiscard]] std::uint32_t hashUint32(const std::int32_t a, const std::int32_t b, const std::int32_t c)
{
    std::uint32_t value = static_cast<std::uint32_t>(a) * 0x9e3779b9U;
    value ^= static_cast<std::uint32_t>(b) * 0x85ebca6bU + 0x7f4a7c15U;
    value ^= static_cast<std::uint32_t>(c) * 0xc2b2ae35U + 0x165667b1U;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] float hash01(const std::int32_t a, const std::int32_t b, const std::int32_t c)
{
    return static_cast<float>(hashUint32(a, b, c) & 0x00ffffffU) / static_cast<float>(0x01000000U);
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
    const float alpha)
{
    const glm::vec3 normal = celestialBillboardNormal(cameraPosition, center);
    debugDrawEncoder.setColor(packAbgr8(color, alpha));
    debugDrawEncoder.drawQuad(
        bx::Vec3(normal.x, normal.y, normal.z),
        bx::Vec3(center.x, center.y, center.z),
        size);
}

[[nodiscard]] glm::vec3 safeNormalized(const glm::vec3& value, const glm::vec3& fallback)
{
    return glm::dot(value, value) > 1.0e-6f ? glm::normalize(value) : fallback;
}
}  // namespace

void drawSkyAtmosphereVeils(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr int kPanelCount = 14;
    constexpr float kRingDistance = 214.0f;
    const glm::vec3 veilTint = glm::mix(cameraFrameData.horizonTint, cameraFrameData.sunLightTint, 0.32f);
    const glm::vec3 coolTint = glm::mix(cameraFrameData.skyTint, cameraFrameData.cloudTint, 0.62f);
    const glm::vec3 sunHorizDir = safeNormalized(
        glm::vec3(cameraFrameData.sunDirection.x, 0.24f, cameraFrameData.sunDirection.z),
        glm::vec3(1.0f, 0.0f, 0.0f));

    for (int panelIndex = 0; panelIndex < kPanelCount; ++panelIndex)
    {
        const float angle = (static_cast<float>(panelIndex) / static_cast<float>(kPanelCount)) * kTwoPi;
        const glm::vec3 direction = glm::normalize(glm::vec3(std::cos(angle), 0.18f, std::sin(angle)));
        const float sunBlend = std::clamp(
            glm::dot(direction, sunHorizDir) * 0.5f + 0.5f,
            0.0f,
            1.0f);
        const glm::vec3 tint = glm::mix(coolTint, veilTint, sunBlend * 0.72f);
        const glm::vec3 center = cameraFrameData.position
            + direction * kRingDistance
            + glm::vec3(0.0f, -14.0f + std::sin(angle * 1.7f) * 7.0f, 0.0f);
        const float size = 198.0f + sunBlend * 48.0f;
        const float alpha = 0.008f + cameraFrameData.sunVisibility * 0.018f + sunBlend * 0.012f;
        drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, center, size, tint, alpha);
    }
}

void drawSkyCirrusBands(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    constexpr int kBandCount = 12;
    const glm::vec3 bandTint = glm::mix(cameraFrameData.cloudTint, cameraFrameData.sunLightTint, 0.28f);
    const glm::vec3 duskTint = glm::mix(cameraFrameData.skyTint, cameraFrameData.horizonTint, 0.5f);
    const glm::vec3 windDir = glm::normalize(glm::vec3(
        cameraFrameData.weatherWindDirectionXZ.x,
        0.0f,
        cameraFrameData.weatherWindDirectionXZ.y));
    const glm::vec3 fallbackDir = glm::dot(windDir, windDir) > 0.0f ? windDir : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 sideDir = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), fallbackDir));

    for (int bandIndex = 0; bandIndex < kBandCount; ++bandIndex)
    {
        const float lane = static_cast<float>(bandIndex - kBandCount / 2);
        const float drift = cameraFrameData.weatherTimeSeconds * (0.4f + bandIndex * 0.03f);
        const glm::vec3 baseCenter = cameraFrameData.position
            + fallbackDir * (70.0f + std::fmod(drift * 24.0f + hash01(bandIndex, 0, 811) * 70.0f, 120.0f))
            + sideDir * (lane * 18.0f + (hash01(bandIndex, 0, 821) - 0.5f) * 16.0f)
            + glm::vec3(0.0f, 84.0f + hash01(bandIndex, 0, 831) * 30.0f, 0.0f);
        const glm::vec3 tint = glm::mix(
            duskTint,
            bandTint,
            0.45f + hash01(bandIndex, 0, 841) * 0.4f + cameraFrameData.sunVisibility * 0.18f);
        const float size = 34.0f + hash01(bandIndex, 0, 851) * 38.0f;
        const float alpha = 0.05f + cameraFrameData.cloudCoverage * 0.08f + hash01(bandIndex, 0, 861) * 0.04f;
        drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, baseCenter, size, tint, alpha);
    }
}

void drawSkyHorizonBloom(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr int kBloomCount = 18;
    const glm::vec3 warmGlow = glm::mix(cameraFrameData.horizonTint, cameraFrameData.sunLightTint, 0.62f);
    const glm::vec3 coolGlow = glm::mix(cameraFrameData.skyTint, cameraFrameData.cloudTint, 0.30f);
    const glm::vec3 horizonSunDir = safeNormalized(
        glm::vec3(cameraFrameData.sunDirection.x, 0.12f, cameraFrameData.sunDirection.z),
        glm::vec3(1.0f, 0.0f, 0.0f));

    for (int bloomIndex = 0; bloomIndex < kBloomCount; ++bloomIndex)
    {
        const float angle = (static_cast<float>(bloomIndex) / static_cast<float>(kBloomCount)) * kTwoPi;
        const glm::vec3 direction = glm::normalize(glm::vec3(std::cos(angle), 0.04f, std::sin(angle)));
        const float sunWeight = std::clamp(glm::dot(direction, horizonSunDir) * 0.5f + 0.5f, 0.0f, 1.0f);
        const glm::vec3 tint = glm::mix(coolGlow, warmGlow, sunWeight * 0.82f);
        const glm::vec3 center = cameraFrameData.position
            + direction * (170.0f + std::sin(angle * 1.5f) * 8.0f)
            + glm::vec3(0.0f, -26.0f + sunWeight * 5.0f, 0.0f);
        const float size = 168.0f + sunWeight * 62.0f;
        const float alpha = 0.015f + cameraFrameData.sunVisibility * 0.025f + cameraFrameData.cloudCoverage * 0.01f;
        drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, center, size, tint, alpha);
    }
}

void drawSkyNebulaCanopy(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.sunVisibility > 0.20f)
    {
        return;
    }

    constexpr int kNebulaCount = 9;
    const glm::vec3 baseTint = glm::mix(cameraFrameData.skyTint, glm::vec3(0.66f, 0.30f, 1.0f), 0.34f);
    const glm::vec3 accentTint = glm::mix(cameraFrameData.cloudTint, glm::vec3(0.30f, 1.0f, 0.92f), 0.40f);
    const glm::vec3 windDir = safeNormalized(
        glm::vec3(cameraFrameData.weatherWindDirectionXZ.x, 0.0f, cameraFrameData.weatherWindDirectionXZ.y),
        glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 sideDir = safeNormalized(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), windDir), glm::vec3(0.0f, 0.0f, 1.0f));

    for (int nebulaIndex = 0; nebulaIndex < kNebulaCount; ++nebulaIndex)
    {
        const float lane = static_cast<float>(nebulaIndex - kNebulaCount / 2);
        const float drift = cameraFrameData.weatherTimeSeconds * (0.15f + nebulaIndex * 0.02f);
        const float pulse = 0.5f + 0.5f * std::sin(drift + nebulaIndex * 0.9f);
        const glm::vec3 tint = glm::mix(baseTint, accentTint, 0.34f + pulse * 0.44f);
        const glm::vec3 center = cameraFrameData.position
            + windDir * (94.0f + std::fmod(drift * 18.0f + hash01(nebulaIndex, 0, 911) * 50.0f, 96.0f))
            + sideDir * (lane * 26.0f + (hash01(nebulaIndex, 0, 921) - 0.5f) * 18.0f)
            + glm::vec3(0.0f, 128.0f + hash01(nebulaIndex, 0, 931) * 48.0f, 0.0f);
        const float size = 58.0f + hash01(nebulaIndex, 0, 941) * 64.0f;
        const float alpha =
            0.04f + cameraFrameData.cloudCoverage * 0.05f + (1.0f - cameraFrameData.sunVisibility) * 0.07f + pulse * 0.03f;
        drawCelestialQuad(debugDrawEncoder, cameraFrameData.position, center, size, tint, alpha);
    }
}
}  // namespace vibecraft::render::detail
