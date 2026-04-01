#include "vibecraft/app/ApplicationAmbientLife.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace vibecraft::app
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

[[nodiscard]] float birdBiomeActivity(const world::SurfaceBiome biome)
{
    switch (biome)
    {
    case world::SurfaceBiome::Jungle:
        return 1.0f;
    case world::SurfaceBiome::TemperateGrassland:
        return 0.62f;
    case world::SurfaceBiome::Snowy:
        return 0.26f;
    case world::SurfaceBiome::Sandy:
    default:
        return 0.0f;
    }
}

[[nodiscard]] glm::vec3 birdTintForBiome(const world::SurfaceBiome biome)
{
    switch (biome)
    {
    case world::SurfaceBiome::Jungle:
        return glm::vec3(0.52f, 0.98f, 0.92f);
    case world::SurfaceBiome::TemperateGrassland:
        return glm::vec3(0.98f, 0.94f, 0.82f);
    case world::SurfaceBiome::Snowy:
        return glm::vec3(0.82f, 0.90f, 1.0f);
    case world::SurfaceBiome::Sandy:
    default:
        return glm::vec3(0.90f, 0.86f, 0.72f);
    }
}
}  // namespace

std::vector<render::FrameDebugData::WorldBirdHud> buildAmbientBirdHud(
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& cameraPosition,
    const world::SurfaceBiome biome,
    const float weatherTimeSeconds,
    const float rainIntensity,
    const float sunVisibility)
{
    std::vector<render::FrameDebugData::WorldBirdHud> birds;
    const float baseActivity = birdBiomeActivity(biome);
    if (baseActivity <= 0.01f)
    {
        return birds;
    }

    const float weatherSuppression = 1.0f - glm::clamp(rainIntensity * 1.65f, 0.0f, 0.95f);
    const float lightSuppression = glm::clamp(0.18f + sunVisibility * 0.92f, 0.0f, 1.0f);
    const float activity = baseActivity * weatherSuppression * lightSuppression;
    if (activity <= 0.06f)
    {
        return birds;
    }

    constexpr int kCellSize = 52;
    constexpr int kRadiusCells = 3;
    constexpr std::size_t kMaxBirds = 18;
    const int centerCellX = static_cast<int>(std::floor(cameraPosition.x / static_cast<float>(kCellSize)));
    const int centerCellZ = static_cast<int>(std::floor(cameraPosition.z / static_cast<float>(kCellSize)));
    const glm::vec3 tint = birdTintForBiome(biome);
    birds.reserve(kMaxBirds);

    for (int cellZ = centerCellZ - kRadiusCells; cellZ <= centerCellZ + kRadiusCells; ++cellZ)
    {
        for (int cellX = centerCellX - kRadiusCells; cellX <= centerCellX + kRadiusCells; ++cellX)
        {
            const float flockChance = 0.12f + activity * 0.55f;
            if (hash01(cellX, cellZ, 701) > flockChance)
            {
                continue;
            }

            const float anchorX = static_cast<float>(cellX * kCellSize)
                + (hash01(cellX, cellZ, 711) - 0.5f) * static_cast<float>(kCellSize - 12);
            const float anchorZ = static_cast<float>(cellZ * kCellSize)
                + (hash01(cellX, cellZ, 721) - 0.5f) * static_cast<float>(kCellSize - 12);
            const int sampleX = static_cast<int>(std::floor(anchorX));
            const int sampleZ = static_cast<int>(std::floor(anchorZ));
            const world::SurfaceBiome sampleBiome = terrainGenerator.surfaceBiomeAt(sampleX, sampleZ);
            if (birdBiomeActivity(sampleBiome) <= 0.01f)
            {
                continue;
            }

            const float surfaceY = static_cast<float>(terrainGenerator.surfaceHeightAt(sampleX, sampleZ));
            const float altitude = sampleBiome == world::SurfaceBiome::Jungle
                ? 26.0f + hash01(cellX, cellZ, 731) * 18.0f
                : 20.0f + hash01(cellX, cellZ, 731) * 14.0f;
            const float anchorY = surfaceY + altitude;
            const std::uint32_t flockHash = hashUint32(cellX, cellZ, 741);
            const int flockCount = 1 + static_cast<int>(flockHash % 3U) + (sampleBiome == world::SurfaceBiome::Jungle ? 1 : 0);

            for (int birdIndex = 0; birdIndex < flockCount; ++birdIndex)
            {
                if (birds.size() >= kMaxBirds)
                {
                    return birds;
                }

                const float phaseOffset = hash01(cellX, cellZ, 751 + birdIndex) * 6.28318530718f;
                const float heading = hash01(cellX, cellZ, 801 + birdIndex) * 6.28318530718f;
                glm::vec2 forward(std::cos(heading), std::sin(heading));
                const float forwardLenSq = forward.x * forward.x + forward.y * forward.y;
                if (forwardLenSq > 1.0e-8f)
                {
                    const float invLen = 1.0f / std::sqrt(forwardLenSq);
                    forward.x *= invLen;
                    forward.y *= invLen;
                }
                else
                {
                    forward = glm::vec2(1.0f, 0.0f);
                }
                const glm::vec2 perp(-forward.y, forward.x);

                const float flapHz = 2.05f + hash01(cellX, birdIndex, 811) * 0.55f;
                const float flapPhase =
                    weatherTimeSeconds * (6.28318530718f * flapHz) + phaseOffset + static_cast<float>(birdIndex) * 0.7f;

                const float cruise = 4.8f + hash01(cellZ, birdIndex, 821) * 4.2f;
                const float alongOffset = hash01(cellX, birdIndex, 831) * 100.0f;
                const float pathSpan = 95.0f;
                float along = std::fmod(weatherTimeSeconds * cruise + alongOffset, pathSpan);
                if (along < 0.0f)
                {
                    along += pathSpan;
                }
                along -= pathSpan * 0.5f;

                const float lateral =
                    (1.15f + hash01(cellZ, cellX, 841) * 1.1f) * std::sin(weatherTimeSeconds * 0.88f + phaseOffset * 0.35f);
                const float downstroke = std::max(0.0f, -std::cos(flapPhase));
                const float surge = 0.55f * downstroke * downstroke;

                const float verticalBob =
                    0.24f * std::sin(flapPhase) + 0.07f * std::sin(weatherTimeSeconds * 0.62f + phaseOffset);

                const glm::vec3 position(
                    anchorX + forward.x * (along + surge) + perp.x * lateral,
                    anchorY + verticalBob,
                    anchorZ + forward.y * (along + surge) + perp.y * lateral);
                const glm::vec3 toBird = position - cameraPosition;
                const float distanceSq = glm::dot(toBird, toBird);
                if (distanceSq < 18.0f * 18.0f || distanceSq > 190.0f * 190.0f)
                {
                    continue;
                }

                const float scale = sampleBiome == world::SurfaceBiome::Snowy ? 0.86f : 1.0f;
                birds.push_back(render::FrameDebugData::WorldBirdHud{
                    .worldPosition = position,
                    .halfWidth = (sampleBiome == world::SurfaceBiome::Jungle ? 1.15f : 0.95f) * scale,
                    .halfHeight = (sampleBiome == world::SurfaceBiome::Jungle ? 0.48f : 0.42f) * scale,
                    .tint = tint,
                    .alpha = 0.42f + activity * 0.35f,
                    .flapPhase = flapPhase,
                    .flightForwardXZ = forward,
                });
            }
        }
    }

    return birds;
}
}  // namespace vibecraft::app
