#pragma once

#include <cstdint>

#include <bgfx/bgfx.h>
#include <glm/vec3.hpp>

#include "vibecraft/app/Application.hpp"
#include "vibecraft/platform/InputState.hpp"
#include "vibecraft/platform/Window.hpp"
#include "vibecraft/game/WeatherSystem.hpp"

namespace vibecraft::app
{
struct MenuUiMetrics
{
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    std::uint32_t windowWidth = 1;
    std::uint32_t windowHeight = 1;
    std::uint16_t textWidth = 100;
    std::uint16_t textHeight = 30;
};

[[nodiscard]] MenuUiMetrics computeMenuUiMetrics(
    const platform::Window& window,
    const platform::InputState& inputState,
    const bgfx::Stats* stats);
[[nodiscard]] game::WeatherType nextWeatherType(game::WeatherType type);
[[nodiscard]] float weatherElapsedSecondsForType(game::WeatherType type);
[[nodiscard]] const char* spawnPresetLabel(SpawnPreset preset);
[[nodiscard]] const char* spawnBiomeTargetLabel(SpawnBiomeTarget target);
[[nodiscard]] SpawnBiomeTarget nextSpawnBiomeTarget(SpawnBiomeTarget target);
[[nodiscard]] const char* difficultyGradeLabel(DifficultyGrade difficulty);
[[nodiscard]] DifficultyGrade nextDifficultyGrade(DifficultyGrade difficulty);
[[nodiscard]] game::MobSpawnSettings mobSpawnSettingsForDifficulty(DifficultyGrade difficulty);
[[nodiscard]] glm::vec3 preferredBiomePreviewProbePosition(
    SpawnBiomeTarget target,
    const glm::vec3& fallbackCameraPosition);
[[nodiscard]] SpawnPreset nextSpawnPreset(SpawnPreset preset);
}  // namespace vibecraft::app
