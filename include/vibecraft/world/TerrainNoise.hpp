#pragma once

#include <cmath>
#include <cstdint>

namespace vibecraft::world::noise
{
[[nodiscard]] inline std::uint32_t hashCoordinates(const int x, const int z, const std::uint32_t seed)
{
    std::uint32_t hash = seed;
    hash ^= static_cast<std::uint32_t>(x) * 0x9e3779b9U;
    hash = (hash << 6U) ^ (hash >> 2U);
    hash ^= static_cast<std::uint32_t>(z) * 0x85ebca6bU;
    hash *= 0xc2b2ae35U;
    hash ^= hash >> 16U;
    return hash;
}

[[nodiscard]] inline double random01(const int x, const int z, const std::uint32_t seed)
{
    constexpr double kInvMax = 1.0 / static_cast<double>(0xffffffffU);
    return static_cast<double>(hashCoordinates(x, z, seed)) * kInvMax;
}

[[nodiscard]] inline double smoothstep(const double t)
{
    return t * t * (3.0 - 2.0 * t);
}

[[nodiscard]] inline double valueNoise2d(
    const double worldX,
    const double worldZ,
    const double scale,
    const std::uint32_t seed)
{
    const double scaledX = worldX / scale;
    const double scaledZ = worldZ / scale;
    const int x0 = static_cast<int>(std::floor(scaledX));
    const int z0 = static_cast<int>(std::floor(scaledZ));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const double tx = smoothstep(scaledX - static_cast<double>(x0));
    const double tz = smoothstep(scaledZ - static_cast<double>(z0));

    const double n00 = random01(x0, z0, seed);
    const double n10 = random01(x1, z0, seed);
    const double n01 = random01(x0, z1, seed);
    const double n11 = random01(x1, z1, seed);

    const double nx0 = std::lerp(n00, n10, tx);
    const double nx1 = std::lerp(n01, n11, tx);
    return std::lerp(nx0, nx1, tz);
}

[[nodiscard]] inline double fbmNoise2d(
    const double worldX,
    const double worldZ,
    const double baseScale,
    const int octaves,
    const std::uint32_t seed)
{
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    double scale = baseScale;

    for (int octave = 0; octave < octaves; ++octave)
    {
        value += valueNoise2d(worldX, worldZ, scale, seed + static_cast<std::uint32_t>(octave) * 101U)
            * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        scale *= 0.5;
    }

    return amplitudeSum > 0.0 ? value / amplitudeSum : 0.0;
}

[[nodiscard]] inline double ridgeNoise2d(
    const double worldX,
    const double worldZ,
    const double scale,
    const std::uint32_t seed)
{
    const double base = fbmNoise2d(worldX, worldZ, scale, 3, seed);
    return 1.0 - std::abs(base * 2.0 - 1.0);
}
}  // namespace vibecraft::world::noise
