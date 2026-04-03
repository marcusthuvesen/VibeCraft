#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <unordered_set>
#include <vector>

#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/ApplicationChunkStreaming.hpp"
#include "vibecraft/app/ApplicationCombat.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/app/ApplicationMultiplayerLog.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ApplicationAmbientLife.hpp"
#include "vibecraft/app/ApplicationBotanyRuntime.hpp"
#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"
#include "vibecraft/app/ApplicationOxygenRuntime.hpp"
#include "vibecraft/app/ApplicationTerraformingRuntime.hpp"
#include "vibecraft/app/OxygenItems.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/audio/RuntimeAudioRoot.hpp"
#include "vibecraft/core/Logger.hpp"
#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"
#include "vibecraft/platform/LocalNetworkAddress.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] bool envFlagEnabled(const char* const value)
{
    if (value == nullptr)
    {
        return false;
    }

    std::string normalized(value);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

struct BiomeVisualProfile
{
    glm::vec3 skyTarget{0.62f, 0.75f, 0.96f};
    glm::vec3 horizonTarget{0.76f, 0.88f, 1.0f};
    glm::vec3 cloudTarget{0.96f, 0.97f, 1.0f};
    glm::vec3 sunTintTarget{1.0f, 0.97f, 0.92f};
    glm::vec3 moonTintTarget{0.62f, 0.72f, 1.0f};
    glm::vec3 terrainHazeTarget{0.67f, 0.80f, 0.95f};
    glm::vec3 terrainBounceTarget{0.92f, 1.0f, 0.94f};
    float skyBlend = 0.0f;
    float horizonBlend = 0.0f;
    float cloudBlend = 0.0f;
    float sunBlend = 0.0f;
    float moonBlend = 0.0f;
    float skyBrightness = 1.0f;
    float horizonBrightness = 1.0f;
    float cloudBrightness = 1.0f;
    float terrainHazeBlend = 0.0f;
    float terrainHazeStrength = 0.18f;
    float terrainSaturation = 1.05f;
};

[[nodiscard]] BiomeVisualProfile biomeVisualProfile(const world::SurfaceBiome biome)
{
    using B = world::SurfaceBiome;
    switch (biome)
    {
    case B::Desert:
        // Vanilla-like desert: mostly normal blue sky with warmer bounce light.
        return {
            .skyTarget = {0.54f, 0.74f, 0.98f},
            .horizonTarget = {0.82f, 0.88f, 0.98f},
            .cloudTarget = {0.96f, 0.97f, 1.0f},
            .sunTintTarget = {1.0f, 0.96f, 0.88f},
            .moonTintTarget = {0.68f, 0.78f, 0.96f},
            .terrainHazeTarget = {0.84f, 0.84f, 0.78f},
            .terrainBounceTarget = {0.96f, 0.88f, 0.70f},
            .skyBlend = 0.06f,
            .horizonBlend = 0.08f,
            .cloudBlend = 0.06f,
            .sunBlend = 0.06f,
            .moonBlend = 0.10f,
            .skyBrightness = 1.02f,
            .horizonBrightness = 1.01f,
            .cloudBrightness = 1.01f,
            .terrainHazeBlend = 0.08f,
            .terrainHazeStrength = 0.04f,
            .terrainSaturation = 0.92f,
        };
    case B::SnowyPlains:
    case B::SnowyTaiga:
        // Snow stays bright, but the palette remains close to Minecraft's normal daytime.
        return {
            .skyTarget = {0.56f, 0.76f, 0.99f},
            .horizonTarget = {0.84f, 0.90f, 1.0f},
            .cloudTarget = {0.96f, 0.98f, 1.0f},
            .sunTintTarget = {0.97f, 0.98f, 1.0f},
            .moonTintTarget = {0.72f, 0.82f, 1.0f},
            .terrainHazeTarget = {0.88f, 0.92f, 0.98f},
            .terrainBounceTarget = {0.92f, 0.96f, 1.0f},
            .skyBlend = 0.08f,
            .horizonBlend = 0.10f,
            .cloudBlend = 0.08f,
            .sunBlend = 0.08f,
            .moonBlend = 0.14f,
            .skyBrightness = 1.02f,
            .horizonBrightness = 1.03f,
            .cloudBrightness = 1.02f,
            .terrainHazeBlend = 0.06f,
            .terrainHazeStrength = 0.04f,
            .terrainSaturation = 0.84f,
        };
    case B::Jungle:
    case B::SparseJungle:
    case B::BambooJungle:
        // Jungle keeps a greener bounce light without drifting into stylized fog.
        return {
            .skyTarget = {0.52f, 0.74f, 0.96f},
            .horizonTarget = {0.76f, 0.84f, 0.94f},
            .cloudTarget = {0.94f, 0.96f, 0.98f},
            .sunTintTarget = {0.98f, 0.97f, 0.90f},
            .moonTintTarget = {0.66f, 0.78f, 0.98f},
            .terrainHazeTarget = {0.72f, 0.80f, 0.86f},
            .terrainBounceTarget = {0.74f, 0.86f, 0.72f},
            .skyBlend = 0.10f,
            .horizonBlend = 0.12f,
            .cloudBlend = 0.10f,
            .sunBlend = 0.08f,
            .moonBlend = 0.14f,
            .skyBrightness = 1.00f,
            .horizonBrightness = 1.01f,
            .cloudBrightness = 1.01f,
            .terrainHazeBlend = 0.07f,
            .terrainHazeStrength = 0.04f,
            .terrainSaturation = 0.90f,
        };
    case B::Forest:
    case B::BirchForest:
    case B::DarkForest:
    case B::Taiga:
    case B::Plains:
        // Temperate plains/forest palette closer to Minecraft daytime.
        return {
            .skyTarget = {0.50f, 0.72f, 0.96f},
            .horizonTarget = {0.76f, 0.84f, 0.96f},
            .cloudTarget = {0.94f, 0.96f, 1.0f},
            .sunTintTarget = {1.0f, 0.97f, 0.90f},
            .moonTintTarget = {0.66f, 0.78f, 0.96f},
            .terrainHazeTarget = {0.72f, 0.80f, 0.90f},
            .terrainBounceTarget = {0.84f, 0.88f, 0.74f},
            .skyBlend = 0.08f,
            .horizonBlend = 0.10f,
            .cloudBlend = 0.08f,
            .sunBlend = 0.08f,
            .moonBlend = 0.14f,
            .skyBrightness = 1.02f,
            .horizonBrightness = 1.01f,
            .cloudBrightness = 1.01f,
            .terrainHazeBlend = 0.04f,
            .terrainHazeStrength = 0.03f,
            .terrainSaturation = 0.76f,
        };
    default:
        return biomeVisualProfile(B::Forest);
    }
}

[[nodiscard]] glm::vec3 applyBiomeColorGrade(
    const glm::vec3& baseColor,
    const glm::vec3& targetColor,
    const float blend,
    const float brightness)
{
    const float t = std::clamp(blend, 0.0f, 1.0f);
    glm::vec3 mixed = glm::mix(baseColor, targetColor, t) * std::max(0.0f, brightness);
    const float luma = glm::dot(mixed, glm::vec3(0.299f, 0.587f, 0.114f));
    const glm::vec3 gray(luma);
    const glm::vec3 chroma = mixed - gray;
    // Slight chroma lift on stronger biome grades keeps skies and light tints vivid (less muddy gray).
    mixed = glm::clamp(gray + chroma * (1.0f + 0.11f * t), glm::vec3(0.0f), glm::vec3(1.0f));
    return mixed;
}

}

