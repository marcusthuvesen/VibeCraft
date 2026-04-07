#include "vibecraft/app/Application.hpp"

#include <algorithm>

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include "vibecraft/app/StartupFlow.hpp"
#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"

namespace vibecraft::app
{
void Application::processMainMenuInput()
{
    if (mouseCaptured_)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }
    processJoinMenuTextInput();
    processDisplayNameTextInput();
    if (singleplayerLoadState_.active)
    {
        return;
    }
    const bgfx::Stats* const stats = bgfx::getStats();
    const MenuUiMetrics menuUiMetrics = computeMenuUiMetrics(window_, inputState_, stats);
    if (mainMenuBootLoading_)
    {
        const bool menuTextReady = stats != nullptr && stats->textWidth > 0 && stats->textHeight > 0;
        const bool menuLogoReady = renderer_.menuLogoWidthPx() > 0 && renderer_.menuLogoHeightPx() > 0;
        if (!menuTextReady || !menuLogoReady)
        {
            return;
        }
        refreshSingleplayerWorldList();
        mainMenuBootLoading_ = false;
        mainMenuNotice_.clear();
        return;
    }
    const auto startSingleplayerFromRequest = [this](const SingleplayerStartRequest request)
    {
        pendingHostStartAfterWorldLoad_ = false;
        switch (resolveSingleplayerStartAction(request, !singleplayerWorlds_.empty()))
        {
        case SingleplayerStartAction::StartSelectedWorld:
            beginSingleplayerLoad();
            return true;
        case SingleplayerStartAction::CreateAndStartWorld:
            if (createNewSingleplayerWorld())
            {
                beginSingleplayerLoad();
                return true;
            }
            return false;
        case SingleplayerStartAction::MissingSavedWorld:
            mainMenuSingleplayerPickerOpen_ = true;
            mainMenuNotice_ = "No saved worlds yet. Choose Start new world.";
            return false;
        }
        return false;
    };
    if (autoStartSingleplayerRequested_ && !autoStartSingleplayerConsumed_)
    {
        autoStartSingleplayerConsumed_ = true;
        if (!startSingleplayerFromRequest(
                autoStartCreatesNewWorld_ ? SingleplayerStartRequest::CreateNewWorld
                                          : SingleplayerStartRequest::LoadSavedWorld))
        {
            if (autoStartCreatesNewWorld_)
            {
                mainMenuNotice_ = "Auto-start singleplayer failed.";
            }
        }
        return;
    }
    const bool creativeToggleKeyDown = inputState_.isKeyDown(SDL_SCANCODE_C);
    const bool previousWorldKeyDown = inputState_.isKeyDown(SDL_SCANCODE_LEFTBRACKET);
    const bool newWorldKeyDown = inputState_.isKeyDown(SDL_SCANCODE_N);
    const bool nextWorldKeyDown = inputState_.isKeyDown(SDL_SCANCODE_RIGHTBRACKET);
    const bool spawnPresetToggleKeyDown = inputState_.isKeyDown(SDL_SCANCODE_V);
    if (creativeToggleKeyDown && !creativeToggleKeyWasDown_)
    {
        creativeModeEnabled_ = !creativeModeEnabled_;
        mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
    }
    if (previousWorldKeyDown && !previousWorldKeyWasDown_)
    {
        cycleSelectedSingleplayerWorld(-1);
        mainMenuSingleplayerPickerOpen_ = true;
    }
    if (newWorldKeyDown && !newWorldKeyWasDown_)
    {
        if (mainMenuSingleplayerPickerOpen_)
        {
            startSingleplayerFromRequest(SingleplayerStartRequest::CreateNewWorld);
        }
        else
        {
            [[maybe_unused]] const bool createdNewWorld = createNewSingleplayerWorld();
        }
    }
    if (nextWorldKeyDown && !nextWorldKeyWasDown_)
    {
        cycleSelectedSingleplayerWorld(1);
        mainMenuSingleplayerPickerOpen_ = true;
    }
    if (spawnPresetToggleKeyDown && !spawnPresetToggleKeyWasDown_)
    {
        spawnPreset_ = nextSpawnPreset(spawnPreset_);
        mainMenuNotice_ = fmt::format("Spawn preset: {}", spawnPresetLabel(spawnPreset_));
    }
    creativeToggleKeyWasDown_ = creativeToggleKeyDown;
    previousWorldKeyWasDown_ = previousWorldKeyDown;
    newWorldKeyWasDown_ = newWorldKeyDown;
    nextWorldKeyWasDown_ = nextWorldKeyDown;
    spawnPresetToggleKeyWasDown_ = spawnPresetToggleKeyDown;

