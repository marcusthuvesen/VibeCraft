#pragma once

#include <SDL3/SDL.h>

namespace vibecraft::platform
{
struct InputState
{
    bool quitRequested = false;
    bool captureMouseRequested = false;
    bool releaseMouseRequested = false;
    bool leftMousePressed = false;
    bool rightMousePressed = false;
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;

    void beginFrame()
    {
        captureMouseRequested = false;
        releaseMouseRequested = false;
        leftMousePressed = false;
        rightMousePressed = false;
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
