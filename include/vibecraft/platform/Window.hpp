#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

#include "vibecraft/platform/InputState.hpp"

namespace vibecraft::platform
{
class Window
{
  public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const std::string& title, std::uint32_t width, std::uint32_t height);
    void destroy();
    void pollEvents(InputState& inputState);
    bool setRelativeMouseMode(bool enabled) const;

    [[nodiscard]] SDL_Window* sdlWindow() const
    {
        return window_;
    }

    [[nodiscard]] void* nativeWindowHandle() const;
    [[nodiscard]] std::uint32_t width() const;
    [[nodiscard]] std::uint32_t height() const;

  private:
    SDL_Window* window_ = nullptr;
};
}  // namespace vibecraft::platform
