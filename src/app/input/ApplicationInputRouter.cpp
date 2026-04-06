#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>

#include "vibecraft/app/PauseMenuInputPolicy.hpp"

namespace vibecraft::app
{
void Application::processInput(const float deltaTimeSeconds)
{
    const bool allowMainMenuPointerInputWhileUnfocused =
        gameScreen_ == GameScreen::MainMenu
        && (inputState_.leftMousePressed || inputState_.leftMouseClicked);
    const bool allowPausedPointerInputWhileUnfocused =
        gameScreen_ == GameScreen::Paused
        && shouldAllowPausedPointerInputWhileUnfocused(
            mouseCaptured_,
            inputState_.leftMousePressed,
            inputState_.leftMouseClicked);
    // Chest, furnace, workbench, and inventory crafting: same as pause — process the refocus click
    // instead of dropping a frame behind SDL focus / window-flag lag.
    const bool allowCraftingPointerInputWhileUnfocused =
        gameScreen_ == GameScreen::Playing
        && craftingMenuState_.active
        && shouldAllowPausedPointerInputWhileUnfocused(
            mouseCaptured_,
            inputState_.leftMousePressed,
            inputState_.leftMouseClicked);
    if (!inputState_.windowFocused && !allowMainMenuPointerInputWhileUnfocused
        && !allowPausedPointerInputWhileUnfocused && !allowCraftingPointerInputWhileUnfocused)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        return;
    }

    // Apply before Esc handling so a focus-loss release cannot undo resume capture in the same frame.
    if (inputState_.releaseMouseRequested)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }

    if (inputState_.windowFocusGainedThisFrame)
    {
        if (gameScreen_ == GameScreen::Paused)
        {
            // Clicking another app can drop the matching mouse-up; reset so pause menu clicks work again.
            pauseMenuAwaitingMouseRelease_ = false;
        }
        if (gameScreen_ == GameScreen::Playing && craftingMenuState_.active)
        {
            // Avoid stale relative deltas if focus was lost while a menu had the cursor.
            inputState_.clearMouseMotion();
        }
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
    const bool chatAutocompleteKeyDown = inputState_.isKeyDown(SDL_SCANCODE_TAB);
    const bool chatAutocompleteKeyPressed = chatAutocompleteKeyDown && !chatAutocompleteKeyWasDown_;
    chatAutocompleteKeyWasDown_ = chatAutocompleteKeyDown;
    const bool chatHistoryUpKeyDown = inputState_.isKeyDown(SDL_SCANCODE_UP);
    const bool chatHistoryUpKeyPressed = chatHistoryUpKeyDown && !chatHistoryUpKeyWasDown_;
    chatHistoryUpKeyWasDown_ = chatHistoryUpKeyDown;
    const bool chatHistoryDownKeyDown = inputState_.isKeyDown(SDL_SCANCODE_DOWN);
    const bool chatHistoryDownKeyPressed = chatHistoryDownKeyDown && !chatHistoryDownKeyWasDown_;
    chatHistoryDownKeyWasDown_ = chatHistoryDownKeyDown;
    const bool chatCursorLeftKeyDown = inputState_.isKeyDown(SDL_SCANCODE_LEFT);
    const bool chatCursorLeftKeyPressed = chatCursorLeftKeyDown && !chatCursorLeftKeyWasDown_;
    chatCursorLeftKeyWasDown_ = chatCursorLeftKeyDown;
    const bool chatCursorRightKeyDown = inputState_.isKeyDown(SDL_SCANCODE_RIGHT);
    const bool chatCursorRightKeyPressed = chatCursorRightKeyDown && !chatCursorRightKeyWasDown_;
    chatCursorRightKeyWasDown_ = chatCursorRightKeyDown;
    const bool chatDeleteKeyDown = inputState_.isKeyDown(SDL_SCANCODE_DELETE);
    const bool chatDeleteKeyPressed = chatDeleteKeyDown && !chatDeleteKeyWasDown_;
    chatDeleteKeyWasDown_ = chatDeleteKeyDown;
    const bool chatHomeKeyDown = inputState_.isKeyDown(SDL_SCANCODE_HOME);
    const bool chatHomeKeyPressed = chatHomeKeyDown && !chatHomeKeyWasDown_;
    chatHomeKeyWasDown_ = chatHomeKeyDown;
    const bool chatEndKeyDown = inputState_.isKeyDown(SDL_SCANCODE_END);
    const bool chatEndKeyPressed = chatEndKeyDown && !chatEndKeyWasDown_;
    chatEndKeyWasDown_ = chatEndKeyDown;
    const bool dropKeyDown = inputState_.isKeyDown(SDL_SCANCODE_Q);
    const bool dropKeyPressed = dropKeyDown && !dropKeyWasDown_;
    dropKeyWasDown_ = dropKeyDown;

    if (craftingMenuState_.active && (inputState_.escapePressed || craftingKeyPressed))
    {
        closeCraftingMenu();
        return;
    }

    if (gameScreen_ == GameScreen::Playing && chatState_.open)
    {
        processPlayingChatInput(ChatInputIntent{
            .submit = chatSubmitKeyPressed,
            .autocomplete = chatAutocompleteKeyPressed,
            .historyPrev = chatHistoryUpKeyPressed,
            .historyNext = chatHistoryDownKeyPressed,
            .moveCursorLeft = chatCursorLeftKeyPressed,
            .moveCursorRight = chatCursorRightKeyPressed,
            .moveCursorHome = chatHomeKeyPressed,
            .moveCursorEnd = chatEndKeyPressed,
            .deleteForward = chatDeleteKeyPressed,
        });
        return;
    }

    if (inputState_.escapePressed)
    {
        if (gameScreen_ == GameScreen::Playing)
        {
            gameScreen_ = GameScreen::Paused;
            pauseSoundSettingsOpen_ = false;
            pauseGameSettingsOpen_ = false;
            pauseMenuAwaitingMouseRelease_ = true;
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
            pauseMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::Paused)
        {
            if (pauseGameSettingsOpen_)
            {
                pauseGameSettingsOpen_ = false;
                pauseMenuAwaitingMouseRelease_ = true;
                pauseMenuNotice_.clear();
            }
            else if (pauseSoundSettingsOpen_)
            {
                pauseSoundSettingsOpen_ = false;
                pauseMenuAwaitingMouseRelease_ = true;
                pauseMenuNotice_ = "Sound settings saved.";
                saveAudioPrefs();
            }
            else
            {
                gameScreen_ = GameScreen::Playing;
                pauseMenuAwaitingMouseRelease_ = false;
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
        if (dropKeyPressed)
        {
            dropSingleItemFromSlotInFront(hotbarSlots_[selectedHotbarIndex_], 2.0f);
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
        // Some devices can intermittently miss either down or up edges. Treat either edge as
        // one primary interaction so pickup/place both remain reliable in all crafting UIs.
        if (inputState_.leftMousePressed || inputState_.leftMouseClicked)
        {
            applyCraftingMenuPrimaryInteraction(hoveredCraftingHit);
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
