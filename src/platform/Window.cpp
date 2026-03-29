#include "vibecraft/platform/Window.hpp"

#include "vibecraft/core/Logger.hpp"

namespace vibecraft::platform
{
Window::~Window()
{
    destroy();
}

bool Window::create(const std::string& title, const std::uint32_t width, const std::uint32_t height)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO))
    {
        core::logError(SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        static_cast<int>(width),
        static_cast<int>(height),
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

    if (window_ == nullptr)
    {
        core::logError(SDL_GetError());
        SDL_Quit();
        return false;
    }

    return true;
}

void Window::destroy()
{
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void Window::pollEvents(InputState& inputState)
{
    inputState.beginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            inputState.quitRequested = true;
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE && !event.key.repeat)
            {
                inputState.escapePressed = true;
            }
            if (event.key.scancode == SDL_SCANCODE_TAB && !event.key.repeat)
            {
                inputState.tabPressed = true;
            }
            if (event.key.scancode == SDL_SCANCODE_BACKSPACE && !event.key.repeat)
            {
                inputState.backspacePressed = true;
            }
            break;

        case SDL_EVENT_TEXT_INPUT:
            if (event.text.text != nullptr)
            {
                inputState.textInputUtf8 += event.text.text;
            }
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            inputState.windowFocused = false;
            inputState.releaseMouseRequested = true;
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            inputState.windowFocused = true;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            inputState.windowSizeChanged = true;
            break;

        case SDL_EVENT_MOUSE_MOTION:
            inputState.mouseDeltaX += event.motion.xrel;
            inputState.mouseDeltaY += event.motion.yrel;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                inputState.leftMousePressed = true;
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
            {
                inputState.rightMousePressed = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                inputState.leftMouseClicked = true;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            inputState.mouseWheelDeltaY += static_cast<int>(event.wheel.y);
            break;

        default:
            break;
        }
    }

    if (window_ != nullptr)
    {
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
        inputState.windowFocused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
    }

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    static_cast<void>(SDL_GetMouseState(&mouseX, &mouseY));
    if (window_ != nullptr)
    {
        int windowWidth = 0;
        int windowHeight = 0;
        int pixelWidth = 0;
        int pixelHeight = 0;
        SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
        SDL_GetWindowSizeInPixels(window_, &pixelWidth, &pixelHeight);
        if (windowWidth > 0 && windowHeight > 0 && pixelWidth > 0 && pixelHeight > 0)
        {
            mouseX *= static_cast<float>(pixelWidth) / static_cast<float>(windowWidth);
            mouseY *= static_cast<float>(pixelHeight) / static_cast<float>(windowHeight);
        }
    }
    inputState.mouseWindowX = mouseX;
    inputState.mouseWindowY = mouseY;
}

void Window::setTextInputActive(const bool enabled)
{
    if (window_ == nullptr)
    {
        return;
    }
    if (enabled)
    {
        static_cast<void>(SDL_StartTextInput(window_));
    }
    else
    {
        static_cast<void>(SDL_StopTextInput(window_));
    }
}

bool Window::setRelativeMouseMode(const bool enabled) const
{
    return window_ != nullptr && SDL_SetWindowRelativeMouseMode(window_, enabled);
}

void* Window::nativeWindowHandle() const
{
    if (window_ == nullptr)
    {
        return nullptr;
    }

    const SDL_PropertiesID properties = SDL_GetWindowProperties(window_);

#if defined(SDL_PLATFORM_WINDOWS)
    return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(SDL_PLATFORM_MACOS)
    return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
    return nullptr;
#endif
}

std::uint32_t Window::width() const
{
    int width = 0;
    SDL_GetWindowSizeInPixels(window_, &width, nullptr);
    return static_cast<std::uint32_t>(width);
}

std::uint32_t Window::height() const
{
    int height = 0;
    SDL_GetWindowSizeInPixels(window_, nullptr, &height);
    return static_cast<std::uint32_t>(height);
}
}  // namespace vibecraft::platform