bool Application::initialize()
{
    core::initializeLogger();

    autoStartSingleplayerRequested_ = envFlagEnabled(std::getenv("VIBECRAFT_AUTOSTART_SINGLEPLAYER"));
    autoStartCreatesNewWorld_ = envFlagEnabled(std::getenv("VIBECRAFT_AUTOSTART_NEW_WORLD"));
    autoStartSingleplayerConsumed_ = false;

    if (!window_.create("VibeCraft", kWindowSettings.width, kWindowSettings.height))
    {
        return false;
    }

    // Keep startup fast by deferring world creation until the player chooses Singleplayer.
    {
        vibecraft::world::World::ChunkMap emptyChunks;
        world_.replaceChunks(std::move(emptyChunks));
    }

    multiplayerAddress_ = "127.0.0.1";
    loadMultiplayerPrefs();
    loadAudioPrefs();
    refreshSingleplayerWorldList();

    if (!renderer_.initialize(window_.nativeWindowHandle(), window_.width(), window_.height()))
    {
        core::logError("Failed to initialize bgfx.");
        return false;
    }

    const std::filesystem::path minecraftAudioRoot = audio::resolveMinecraftAudioRoot();
    core::logInfo(fmt::format("Minecraft audio assets: {}", minecraftAudioRoot.generic_string()));
    audio::logMinecraftAudioPackDiagnostics(minecraftAudioRoot);
    if (!sharedAudioOutput_.initialize())
    {
        core::logWarning("Shared audio output failed to open; music and SFX are disabled.");
    }
    else
    {
        if (!musicDirector_.initialize(sharedAudioOutput_.musicStream(), minecraftAudioRoot))
        {
            core::logWarning("Music system failed to initialize; continuing without music.");
        }
        if (!soundEffects_.initialize(sharedAudioOutput_.sfxStream(), minecraftAudioRoot))
        {
            core::logWarning("SFX system failed to initialize; continuing without sound effects.");
        }
    }
    musicDirector_.setMasterGain(musicVolume_);
    soundEffects_.setMasterGain(sfxVolume_);

    camera_.addYawPitch(90.0f, 0.0f);
    dayNightCycle_.setElapsedSeconds(150.0f);
    weatherSystem_.setElapsedSeconds(0.0f);

    gameScreen_ = GameScreen::MainMenu;
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    mainMenuNotice_ = singleplayerWorlds_.empty()
        ? "No worlds yet. Click Singleplayer to choose New World."
        : "Click Singleplayer to choose a saved world or create a new one.";
    applyDefaultHotbarLoadout(hotbarSlots_, selectedHotbarIndex_);
    return true;
}

