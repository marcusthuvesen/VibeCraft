#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "debugdraw.h"

namespace vibecraft::render::detail
{


[[nodiscard]] std::uint32_t hashUint32(
    const std::int32_t a,
    const std::int32_t b,
    const std::int32_t c)
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

[[nodiscard]] float hashUnitFloat(
    const std::int32_t a,
    const std::int32_t b,
    const std::int32_t c)
{
    return static_cast<float>(hashUint32(a, b, c) & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

[[nodiscard]] float bilerp(
    const float v00,
    const float v10,
    const float v01,
    const float v11,
    const float tx,
    const float tz)
{
    const float vx0 = v00 + (v10 - v00) * tx;
    const float vx1 = v01 + (v11 - v01) * tx;
    return vx0 + (vx1 - vx0) * tz;
}

[[nodiscard]] float smoothValueNoise2d(
    const float sampleX,
    const float sampleZ,
    const std::int32_t seed)
{
    const std::int32_t x0 = static_cast<std::int32_t>(std::floor(sampleX));
    const std::int32_t z0 = static_cast<std::int32_t>(std::floor(sampleZ));
    const std::int32_t x1 = x0 + 1;
    const std::int32_t z1 = z0 + 1;
    const float fracX = sampleX - static_cast<float>(x0);
    const float fracZ = sampleZ - static_cast<float>(z0);
    const float tx = fracX * fracX * (3.0f - 2.0f * fracX);
    const float tz = fracZ * fracZ * (3.0f - 2.0f * fracZ);

    const float v00 = hashUnitFloat(x0, z0, seed);
    const float v10 = hashUnitFloat(x1, z0, seed);
    const float v01 = hashUnitFloat(x0, z1, seed);
    const float v11 = hashUnitFloat(x1, z1, seed);
    return bilerp(v00, v10, v01, v11, tx, tz);
}

/// Two octaves (broad + medium): cheaper than three; `broad` may be reused from a cheap-reject pass.
[[nodiscard]] float cloudPatchDensityFromBroadMedium(const float broad, const float sampleX, const float sampleZ)
{
    const float medium = smoothValueNoise2d(sampleX * 0.53f, sampleZ * 0.53f, 902);
    return glm::clamp(broad * 0.62f + medium * 0.38f, 0.0f, 1.0f);
}

[[nodiscard]] glm::vec2 normalizeOrFallback(const glm::vec2& vector, const glm::vec2& fallback)
{
    const float lengthSquared = glm::dot(vector, vector);
    return lengthSquared > 0.0f ? glm::normalize(vector) : fallback;
}

void drawWeatherClouds(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.cloudCoverage <= 0.02f)
    {
        return;
    }

    const glm::vec2 windDirection =
        normalizeOrFallback(cameraFrameData.weatherWindDirectionXZ, glm::vec2(1.0f, 0.0f));
    const glm::vec2 windOffset = windDirection * cameraFrameData.weatherTimeSeconds * cameraFrameData.weatherWindSpeed;
    constexpr float kCloudCellSize = 36.0f;
    const int cloudRadiusInCells =
        std::clamp(static_cast<int>(2.0f + cameraFrameData.cloudCoverage * 1.6f), 2, 3);
    const float cloudHeight = glm::max(78.0f, cameraFrameData.position.y + 38.0f);
    const int baseCellX = static_cast<int>(std::floor((cameraFrameData.position.x + windOffset.x) / kCloudCellSize));
    const int baseCellZ = static_cast<int>(std::floor((cameraFrameData.position.z + windOffset.y) / kCloudCellSize));
    const float densityThreshold = std::clamp(0.86f - cameraFrameData.cloudCoverage * 0.62f, 0.24f, 0.82f);
    const bool drawSecondaryCloudLayer = cameraFrameData.cloudCoverage > 0.72f;
    const int cloudStride = cameraFrameData.cloudCoverage < 0.48f ? 2 : 1;

    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(true);

    for (int cellZ = -cloudRadiusInCells; cellZ <= cloudRadiusInCells; cellZ += cloudStride)
    {
        for (int cellX = -cloudRadiusInCells; cellX <= cloudRadiusInCells; cellX += cloudStride)
        {
            const int gridX = baseCellX + cellX;
            const int gridZ = baseCellZ + cellZ;
            const float sampleX = static_cast<float>(gridX) + windOffset.x * 0.018f;
            const float sampleZ = static_cast<float>(gridZ) + windOffset.y * 0.018f;
            const float broad = smoothValueNoise2d(sampleX * 0.24f, sampleZ * 0.24f, 901);
            // Upper bound on two-octave mix: 0.62 * broad + 0.38 * 1.0
            if (0.62f * broad + 0.38f < densityThreshold)
            {
                continue;
            }

            const float density = cloudPatchDensityFromBroadMedium(broad, sampleX, sampleZ);
            if (density < densityThreshold)
            {
                continue;
            }

            const float patchStrength = std::clamp(
                (density - densityThreshold) / std::max(0.001f, 1.0f - densityThreshold),
                0.0f,
                1.0f);
            const float centerX =
                static_cast<float>(gridX) * kCloudCellSize
                - windOffset.x
                + (hashUnitFloat(gridX, gridZ, 21) - 0.5f) * 14.0f;
            const float centerZ =
                static_cast<float>(gridZ) * kCloudCellSize
                - windOffset.y
                + (hashUnitFloat(gridX, gridZ, 31) - 0.5f) * 12.0f;
            const float y = cloudHeight + (hashUnitFloat(gridX, gridZ, 41) - 0.5f) * 4.0f;
            const float baseSize = 16.0f + patchStrength * 22.0f + hashUnitFloat(gridX, gridZ, 51) * 8.0f;
            const float stretch = 0.9f + hashUnitFloat(gridX, gridZ, 61) * 0.5f;
            const float secondaryOffset = 5.0f + hashUnitFloat(gridX, gridZ, 71) * 5.0f;
            const glm::vec3 brightCloudWhite(0.97f, 0.98f, 1.0f);
            const glm::vec3 primaryTint = glm::mix(
                cameraFrameData.cloudTint,
                brightCloudWhite,
                0.72f + patchStrength * 0.24f);
            const glm::vec3 secondaryTint = glm::mix(primaryTint, cameraFrameData.skyTint, 0.12f);

            debugDrawEncoder.setColor(packAbgr8(primaryTint, 1.0f));
            debugDrawEncoder.drawQuad(
                bx::Vec3(0.0f, 1.0f, 0.0f),
                bx::Vec3(centerX, y, centerZ),
                baseSize * stretch);

            if (drawSecondaryCloudLayer)
            {
                debugDrawEncoder.setColor(packAbgr8(secondaryTint, 1.0f));
                debugDrawEncoder.drawQuad(
                    bx::Vec3(0.0f, 1.0f, 0.0f),
                    bx::Vec3(centerX + secondaryOffset, y - 1.0f, centerZ - secondaryOffset * 0.5f),
                    baseSize * 0.58f);
            }
        }
    }

    debugDrawEncoder.pop();
}

void drawWeatherRain(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.rainIntensity <= 0.02f)
    {
        return;
    }

    const glm::vec2 windDirection =
        normalizeOrFallback(cameraFrameData.weatherWindDirectionXZ, glm::vec2(1.0f, 0.0f));
    const float rainGridSpacing = 5.5f + (1.0f - cameraFrameData.rainIntensity) * 2.5f;
    const int rainRadiusInCells = std::clamp(static_cast<int>(2.0f + cameraFrameData.rainIntensity * 1.5f), 2, 4);
    const float rainFallDistance = 5.0f + cameraFrameData.rainIntensity * 2.8f;
    const float rainSpeed = 13.0f + cameraFrameData.rainIntensity * 8.0f;
    const glm::vec3 rainVector(
        windDirection.x * 0.35f,
        -1.0f,
        windDirection.y * 0.35f);
    const glm::vec3 normalizedRainVector = glm::normalize(rainVector);
    const int baseCellX = static_cast<int>(std::floor(cameraFrameData.position.x / rainGridSpacing));
    const int baseCellZ = static_cast<int>(std::floor(cameraFrameData.position.z / rainGridSpacing));
    const glm::vec3 rainTint = glm::mix(cameraFrameData.cloudTint, glm::vec3(0.70f, 0.82f, 1.0f), 0.65f);
    const int rainStride =
        cameraFrameData.rainIntensity < 0.30f ? 3 : (cameraFrameData.rainIntensity < 0.65f ? 2 : 1);

    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(true);
    debugDrawEncoder.setColor(packAbgr8(rainTint, 1.0f));

    for (int cellZ = -rainRadiusInCells; cellZ <= rainRadiusInCells; cellZ += rainStride)
    {
        for (int cellX = -rainRadiusInCells; cellX <= rainRadiusInCells; cellX += rainStride)
        {
            const int gridX = baseCellX + cellX;
            const int gridZ = baseCellZ + cellZ;
            if (hashUnitFloat(gridX, gridZ, 101) > cameraFrameData.rainIntensity * 0.92f)
            {
                continue;
            }

            const float offsetX = (hashUnitFloat(gridX, gridZ, 111) - 0.5f) * 1.6f;
            const float offsetZ = (hashUnitFloat(gridX, gridZ, 121) - 0.5f) * 1.6f;
            const float phase = hashUnitFloat(gridX, gridZ, 131);
            const float dropCycle =
                std::fmod(cameraFrameData.weatherTimeSeconds * rainSpeed + phase * rainFallDistance, rainFallDistance);
            const float startY = cameraFrameData.position.y + 12.0f - dropCycle;
            const glm::vec3 start(
                static_cast<float>(gridX) * rainGridSpacing + offsetX,
                startY,
                static_cast<float>(gridZ) * rainGridSpacing + offsetZ);
            const glm::vec3 end = start + normalizedRainVector * rainFallDistance;

            debugDrawEncoder.moveTo(start.x, start.y, start.z);
            debugDrawEncoder.lineTo(end.x, end.y, end.z);
        }
    }

    debugDrawEncoder.pop();
}

} // namespace vibecraft::render::detail
