#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"

#include <algorithm>

namespace vibecraft::app
{
MenuUiMetrics computeMenuUiMetrics(
    const platform::Window& window,
    const platform::InputState& inputState,
    const bgfx::Stats* const stats)
{
    const std::uint32_t pixelWidth = std::max(window.width(), 1U);
    const std::uint32_t pixelHeight = std::max(window.height(), 1U);
    const std::uint32_t logicalWidth = std::max(window.logicalWidth(), 1U);
    const std::uint32_t logicalHeight = std::max(window.logicalHeight(), 1U);
    const float xScale = static_cast<float>(logicalWidth) / static_cast<float>(pixelWidth);
    const float yScale = static_cast<float>(logicalHeight) / static_cast<float>(pixelHeight);
    const std::uint16_t rawTextWidth =
        stats != nullptr && stats->textWidth > 0 ? stats->textWidth : static_cast<std::uint16_t>(100);
    const std::uint16_t rawTextHeight =
        stats != nullptr && stats->textHeight > 0 ? stats->textHeight : static_cast<std::uint16_t>(30);

    MenuUiMetrics metrics{};
    metrics.mouseX = inputState.mouseWindowX * xScale;
    metrics.mouseY = inputState.mouseWindowY * yScale;
    metrics.windowWidth = logicalWidth;
    metrics.windowHeight = logicalHeight;
    metrics.textWidth = rawTextWidth;
    metrics.textHeight = rawTextHeight;
    return metrics;
}

game::WeatherType nextWeatherType(const game::WeatherType type)
{
    switch (type)
    {
    case game::WeatherType::Clear:
        return game::WeatherType::Cloudy;
    case game::WeatherType::Cloudy:
        return game::WeatherType::Rain;
    case game::WeatherType::Rain:
    default:
        return game::WeatherType::Clear;
    }
}

float weatherElapsedSecondsForType(const game::WeatherType type)
{
    float elapsed = 0.0f;
    for (const auto& preset : game::weather_detail::kWeatherPattern)
    {
        if (preset.type == type)
        {
            return elapsed;
        }
        elapsed += preset.durationSeconds;
    }
    return 0.0f;
}

const char* spawnPresetLabel(const SpawnPreset preset)
{
    switch (preset)
    {
    case SpawnPreset::North:
        return "Northlands";
    case SpawnPreset::South:
        return "Southlands";
    case SpawnPreset::East:
        return "Eastlands";
    case SpawnPreset::West:
        return "Westlands";
    case SpawnPreset::Origin:
    default:
        return "Origin";
    }
}

const char* spawnBiomeTargetLabel(const SpawnBiomeTarget target)
{
    switch (target)
    {
    case SpawnBiomeTarget::Temperate:
        return "Forest";
    case SpawnBiomeTarget::Sandy:
        return "Desert";
    case SpawnBiomeTarget::Snowy:
        return "Snowy";
    case SpawnBiomeTarget::Jungle:
        return "Jungle";
    case SpawnBiomeTarget::Any:
    default:
        return "Any";
    }
}

SpawnBiomeTarget nextSpawnBiomeTarget(const SpawnBiomeTarget target)
{
    switch (target)
    {
    case SpawnBiomeTarget::Any:
        return SpawnBiomeTarget::Temperate;
    case SpawnBiomeTarget::Temperate:
        return SpawnBiomeTarget::Sandy;
    case SpawnBiomeTarget::Sandy:
        return SpawnBiomeTarget::Snowy;
    case SpawnBiomeTarget::Snowy:
        return SpawnBiomeTarget::Jungle;
    case SpawnBiomeTarget::Jungle:
    default:
        return SpawnBiomeTarget::Any;
    }
}

const char* difficultyGradeLabel(const DifficultyGrade difficulty)
{
    switch (difficulty)
    {
    case DifficultyGrade::Easy:
        return "Easy";
    case DifficultyGrade::Hard:
        return "Hard";
    case DifficultyGrade::Nightmare:
        return "Nightmare";
    case DifficultyGrade::Normal:
    default:
        return "Normal";
    }
}

DifficultyGrade nextDifficultyGrade(const DifficultyGrade difficulty)
{
    switch (difficulty)
    {
    case DifficultyGrade::Easy:
        return DifficultyGrade::Normal;
    case DifficultyGrade::Normal:
        return DifficultyGrade::Hard;
    case DifficultyGrade::Hard:
        return DifficultyGrade::Nightmare;
    case DifficultyGrade::Nightmare:
    default:
        return DifficultyGrade::Easy;
    }
}

game::MobSpawnSettings mobSpawnSettingsForDifficulty(const DifficultyGrade difficulty)
{
    game::MobSpawnSettings settings{};
    switch (difficulty)
    {
    case DifficultyGrade::Easy:
        settings.maxHostileMobsNearPlayer = 5;
        settings.maxPassiveMobsNearPlayer = 10;
        settings.spawnAttemptIntervalSeconds = 3.6f;
        settings.hostileTorchExclusionRadius = 12.0f;
        break;
    case DifficultyGrade::Hard:
        settings.maxHostileMobsNearPlayer = 18;
        settings.maxPassiveMobsNearPlayer = 16;
        settings.spawnAttemptIntervalSeconds = 1.55f;
        settings.despawnHorizontalDistance = 84.0f;
        settings.hostileTorchExclusionRadius = 8.0f;
        break;
    case DifficultyGrade::Nightmare:
        settings.maxHostileMobsNearPlayer = 30;
        settings.maxPassiveMobsNearPlayer = 18;
        settings.spawnAttemptIntervalSeconds = 0.8f;
        settings.spawnMinHorizontalDistance = 14.0f;
        settings.spawnMaxHorizontalDistance = 56.0f;
        settings.despawnHorizontalDistance = 96.0f;
        settings.hostileTorchExclusionRadius = 5.0f;
        break;
    case DifficultyGrade::Normal:
    default:
        break;
    }
    return settings;
}

glm::vec3 preferredBiomePreviewProbePosition(
    const SpawnBiomeTarget target,
    const glm::vec3& fallbackCameraPosition)
{
    constexpr float kPreviewOffset = 4096.0f;
    switch (target)
    {
    case SpawnBiomeTarget::Temperate:
        return glm::vec3(kPreviewOffset, fallbackCameraPosition.y, 0.0f);
    case SpawnBiomeTarget::Sandy:
        return glm::vec3(-kPreviewOffset, fallbackCameraPosition.y, 0.0f);
    case SpawnBiomeTarget::Snowy:
        return glm::vec3(0.0f, fallbackCameraPosition.y, kPreviewOffset);
    case SpawnBiomeTarget::Jungle:
        return glm::vec3(0.0f, fallbackCameraPosition.y, -kPreviewOffset);
    case SpawnBiomeTarget::Any:
    default:
        return fallbackCameraPosition;
    }
}

SpawnPreset nextSpawnPreset(const SpawnPreset preset)
{
    switch (preset)
    {
    case SpawnPreset::Origin:
        return SpawnPreset::North;
    case SpawnPreset::North:
        return SpawnPreset::South;
    case SpawnPreset::South:
        return SpawnPreset::East;
    case SpawnPreset::East:
        return SpawnPreset::West;
    case SpawnPreset::West:
    default:
        return SpawnPreset::Origin;
    }
}
}  // namespace vibecraft::app
