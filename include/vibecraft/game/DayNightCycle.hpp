#pragma once

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>

namespace vibecraft::game
{
enum class TimeOfDayPeriod
{
    Dawn,
    Day,
    Dusk,
    Night,
};

struct DayNightSample
{
    TimeOfDayPeriod period = TimeOfDayPeriod::Day;
    float elapsedSeconds = 0.0f;
    float cycleProgress = 0.0f;
    float sunOrbitDegrees360 = 0.0f;
    float moonOrbitDegrees360 = 180.0f;
    glm::vec3 sunDirection{1.0f, 0.0f, 0.0f};
    glm::vec3 moonDirection{-1.0f, 0.0f, 0.0f};
    float sunVisibility = 0.0f;
    float moonVisibility = 0.0f;
    glm::vec3 skyTint{0.54f, 0.76f, 0.98f};
    glm::vec3 horizonTint{0.76f, 0.88f, 1.0f};
    glm::vec3 sunLightTint{1.0f, 0.97f, 0.92f};
    glm::vec3 moonLightTint{0.62f, 0.72f, 1.0f};
};

namespace detail
{
inline constexpr float kFullRotationDegrees = 360.0f;
inline constexpr float kTwoPi = 6.28318530717958647692f;
inline const glm::vec3 kDaySkyTint{0.44f, 0.67f, 0.98f};
inline const glm::vec3 kNightSkyTint{0.01f, 0.01f, 0.03f};
inline const glm::vec3 kTwilightSkyTint{0.92f, 0.50f, 0.30f};
inline const glm::vec3 kDayHorizonTint{0.76f, 0.86f, 0.98f};
inline const glm::vec3 kNightHorizonTint{0.05f, 0.05f, 0.08f};
inline const glm::vec3 kTwilightHorizonTint{0.98f, 0.68f, 0.42f};
inline const glm::vec3 kDaySunTint{1.0f, 0.95f, 0.86f};
inline const glm::vec3 kTwilightSunTint{1.0f, 0.68f, 0.36f};
inline const glm::vec3 kMoonLightTint{0.62f, 0.72f, 1.0f};

[[nodiscard]] inline float wrapPositive(const float value, const float modulus)
{
    const float wrapped = std::fmod(value, modulus);
    return wrapped < 0.0f ? wrapped + modulus : wrapped;
}

[[nodiscard]] inline glm::vec3 blendTwilight(
    const float progress,
    const glm::vec3& fromTint,
    const glm::vec3& twilightTint,
    const glm::vec3& toTint)
{
    const float clampedProgress = glm::clamp(progress, 0.0f, 1.0f);
    if (clampedProgress < 0.5f)
    {
        return glm::mix(fromTint, twilightTint, clampedProgress * 2.0f);
    }

    return glm::mix(twilightTint, toTint, (clampedProgress - 0.5f) * 2.0f);
}
}  // namespace detail

class DayNightCycle
{
  public:
    // Minecraft-like pacing: full cycle ~= 20 minutes.
    static constexpr float kDaylightDurationSeconds = 600.0f;
    static constexpr float kNightDurationSeconds = 600.0f;
    static constexpr float kFullCycleDurationSeconds = kDaylightDurationSeconds + kNightDurationSeconds;
    static constexpr float kTwilightDurationSeconds = 90.0f;

    DayNightCycle() = default;
    explicit DayNightCycle(const float elapsedSeconds) : elapsedSeconds_(wrapCycleSeconds(elapsedSeconds)) {}

    void advanceSeconds(const float deltaSeconds)
    {
        elapsedSeconds_ = wrapCycleSeconds(elapsedSeconds_ + deltaSeconds);
    }

    void setElapsedSeconds(const float elapsedSeconds)
    {
        elapsedSeconds_ = wrapCycleSeconds(elapsedSeconds);
    }

    [[nodiscard]] float elapsedSeconds() const
    {
        return elapsedSeconds_;
    }

    [[nodiscard]] float cycleProgress() const
    {
        return elapsedSeconds_ / kFullCycleDurationSeconds;
    }

