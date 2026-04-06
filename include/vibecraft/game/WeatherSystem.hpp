#pragma once

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cmath>

namespace vibecraft::game
{
enum class WeatherType
{
    Clear,
    Cloudy,
    Rain,
};

struct WeatherSample
{
    WeatherType type = WeatherType::Clear;
    WeatherType nextType = WeatherType::Clear;
    float elapsedSeconds = 0.0f;
    float stateElapsedSeconds = 0.0f;
    float cycleProgress = 0.0f;
    float transitionProgress = 0.0f;
    float cloudCoverage = 0.15f;
    float rainIntensity = 0.0f;
    float sunOcclusion = 0.05f;
    glm::vec3 skyTintMultiplier{1.0f, 1.0f, 1.0f};
    glm::vec3 horizonTintMultiplier{1.0f, 1.0f, 1.0f};
    glm::vec3 cloudTint{0.96f, 0.97f, 1.0f};
    glm::vec2 windDirectionXZ{1.0f, 0.0f};
    float windSpeed = 2.0f;
};

namespace weather_detail
{
struct WeatherPreset
{
    WeatherType type = WeatherType::Clear;
    float durationSeconds = 90.0f;
    float cloudCoverage = 0.15f;
    float rainIntensity = 0.0f;
    float sunOcclusion = 0.05f;
    glm::vec3 skyTintMultiplier{1.0f, 1.0f, 1.0f};
    glm::vec3 horizonTintMultiplier{1.0f, 1.0f, 1.0f};
    glm::vec3 cloudTint{0.96f, 0.97f, 1.0f};
    glm::vec2 windDirectionXZ{1.0f, 0.0f};
    float windSpeed = 2.0f;
};

inline constexpr float kWeatherTransitionSeconds = 22.0f;
inline constexpr std::array<WeatherPreset, 5> kWeatherPattern{{
    {
        .type = WeatherType::Clear,
        .durationSeconds = 140.0f,
        .cloudCoverage = 0.14f,
        .rainIntensity = 0.0f,
        .sunOcclusion = 0.05f,
        .skyTintMultiplier = {1.08f, 1.02f, 1.16f},
        .horizonTintMultiplier = {1.02f, 0.95f, 1.10f},
        .cloudTint = {0.98f, 0.98f, 0.99f},
        .windDirectionXZ = {0.58f, 0.81f},
        .windSpeed = 2.4f,
    },
    {
        .type = WeatherType::Cloudy,
        .durationSeconds = 90.0f,
        .cloudCoverage = 0.52f,
        .rainIntensity = 0.0f,
        .sunOcclusion = 0.36f,
        .skyTintMultiplier = {0.86f, 0.93f, 1.08f},
        .horizonTintMultiplier = {0.90f, 0.86f, 1.04f},
        .cloudTint = {0.93f, 0.93f, 0.94f},
        .windDirectionXZ = {-0.35f, 0.94f},
        .windSpeed = 3.5f,
    },
    {
        .type = WeatherType::Rain,
        .durationSeconds = 115.0f,
        .cloudCoverage = 0.92f,
        .rainIntensity = 1.0f,
        .sunOcclusion = 0.80f,
        .skyTintMultiplier = {0.61f, 0.74f, 1.04f},
        .horizonTintMultiplier = {0.70f, 0.78f, 1.03f},
        .cloudTint = {0.82f, 0.82f, 0.84f},
        .windDirectionXZ = {-0.12f, 0.99f},
        .windSpeed = 6.2f,
    },
    {
        .type = WeatherType::Cloudy,
        .durationSeconds = 82.0f,
        .cloudCoverage = 0.64f,
        .rainIntensity = 0.18f,
        .sunOcclusion = 0.48f,
        .skyTintMultiplier = {0.94f, 0.80f, 0.96f},
        .horizonTintMultiplier = {0.98f, 0.84f, 0.90f},
        .cloudTint = {0.90f, 0.90f, 0.92f},
        .windDirectionXZ = {0.92f, -0.11f},
        .windSpeed = 4.1f,
    },
    {
        .type = WeatherType::Clear,
        .durationSeconds = 135.0f,
        .cloudCoverage = 0.16f,
        .rainIntensity = 0.0f,
        .sunOcclusion = 0.03f,
        .skyTintMultiplier = {1.12f, 0.94f, 0.98f},
        .horizonTintMultiplier = {1.05f, 0.90f, 0.95f},
        .cloudTint = {0.99f, 0.98f, 0.99f},
        .windDirectionXZ = {-0.64f, 0.58f},
        .windSpeed = 2.0f,
    },
}};

inline constexpr float kWeatherCycleDurationSeconds =
    kWeatherPattern[0].durationSeconds
    + kWeatherPattern[1].durationSeconds
    + kWeatherPattern[2].durationSeconds
    + kWeatherPattern[3].durationSeconds
    + kWeatherPattern[4].durationSeconds;

[[nodiscard]] inline float wrapPositive(const float value, const float modulus)
{
    const float wrapped = std::fmod(value, modulus);
    return wrapped < 0.0f ? wrapped + modulus : wrapped;
}

[[nodiscard]] inline glm::vec2 normalizeOrFallback(const glm::vec2& vector, const glm::vec2& fallback)
{
    const float lengthSquared = glm::dot(vector, vector);
    return lengthSquared > 0.0f ? glm::normalize(vector) : fallback;
}

[[nodiscard]] inline WeatherSample blendPresets(
    const WeatherPreset& currentPreset,
    const WeatherPreset& nextPreset,
    const float blendFactor,
    const float elapsedSeconds,
    const float stateElapsedSeconds)
{
    const float clampedBlendFactor = glm::clamp(blendFactor, 0.0f, 1.0f);
    WeatherSample sample;
    sample.type = clampedBlendFactor < 0.5f ? currentPreset.type : nextPreset.type;
    sample.nextType = nextPreset.type;
    sample.elapsedSeconds = elapsedSeconds;
    sample.stateElapsedSeconds = stateElapsedSeconds;
    sample.transitionProgress = clampedBlendFactor;
    sample.cloudCoverage = glm::mix(currentPreset.cloudCoverage, nextPreset.cloudCoverage, clampedBlendFactor);
    sample.rainIntensity = glm::mix(currentPreset.rainIntensity, nextPreset.rainIntensity, clampedBlendFactor);
    sample.sunOcclusion = glm::mix(currentPreset.sunOcclusion, nextPreset.sunOcclusion, clampedBlendFactor);
    sample.skyTintMultiplier =
        glm::mix(currentPreset.skyTintMultiplier, nextPreset.skyTintMultiplier, clampedBlendFactor);
    sample.horizonTintMultiplier =
        glm::mix(currentPreset.horizonTintMultiplier, nextPreset.horizonTintMultiplier, clampedBlendFactor);
    sample.cloudTint = glm::mix(currentPreset.cloudTint, nextPreset.cloudTint, clampedBlendFactor);
    sample.windDirectionXZ = normalizeOrFallback(
        glm::mix(currentPreset.windDirectionXZ, nextPreset.windDirectionXZ, clampedBlendFactor),
        currentPreset.windDirectionXZ);
    sample.windSpeed = glm::mix(currentPreset.windSpeed, nextPreset.windSpeed, clampedBlendFactor);
    return sample;
}
}  // namespace weather_detail

class WeatherSystem
{
  public:
    static constexpr float kTransitionDurationSeconds = weather_detail::kWeatherTransitionSeconds;
    static constexpr float kWeatherCycleDurationSeconds = weather_detail::kWeatherCycleDurationSeconds;

