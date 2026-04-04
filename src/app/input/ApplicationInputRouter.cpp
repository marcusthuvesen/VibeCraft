#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>

namespace vibecraft::app
{
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
    const bool chatOpenKeyDown = inputState_.isKeyDown(SDL_SCANCODE_T);
    const bool chatOpenKeyPressed = chatOpenKeyDown && !chatOpenKeyWasDown_;
    chatOpenKeyWasDown_ = chatOpenKeyDown;
    const bool chatSlashKeyDown = inputState_.isKeyDown(SDL_SCANCODE_SLASH);
    const bool chatSlashKeyPressed = chatSlashKeyDown && !chatSlashKeyWasDown_;
    chatSlashKeyWasDown_ = chatSlashKeyDown;
    const bool chatSubmitKeyDown =
        inputState_.isKeyDown(SDL_SCANCODE_RETURN) || inputState_.isKeyDown(SDL_SCANCODE_KP_ENTER);
    const bool chatSubmitKeyPressed = chatSubmitKeyDown && !chatSubmitKeyWasDown_;
    chatSubmitKeyWasDown_ = chatSubmitKeyDown;

    if (craftingMenuState_.active && (inputState_.escapePressed || craftingKeyPressed))
    {
        closeCraftingMenu();
        return;
    }

    if (gameScreen_ == GameScreen::Playing && chatState_.open)
    {
        processPlayingChatInput(chatSubmitKeyPressed);
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

    if (!craftingMenuState_.active)
    {
        if (chatOpenKeyPressed)
        {
            openChat();
            return;
        }
        if (chatSlashKeyPressed)
        {
            openChat("/");
            return;
        }
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
        const std::uint32_t mouseButtons = SDL_GetMouseState(nullptr, nullptr);
        const bool leftMouseHeld = (mouseButtons & SDL_BUTTON_LMASK) != 0U;
        const render::CraftingUiMode renderMode =
            craftingMenuState_.mode == CraftingMenuState::Mode::Furnace ? render::CraftingUiMode::Furnace
            : craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage ? render::CraftingUiMode::Chest
            : craftingMenuState_.usesWorkbench ? render::CraftingUiMode::Workbench
                                               : render::CraftingUiMode::Inventory;
        const int hoveredCraftingHit = render::Renderer::hitTestCraftingMenu(
            inputState_.mouseWindowX,
            inputState_.mouseWindowY,
            window_.width(),
            window_.height(),
            renderMode,
            craftingMenuState_.usesWorkbench,
            craftingMenuState_.bagStartRow);
        if (!leftMouseHeld)
        {
            craftingDragActive_ = false;
            craftingDragLastHit_ = -1;
        }
        else if (!craftingDragActive_)
        {
            if (inputState_.leftMousePressed)
            {
                handleCraftingMenuClick();
                craftingDragActive_ = true;
                craftingDragLastHit_ = hoveredCraftingHit;
            }
        }
        else if (hoveredCraftingHit != craftingDragLastHit_)
        {
            handleCraftingMenuClick();
            craftingDragLastHit_ = hoveredCraftingHit;
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