    [[nodiscard]] DayNightSample sample() const
    {
        return sampleAtElapsedSeconds(elapsedSeconds_);
    }

    [[nodiscard]] static float wrapCycleSeconds(const float elapsedSeconds)
    {
        return detail::wrapPositive(elapsedSeconds, kFullCycleDurationSeconds);
    }

    [[nodiscard]] static DayNightSample sampleAtElapsedSeconds(const float elapsedSeconds)
    {
        DayNightSample sample;
        sample.elapsedSeconds = wrapCycleSeconds(elapsedSeconds);
        sample.cycleProgress = sample.elapsedSeconds / kFullCycleDurationSeconds;
        sample.sunOrbitDegrees360 =
            detail::wrapPositive(sample.cycleProgress * detail::kFullRotationDegrees, detail::kFullRotationDegrees);
        sample.moonOrbitDegrees360 =
            detail::wrapPositive(sample.sunOrbitDegrees360 + 180.0f, detail::kFullRotationDegrees);

        const float sunOrbitRadians = sample.cycleProgress * detail::kTwoPi;

        // +X is east and -X is west. The celestial arc moves through the X/Y plane.
        sample.sunDirection = glm::normalize(glm::vec3(std::cos(sunOrbitRadians), std::sin(sunOrbitRadians), 0.0f));
        sample.moonDirection = -sample.sunDirection;
        sample.sunVisibility = glm::clamp(sample.sunDirection.y, 0.0f, 1.0f);
        sample.moonVisibility = glm::clamp(sample.moonDirection.y, 0.0f, 1.0f);
        sample.moonLightTint = detail::kMoonLightTint;

        if (sample.elapsedSeconds < kTwilightDurationSeconds)
        {
            const float dawnProgress = sample.elapsedSeconds / kTwilightDurationSeconds;
            sample.period = TimeOfDayPeriod::Dawn;
            sample.skyTint = detail::blendTwilight(
                dawnProgress,
                detail::kNightSkyTint,
                detail::kTwilightSkyTint,
                detail::kDaySkyTint);
            sample.horizonTint = detail::blendTwilight(
                dawnProgress,
                detail::kNightHorizonTint,
                detail::kTwilightHorizonTint,
                detail::kDayHorizonTint);
            sample.sunLightTint = detail::blendTwilight(
                dawnProgress,
                detail::kTwilightSunTint,
                detail::kTwilightSunTint,
                detail::kDaySunTint);
            return sample;
        }

        if (sample.elapsedSeconds < kDaylightDurationSeconds - kTwilightDurationSeconds)
        {
            sample.period = TimeOfDayPeriod::Day;
            sample.skyTint = detail::kDaySkyTint;
            sample.horizonTint = detail::kDayHorizonTint;
            sample.sunLightTint = detail::kDaySunTint;
            return sample;
        }

        if (sample.elapsedSeconds < kDaylightDurationSeconds)
        {
            const float duskProgress =
                (sample.elapsedSeconds - (kDaylightDurationSeconds - kTwilightDurationSeconds))
                / kTwilightDurationSeconds;
            sample.period = TimeOfDayPeriod::Dusk;
            sample.skyTint = detail::blendTwilight(
                duskProgress,
                detail::kDaySkyTint,
                detail::kTwilightSkyTint,
                detail::kNightSkyTint);
            sample.horizonTint = detail::blendTwilight(
                duskProgress,
                detail::kDayHorizonTint,
                detail::kTwilightHorizonTint,
                detail::kNightHorizonTint);
            sample.sunLightTint = detail::blendTwilight(
                duskProgress,
                detail::kDaySunTint,
                detail::kTwilightSunTint,
                detail::kTwilightSunTint);
            return sample;
        }

        sample.period = TimeOfDayPeriod::Night;
        sample.skyTint = detail::kNightSkyTint;
        sample.horizonTint = detail::kNightHorizonTint;
        sample.sunLightTint = detail::kTwilightSunTint;
        return sample;
    }

  private:
    float elapsedSeconds_ = 0.0f;
};
}  // namespace vibecraft::game
