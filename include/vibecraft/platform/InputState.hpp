#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace vibecraft::platform
{
struct InputState
{
    bool quitRequested = false;
    bool captureMouseRequested = false;
    bool releaseMouseRequested = false;
    bool windowFocused = true;
    bool windowSizeChanged = false;
    bool leftMousePressed = false;
    /// True when left mouse button is released this frame (menu-style click activation).
    bool leftMouseClicked = false;
    bool rightMousePressed = false;
    /// Set on SDL_EVENT_KEY_DOWN for Escape (not key-repeat).
    bool escapePressed = false;
    /// Backspace on key-down (not repeat), for text fields.
    bool backspacePressed = false;
    /// Tab on key-down (not repeat), for UI focus cycling.
    bool tabPressed = false;
    /// UTF-8 text from SDL_EVENT_TEXT_INPUT this frame (after beginFrame).
    std::string textInputUtf8;
    int mouseWheelDeltaY = 0;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
    /// Window-relative cursor position from the last event pump (SDL_GetMouseState).
    float mouseWindowX = 0.0f;
    float mouseWindowY = 0.0f;

    void beginFrame()
    {
        captureMouseRequested = false;
        releaseMouseRequested = false;
        windowSizeChanged = false;
        leftMousePressed = false;
        leftMouseClicked = false;
        rightMousePressed = false;
        escapePressed = false;
        backspacePressed = false;
        tabPressed = false;
        textInputUtf8.clear();
        mouseWheelDeltaY = 0;
        mouseDeltaX = 0.0f;
        mouseDeltaY = 0.0f;
    }

    /// Clears accumulated mouse deltas (e.g. after re-enabling relative mode so menu motion does not steer).
    void clearMouseMotion()
    {
        mouseDeltaX = 0.0f;
        mouseDeltaY = 0.0f;
    }

    [[nodiscard]] bool isKeyDown(const SDL_Scancode scancode) const
    {
        int keyCount = 0;
        const bool* const keyboardState = SDL_GetKeyboardState(&keyCount);
        return keyboardState != nullptr && static_cast<int>(scancode) < keyCount && keyboardState[scancode];
    }
};
}  // namespace vibecraft::platform
