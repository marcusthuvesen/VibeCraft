#include "vibecraft/app/Application.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/vec3.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/Chunk.hpp"

namespace vibecraft::app
{
void Application::processPausedInput()
{
    const bgfx::Stats* const stats = bgfx::getStats();
    const MenuUiMetrics menuUiMetrics = computeMenuUiMetrics(window_, inputState_, stats);

    if (!inputState_.leftMouseClicked)
    {
        return;
    }

    if (pauseGameSettingsOpen_)
    {
        const int hit = render::Renderer::hitTestPauseGameSettingsMenu(
            menuUiMetrics.mouseX,
            menuUiMetrics.mouseY,
            menuUiMetrics.windowWidth,
            menuUiMetrics.windowHeight,
            menuUiMetrics.textWidth,
            menuUiMetrics.textHeight);
        if (hit >= 0)
        {
            soundEffects_.playUiClick();
        }
        switch (hit)
        {
        case 0:
            pauseGameSettingsOpen_ = false;
            pauseMenuNotice_.clear();
            break;
        case 1:
            mobSpawningEnabled_ = !mobSpawningEnabled_;
            if (!mobSpawningEnabled_)
            {
                mobSpawnSystem_.clearAllMobs();
            }
            pauseMenuNotice_ = mobSpawningEnabled_ ? "Mob spawning enabled." : "Mob spawning disabled.";
            break;
        case 2:
            spawnBiomeTarget_ = nextSpawnBiomeTarget(spawnBiomeTarget_);
            pauseMenuNotice_ = fmt::format(
                "Biome target: {}. Select Travel now to move.",
                spawnBiomeTargetLabel(spawnBiomeTarget_));
            break;
        case 3:
        {
            const glm::vec3 biomePreviewProbePosition =
                preferredBiomePreviewProbePosition(spawnBiomeTarget_, camera_.position());
            const world::ChunkCoord biomePreviewChunk = world::worldToChunkCoord(
                static_cast<int>(std::floor(biomePreviewProbePosition.x)),
                static_cast<int>(std::floor(biomePreviewProbePosition.z)));
            world_.generateMissingChunksAround(
                terrainGenerator_,
                biomePreviewChunk,
                kStreamingSettings.bootstrapChunkRadius,
                static_cast<std::size_t>((kStreamingSettings.bootstrapChunkRadius * 2 + 1)
                                         * (kStreamingSettings.bootstrapChunkRadius * 2 + 1)));
            spawnFeetPosition_ = resolveSpawnFeetPosition(
                world_,
                terrainGenerator_,
                SpawnPreset::Origin,
                spawnBiomeTarget_,
                biomePreviewProbePosition,
                kPlayerMovementSettings.standingColliderHeight);
            playerFeetPosition_ = spawnFeetPosition_;
            verticalVelocity_ = 0.0f;
            accumulatedFallDistance_ = 0.0f;
            jumpWasHeld_ = false;
            autoJumpCooldownSeconds_ = 0.0f;
            isGrounded_ = isGroundedAtFeetPosition(
                world_,
                playerFeetPosition_,
                kPlayerMovementSettings.standingColliderHeight);
            camera_.setPosition(
                playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
            playerHazards_ = samplePlayerHazards(
                world_,
                playerFeetPosition_,
                kPlayerMovementSettings.standingColliderHeight,
                kPlayerMovementSettings.standingEyeHeight);
            pauseMenuNotice_ = fmt::format("Travelled to {}.", spawnBiomeTargetLabel(spawnBiomeTarget_));
            break;
        }
        case 4:
        {
            const game::WeatherType currentWeatherType = weatherSystem_.sample().type;
            const game::WeatherType nextType = nextWeatherType(currentWeatherType);
            weatherSystem_.setElapsedSeconds(weatherElapsedSecondsForType(nextType));
            pauseMenuNotice_ = fmt::format("Weather set to {}.", weatherLabel(nextType));
            break;
        }
        default:
            break;
        }
        return;
    }

    if (pauseSoundSettingsOpen_)
    {
        const int hit = render::Renderer::hitTestPauseSoundMenu(
            menuUiMetrics.mouseX,
            menuUiMetrics.mouseY,
            menuUiMetrics.windowWidth,
            menuUiMetrics.windowHeight,
            menuUiMetrics.textWidth,
            menuUiMetrics.textHeight);
        bool playedUiClick = false;
        if (hit >= 0)
        {
            soundEffects_.playUiClick();
            playedUiClick = true;
        }
        switch (hit)
        {
        case 0:
            pauseSoundSettingsOpen_ = false;
            pauseMenuNotice_ = "Sound settings saved.";
            break;
        default:
        {
            if (const std::optional<float> sliderValue = render::Renderer::pauseSoundSliderValueFromMouse(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    true);
                sliderValue.has_value())
            {
                if (!playedUiClick)
                {
                    soundEffects_.playUiClick();
                    playedUiClick = true;
                }
                musicVolume_ = std::clamp(*sliderValue, 0.0f, 1.0f);
                musicDirector_.setMasterGain(musicVolume_);
            }
            else if (const std::optional<float> sliderValue = render::Renderer::pauseSoundSliderValueFromMouse(
                         menuUiMetrics.mouseX,
                         menuUiMetrics.mouseY,
                         menuUiMetrics.windowWidth,
                         menuUiMetrics.windowHeight,
                         menuUiMetrics.textWidth,
                         menuUiMetrics.textHeight,
                         false);
                     sliderValue.has_value())
            {
                if (!playedUiClick)
                {
                    soundEffects_.playUiClick();
                    playedUiClick = true;
                }
                sfxVolume_ = std::clamp(*sliderValue, 0.0f, 1.0f);
                soundEffects_.setMasterGain(sfxVolume_);
            }
            break;
        }
        }
        saveAudioPrefs();
        return;
    }

    const int hit = render::Renderer::hitTestPauseMenu(
        menuUiMetrics.mouseX,
        menuUiMetrics.mouseY,
        menuUiMetrics.windowWidth,
        menuUiMetrics.windowHeight,
        menuUiMetrics.textWidth,
        menuUiMetrics.textHeight);
    if (hit >= 0)
    {
        soundEffects_.playUiClick();
    }
    switch (hit)
    {
    case 0:
        gameScreen_ = GameScreen::Playing;
        pauseSoundSettingsOpen_ = false;
        pauseGameSettingsOpen_ = false;
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        pauseMenuNotice_.clear();
        inputState_.clearMouseMotion();
        break;
    case 1:
        pauseSoundSettingsOpen_ = true;
        pauseMenuNotice_.clear();
        break;
    case 2:
        pauseGameSettingsOpen_ = true;
        pauseMenuNotice_.clear();
        break;
    case 3:
        if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
        {
            static_cast<void>(saveActiveSingleplayerWorld(true));
        }
        stopMultiplayerSessions();
        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
        window_.setTextInputActive(false);
        sessionPlayTimeSeconds_ = 0.0f;
        gameScreen_ = GameScreen::MainMenu;
        pauseSoundSettingsOpen_ = false;
        pauseGameSettingsOpen_ = false;
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
        pauseMenuNotice_.clear();
        singleplayerLoadState_ = {};
        unloadActiveSingleplayerWorld();
        refreshSingleplayerWorldList();
        break;
    case 4:
        inputState_.quitRequested = true;
        break;
    default:
        break;
    }
}
}  // namespace vibecraft::app