    WeatherSystem() = default;
    explicit WeatherSystem(const float elapsedSeconds) : elapsedSeconds_(wrapCycleSeconds(elapsedSeconds)) {}

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

    [[nodiscard]] WeatherSample sample() const
    {
        return sampleAtElapsedSeconds(elapsedSeconds_);
    }

    [[nodiscard]] static float wrapCycleSeconds(const float elapsedSeconds)
    {
        return weather_detail::wrapPositive(elapsedSeconds, kWeatherCycleDurationSeconds);
    }

    [[nodiscard]] static WeatherSample sampleAtElapsedSeconds(const float elapsedSeconds)
    {
        const float wrappedElapsedSeconds = wrapCycleSeconds(elapsedSeconds);
        float stageStartSeconds = 0.0f;

        for (std::size_t stageIndex = 0; stageIndex < weather_detail::kWeatherPattern.size(); ++stageIndex)
        {
            const weather_detail::WeatherPreset& currentPreset = weather_detail::kWeatherPattern[stageIndex];
            const float stageEndSeconds = stageStartSeconds + currentPreset.durationSeconds;
            if (wrappedElapsedSeconds >= stageEndSeconds)
            {
                stageStartSeconds = stageEndSeconds;
                continue;
            }

            const float stageElapsedSeconds = wrappedElapsedSeconds - stageStartSeconds;
            const std::size_t nextStageIndex = (stageIndex + 1U) % weather_detail::kWeatherPattern.size();
            const weather_detail::WeatherPreset& nextPreset = weather_detail::kWeatherPattern[nextStageIndex];
            const float transitionStartSeconds =
                glm::max(0.0f, currentPreset.durationSeconds - kTransitionDurationSeconds);
            const float transitionProgress = currentPreset.durationSeconds > transitionStartSeconds
                ? (stageElapsedSeconds - transitionStartSeconds)
                    / (currentPreset.durationSeconds - transitionStartSeconds)
                : 0.0f;

            WeatherSample sample = weather_detail::blendPresets(
                currentPreset,
                nextPreset,
                transitionProgress > 0.0f ? transitionProgress : 0.0f,
                wrappedElapsedSeconds,
                stageElapsedSeconds);
            sample.cycleProgress = wrappedElapsedSeconds / kWeatherCycleDurationSeconds;
            return sample;
        }

        WeatherSample sample = weather_detail::blendPresets(
            weather_detail::kWeatherPattern.front(),
            weather_detail::kWeatherPattern.front(),
            0.0f,
            wrappedElapsedSeconds,
            0.0f);
        sample.cycleProgress = wrappedElapsedSeconds / kWeatherCycleDurationSeconds;
        return sample;
    }

  private:
    float elapsedSeconds_ = 0.0f;
};
}  // namespace vibecraft::game