    if (inputState_.leftMouseClicked)
    {
        if (mainMenuSoundSettingsOpen_)
        {
            const int hit = render::Renderer::hitTestPauseSoundMenu(
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
                mainMenuSoundSettingsOpen_ = false;
                mainMenuNotice_ = "Sound settings saved.";
                break;
            case 1:
                musicVolume_ = std::max(0.0f, musicVolume_ - 0.05f);
                musicDirector_.setMasterGain(musicVolume_);
                break;
            case 2:
                musicVolume_ = std::min(1.0f, musicVolume_ + 0.05f);
                musicDirector_.setMasterGain(musicVolume_);
                break;
            case 3:
                sfxVolume_ = std::max(0.0f, sfxVolume_ - 0.05f);
                soundEffects_.setMasterGain(sfxVolume_);
                break;
            case 4:
                sfxVolume_ = std::min(1.0f, sfxVolume_ + 0.05f);
                soundEffects_.setMasterGain(sfxVolume_);
                break;
            default:
                break;
            }
            saveAudioPrefs();
        }
        else if (mainMenuOptionsOpen_)
        {
            const int hit = render::Renderer::hitTestMainMenuOptions(
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
                mainMenuDisplayNameEditing_ = false;
                mainMenuOptionsOpen_ = false;
                window_.setTextInputActive(false);
                mainMenuNotice_.clear();
                savePlayerPrefs();
                break;
            case 1:
                mainMenuDisplayNameEditing_ = false;
                window_.setTextInputActive(false);
                mainMenuOptionsOpen_ = false;
                mainMenuSoundSettingsOpen_ = true;
                mainMenuNotice_.clear();
                break;
            case 2:
                mainMenuDisplayNameEditing_ = true;
                window_.setTextInputActive(true);
                break;
            default:
                break;
            }
        }
        else if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None)
        {
            const int joinSlotsForMpLayout = static_cast<int>(std::min(joinPresets_.size(), std::size_t(3)));
            const int mainMenuContentTopBias = vibecraft::render::detail::mainMenuLogoReservedDbgRows(
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
            const int multiplayerRowShift = render::Renderer::multiplayerMenuRowShift(
                menuUiMetrics.textHeight,
                mainMenuMultiplayerPanel_,
                mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join ? joinSlotsForMpLayout : 0,
                mainMenuContentTopBias);
            if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Hub)
            {
                const int hit = render::Renderer::hitTestMainMenuMultiplayerHub(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                if (hit >= 0)
                {
                    soundEffects_.playUiClick();
                }
                switch (hit)
                {
                case 0:
                    refreshDetectedLanAddress();
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Host;
                    mainMenuNotice_.clear();
                    break;
                case 1:
                    loadJoinPresets();
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Join;
                    joinFocusedField_ = 0;
                    window_.setTextInputActive(true);
                    mainMenuNotice_.clear();
                    break;
                case 2:
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
                    mainMenuNotice_.clear();
                    break;
                default:
                    break;
                }
            }
            else if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Host)
            {
                const int hit = render::Renderer::hitTestMainMenuMultiplayerHost(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                if (hit >= 0)
                {
                    soundEffects_.playUiClick();
                }
                switch (hit)
                {
                case 0:
                    tryStartHostFromMenu();
                    break;
                case 1:
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                    mainMenuNotice_.clear();
                    break;
                default:
                    break;
                }
            }
            else if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join)
            {
                const int presetSlots = joinSlotsForMpLayout;
                const int hit = render::Renderer::hitTestMainMenuMultiplayerJoin(
                    menuUiMetrics.mouseX,
                    menuUiMetrics.mouseY,
                    menuUiMetrics.windowWidth,
                    menuUiMetrics.windowHeight,
                    menuUiMetrics.textWidth,
                    menuUiMetrics.textHeight,
                    presetSlots,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                if (hit >= 0)
                {
                    soundEffects_.playUiClick();
                }
                if (hit >= 0 && hit < presetSlots)
                {
                    applyJoinPreset(joinPresets_[static_cast<std::size_t>(hit)]);
                    tryConnectFromJoinMenu();
                }
                else
                {
                    const int manual = hit - presetSlots;
                    switch (manual)
                    {
                    case 0:
                        joinFocusedField_ = 0;
                        window_.setTextInputActive(true);
                        break;
                    case 1:
                        joinFocusedField_ = 1;
                        window_.setTextInputActive(true);
                        break;
                    case 2:
                        tryConnectFromJoinMenu();
                        break;
                    case 3:
                        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                        window_.setTextInputActive(false);
                        mainMenuNotice_.clear();
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        else if (mainMenuSingleplayerPickerOpen_)
        {
            const int hit = render::Renderer::hitTestMainMenuSingleplayerPanel(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
            if (hit >= 0)
            {
                soundEffects_.playUiClick();
            }
            switch (hit)
            {
            case 0:
                startSingleplayerFromRequest(SingleplayerStartRequest::LoadSavedWorld);
                break;
            case 1:
                startSingleplayerFromRequest(SingleplayerStartRequest::CreateNewWorld);
                break;
            case 2:
                spawnBiomeTarget_ = nextSpawnBiomeTarget(spawnBiomeTarget_);
                mainMenuNotice_ = fmt::format("Biome target: {}", spawnBiomeTargetLabel(spawnBiomeTarget_));
                break;
            case 3:
                mainMenuSingleplayerPickerOpen_ = false;
                mainMenuNotice_.clear();
                break;
            default:
                break;
            }
        }
        else
        {
            const int hit = render::Renderer::hitTestMainMenu(
                menuUiMetrics.mouseX,
                menuUiMetrics.mouseY,
                menuUiMetrics.windowWidth,
                menuUiMetrics.windowHeight,
                menuUiMetrics.textWidth,
                menuUiMetrics.textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
            if (hit >= 0)
            {
                soundEffects_.playUiClick();
            }
            switch (hit)
            {
            case 0:
                refreshSingleplayerWorldList();
                mainMenuSingleplayerPickerOpen_ = true;
                mainMenuNotice_.clear();
                break;
            case 1:
                mainMenuSingleplayerPickerOpen_ = false;
                mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                mainMenuNotice_.clear();
                break;
            case 2:
                creativeModeEnabled_ = !creativeModeEnabled_;
                mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
                break;
            case 3:
                mainMenuOptionsOpen_ = true;
                mainMenuNotice_.clear();
                break;
            case 4:
                inputState_.quitRequested = true;
                break;
            case 5:
                creativeModeEnabled_ = !creativeModeEnabled_;
                mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
                break;
            case 6:
                spawnPreset_ = nextSpawnPreset(spawnPreset_);
                mainMenuNotice_ = fmt::format("Spawn preset: {}", spawnPresetLabel(spawnPreset_));
                break;
            case 7:
                cycleSelectedSingleplayerWorld(-1);
                mainMenuSingleplayerPickerOpen_ = true;
                break;
            case 8:
                startSingleplayerFromRequest(SingleplayerStartRequest::CreateNewWorld);
                break;
            case 9:
                cycleSelectedSingleplayerWorld(1);
                mainMenuSingleplayerPickerOpen_ = true;
                break;
            default:
                break;
            }
        }
    }
}
}  // namespace vibecraft::app