int Application::run()
{
    std::uint64_t lastCounter = SDL_GetPerformanceCounter();
    const std::uint64_t frequency = SDL_GetPerformanceFrequency();

    while (!inputState_.quitRequested)
    {
        const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
        const float deltaTimeSeconds =
            static_cast<float>(currentCounter - lastCounter) / static_cast<float>(frequency);
        lastCounter = currentCounter;

        window_.pollEvents(inputState_);
        processInput(deltaTimeSeconds);
        update(deltaTimeSeconds);
    }

    if (!activeSingleplayerWorldFolderName_.empty() && multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        static_cast<void>(saveActiveSingleplayerWorld(false));
    }
    saveAudioPrefs();
    stopMultiplayerSessions();
    musicDirector_.shutdown();
    soundEffects_.shutdown();
    sharedAudioOutput_.shutdown();
    renderer_.shutdown();
    return 0;
}

void Application::update(const float deltaTimeSeconds)
{
    if ((gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
        && multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        world_.tickFluids(multiplayerMode_ == MultiplayerRuntimeMode::Host ? 64 : 96);
    }

    dayNightCycle_.advanceSeconds(deltaTimeSeconds);
    weatherSystem_.advanceSeconds(deltaTimeSeconds);
    const game::DayNightSample dayNightSample = dayNightCycle_.sample();
    const game::WeatherSample weatherSample = weatherSystem_.sample();
    const world::SurfaceBiome playerSurfaceBiome =
        (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
        ? terrainGenerator_.surfaceBiomeAt(
            static_cast<int>(std::floor(playerFeetPosition_.x)),
            static_cast<int>(std::floor(playerFeetPosition_.z)))
        : world::SurfaceBiome::Forest;
    const BiomeVisualProfile biomeProfile = biomeVisualProfile(playerSurfaceBiome);
    const glm::vec3 clearSkyTint = dayNightSample.skyTint * weatherSample.skyTintMultiplier;
    const glm::vec3 clearHorizonTint = dayNightSample.horizonTint * weatherSample.horizonTintMultiplier;
    const glm::vec3 weatherSkyTint =
        glm::mix(clearSkyTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.28f);
    const glm::vec3 weatherHorizonTint =
        glm::mix(clearHorizonTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.18f);
    const glm::vec3 skyTint = applyBiomeColorGrade(
        weatherSkyTint,
        biomeProfile.skyTarget,
        biomeProfile.skyBlend,
        biomeProfile.skyBrightness);
    const glm::vec3 horizonTint = applyBiomeColorGrade(
        weatherHorizonTint,
        biomeProfile.horizonTarget,
        biomeProfile.horizonBlend,
        biomeProfile.horizonBrightness);
    const glm::vec3 cloudTint = applyBiomeColorGrade(
        weatherSample.cloudTint,
        biomeProfile.cloudTarget,
        biomeProfile.cloudBlend,
        biomeProfile.cloudBrightness);
    const glm::vec3 terrainHazeBase = glm::mix(weatherHorizonTint, weatherSkyTint, 0.42f);
    const float sunLightScale = 1.0f - weatherSample.sunOcclusion * 0.55f;
    const float moonLightScale = 1.0f - weatherSample.cloudCoverage * 0.20f;
    const float visibleSunScale = 1.0f - weatherSample.sunOcclusion * 0.35f;
    const float visibleMoonScale = 1.0f - weatherSample.cloudCoverage * 0.12f;
    float finalSunVisibility = glm::clamp(dayNightSample.sunVisibility * visibleSunScale, 0.0f, 1.0f);
    float finalMoonVisibility = glm::clamp(dayNightSample.moonVisibility * visibleMoonScale, 0.0f, 1.0f);
    constexpr float kMinCelestialVisibility = 0.12f;
    if (std::max(finalSunVisibility, finalMoonVisibility) < kMinCelestialVisibility)
    {
        // Keep one celestial body present through horizon crossover.
        if (dayNightSample.sunDirection.y >= dayNightSample.moonDirection.y)
        {
            finalSunVisibility = kMinCelestialVisibility;
        }
        else
        {
            finalMoonVisibility = kMinCelestialVisibility;
        }
    }
    const glm::vec3 sunLightTint = applyBiomeColorGrade(
        dayNightSample.sunLightTint,
        biomeProfile.sunTintTarget,
        biomeProfile.sunBlend,
        1.0f);
    const glm::vec3 moonLightTint = applyBiomeColorGrade(
        dayNightSample.moonLightTint,
        biomeProfile.moonTintTarget,
        biomeProfile.moonBlend,
        1.0f);
    const glm::vec3 terrainHazeColor = applyBiomeColorGrade(
        terrainHazeBase,
        biomeProfile.terrainHazeTarget,
        biomeProfile.terrainHazeBlend,
        1.0f);
    const glm::vec3 terrainBounceTint = glm::clamp(biomeProfile.terrainBounceTarget, glm::vec3(0.0f), glm::vec3(1.25f));

    const float frameTimeMs = deltaTimeSeconds * 1000.0f;
    if (!frameTimeInitialized_)
    {
        smoothedFrameTimeMs_ = frameTimeMs;
        frameTimeInitialized_ = true;
    }
    else
    {
        constexpr float kFrameTimeSmoothingAlpha = 0.1f;
        smoothedFrameTimeMs_ =
            smoothedFrameTimeMs_ + (frameTimeMs - smoothedFrameTimeMs_) * kFrameTimeSmoothingAlpha;
    }

    if (gameScreen_ == GameScreen::Playing)
    {
        sessionPlayTimeSeconds_ += deltaTimeSeconds;
    }

    if (singleplayerLoadState_.active)
    {
        updateSingleplayerLoad();
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        mainMenuTimeSeconds_ += deltaTimeSeconds;
    }

    updateMultiplayer(deltaTimeSeconds);

    if (inputState_.windowSizeChanged && window_.width() != 0 && window_.height() != 0)
    {
        renderer_.resize(window_.width(), window_.height());
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        syncWorldData();
        if (gameScreen_ == GameScreen::Playing && multiplayerMode_ != MultiplayerRuntimeMode::Client)
        {
            static_cast<void>(tickLocalTerraforming(
                deltaTimeSeconds,
                world_,
                terrainGenerator_,
                playerFeetPosition_,
                terraformingRuntimeState_));
            static_cast<void>(tickLocalBotany(
                deltaTimeSeconds,
                world_,
                terrainGenerator_,
                playerFeetPosition_,
                botanyRuntimeState_));
        }
        const float currentEyeHeight = std::max(0.0f, camera_.position().y - playerFeetPosition_.y);
        updateDroppedItems(deltaTimeSeconds, currentEyeHeight);
        if (!activeSingleplayerWorldFolderName_.empty() && multiplayerMode_ != MultiplayerRuntimeMode::Client)
        {
            autosaveAccumulatorSeconds_ += deltaTimeSeconds;
            if (autosaveAccumulatorSeconds_ >= 30.0f)
            {
                saveActiveSingleplayerWorld(false);
            }
        }
    }

    // Multiplayer clients use host-replicated mob poses for rendering; skip local mob simulation.
    if (gameScreen_ == GameScreen::Playing && multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        const float healthBeforeMobTick = playerVitals_.health();
        const float armorProtection = equippedArmorProtectionFraction(equipmentSlots_);
        std::vector<glm::vec3> mobTickRemoteFeet;
        std::vector<float> mobTickRemoteHealth;
        std::span<const glm::vec3> mobTickRemoteFeetSpan{};
        std::span<float> mobTickRemoteHealthSpan{};
        if (!remotePlayers_.empty())
        {
            mobTickRemoteFeet.reserve(remotePlayers_.size());
            mobTickRemoteHealth.reserve(remotePlayers_.size());
            for (const RemotePlayerState& remote : remotePlayers_)
            {
                mobTickRemoteFeet.push_back(remote.position);
                mobTickRemoteHealth.push_back(remote.health);
            }
            mobTickRemoteFeetSpan = mobTickRemoteFeet;
            mobTickRemoteHealthSpan = mobTickRemoteHealth;
        }
        mobSpawnSystem_.tick(
            world_,
            terrainGenerator_,
            playerFeetPosition_,
            kPlayerMovementSettings.colliderHalfWidth,
            deltaTimeSeconds,
            dayNightSample.period,
            mobSpawningEnabled_,
            playerVitals_,
            1.0f - armorProtection,
            mobTickRemoteFeetSpan,
            mobTickRemoteHealthSpan,
            playerVitals_.maxHealth(),
            1.0f);
        if (!remotePlayers_.empty())
        {
            for (std::size_t i = 0; i < remotePlayers_.size(); ++i)
            {
                remotePlayers_[i].health = mobTickRemoteHealth[i];
            }
        }
        if (!creativeModeEnabled_ && playerVitals_.health() + 0.001f < healthBeforeMobTick)
        {
            soundEffects_.playPlayerHurt();
        }
    }

    std::optional<world::RaycastHit> raycastHit;
    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        raycastHit = world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    }

    render::FrameDebugData frameDebugData;
    buildFrameDebugData(
        deltaTimeSeconds,
        dayNightSample,
        weatherSample,
        playerSurfaceBiome,
        raycastHit,
        frameDebugData);

    audio::MusicContext musicContext = audio::MusicContext::OverworldDay;
    if (gameScreen_ == GameScreen::MainMenu)
    {
        musicContext = audio::MusicContext::Menu;
    }
    else if (playerHazards_.headSubmergedInWater)
    {
        musicContext = audio::MusicContext::Underwater;
    }
    else if (dayNightSample.period == game::TimeOfDayPeriod::Dusk
             || dayNightSample.period == game::TimeOfDayPeriod::Night)
    {
        musicContext = audio::MusicContext::OverworldNight;
    }
    musicDirector_.update(deltaTimeSeconds, musicContext);

    renderer_.renderFrame(
        frameDebugData,
        render::CameraFrameData{
            .position = camera_.position(),
            .forward = camera_.forward(),
            .up = camera_.up(),
            .skyTint = skyTint,
            .horizonTint = horizonTint,
            .sunDirection = dayNightSample.sunDirection,
            .moonDirection = dayNightSample.moonDirection,
            .sunLightTint = sunLightTint * sunLightScale,
            .moonLightTint = moonLightTint * moonLightScale,
            .cloudTint = cloudTint,
            .terrainHazeColor = terrainHazeColor,
            .terrainBounceTint = terrainBounceTint,
            .weatherWindDirectionXZ = weatherSample.windDirectionXZ,
            .sunVisibility = finalSunVisibility,
            .moonVisibility = finalMoonVisibility,
            .cloudCoverage = weatherSample.cloudCoverage,
            .rainIntensity = weatherSample.rainIntensity,
            .weatherTimeSeconds = weatherSample.elapsedSeconds,
            .weatherWindSpeed = weatherSample.windSpeed,
            .terrainHazeStrength = biomeProfile.terrainHazeStrength,
            .terrainSaturation = biomeProfile.terrainSaturation,
        });
}

void Application::processInput(const float deltaTimeSeconds)
{
    const bool allowMainMenuPointerInputWhileUnfocused =
        gameScreen_ == GameScreen::MainMenu
        && (inputState_.leftMousePressed || inputState_.leftMouseClicked);
    if (!inputState_.windowFocused && !allowMainMenuPointerInputWhileUnfocused)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        return;
    }

    const bool f3Down = inputState_.isKeyDown(SDL_SCANCODE_F3);
    if (f3Down && !debugF3KeyWasDown_
        && (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused))
    {
        showWorldOriginGuides_ = !showWorldOriginGuides_;
    }
    debugF3KeyWasDown_ = f3Down;

    const bool craftingKeyDown = inputState_.isKeyDown(SDL_SCANCODE_E);
    const bool craftingKeyPressed = craftingKeyDown && !craftingKeyWasDown_;
    craftingKeyWasDown_ = craftingKeyDown;

    if (craftingMenuState_.active && (inputState_.escapePressed || craftingKeyPressed))
    {
        closeCraftingMenu();
        return;
    }

    if (inputState_.escapePressed)
    {
        if (gameScreen_ == GameScreen::Playing)
        {
            gameScreen_ = GameScreen::Paused;
            pauseSoundSettingsOpen_ = false;
            pauseGameSettingsOpen_ = false;
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
            pauseMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::Paused)
        {
            if (pauseGameSettingsOpen_)
            {
                pauseGameSettingsOpen_ = false;
                pauseMenuNotice_.clear();
            }
            else if (pauseSoundSettingsOpen_)
            {
                pauseSoundSettingsOpen_ = false;
                pauseMenuNotice_ = "Sound settings saved.";
                saveAudioPrefs();
            }
            else
            {
                gameScreen_ = GameScreen::Playing;
                mouseCaptured_ = true;
                window_.setRelativeMouseMode(true);
                pauseMenuNotice_.clear();
                inputState_.clearMouseMotion();
            }
        }
        else if (gameScreen_ == GameScreen::MainMenu && mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None
                 && !mainMenuSoundSettingsOpen_)
        {
            if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Hub)
            {
                mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
            }
            else
            {
                mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
            }
            window_.setTextInputActive(false);
            mainMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::MainMenu && mainMenuSoundSettingsOpen_)
        {
            mainMenuSoundSettingsOpen_ = false;
            mainMenuNotice_ = "Sound settings saved.";
            saveAudioPrefs();
        }
        else if (gameScreen_ == GameScreen::MainMenu && mainMenuSingleplayerPickerOpen_)
        {
            mainMenuSingleplayerPickerOpen_ = false;
            mainMenuNotice_.clear();
        }
    }

    if (inputState_.releaseMouseRequested)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }

    if (inputState_.captureMouseRequested && gameScreen_ == GameScreen::Playing)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }
    if (inputState_.tabPressed && gameScreen_ == GameScreen::Playing)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        processMainMenuInput();
        return;
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        processPausedInput();
        return;
    }

    if (gameScreen_ != GameScreen::Playing)
    {
        return;
    }

    if (craftingMenuState_.active)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
        constexpr std::size_t kBagColumns = 9;
        constexpr std::size_t kVisibleBagRows = 3;
        const std::size_t totalBagRows = bagSlots_.size() / kBagColumns;
        const std::size_t maxBagStartRow =
            totalBagRows > kVisibleBagRows ? totalBagRows - kVisibleBagRows : 0;
        if (inputState_.mouseWheelDeltaY != 0)
        {
            const int scrollDelta = inputState_.mouseWheelDeltaY;
            if (scrollDelta > 0)
            {
                const std::size_t step = static_cast<std::size_t>(scrollDelta);
                craftingMenuState_.bagStartRow = craftingMenuState_.bagStartRow > step
                    ? craftingMenuState_.bagStartRow - step
                    : 0;
            }
            else
            {
                craftingMenuState_.bagStartRow = std::min<std::size_t>(
                    maxBagStartRow,
                    craftingMenuState_.bagStartRow + static_cast<std::size_t>(-scrollDelta));
            }
        }
        if (inputState_.leftMouseClicked)
        {
            handleCraftingMenuClick();
        }
        if (inputState_.rightMousePressed)
        {
            handleCraftingMenuRightClick();
        }
        return;
    }

    if (processPlayingMovementInput(deltaTimeSeconds, craftingKeyPressed))
    {
        return;
    }

    processPlayingActionInput(deltaTimeSeconds);
}

}  // namespace vibecraft::app
