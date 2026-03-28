#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"

#include <algorithm>
#include <string>

namespace vibecraft::render
{


int Renderer::hitTestMainMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (windowWidth == 0 || windowHeight == 0 || textWidth == 0 || textHeight == 0)
    {
        return -1;
    }

    const int col = static_cast<int>(mouseX * static_cast<float>(textWidth) / static_cast<float>(windowWidth));
    const int row = static_cast<int>(mouseY * static_cast<float>(textHeight) / static_cast<float>(windowHeight));
    const int tw = static_cast<int>(textWidth);
    const int clampedCol = std::clamp(col, 0, tw - 1);
    const int clampedRow = std::clamp(row, 0, static_cast<int>(textHeight) - 1);

    const int th = static_cast<int>(textHeight);
    const detail::MainMenuComputedLayout menu = detail::computeMainMenuLayout(tw, th);

    for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex)
    {
        const int row0 = menu.buttonTopRows[static_cast<std::size_t>(buttonIndex)];
        if (clampedRow >= row0 && clampedRow <= row0 + detail::MainMenuLayout::kButtonLineCount - 1
            && clampedCol >= menu.centerCol && clampedCol <= menu.centerCol + menu.outerWidth - 1)
        {
            return buttonIndex;
        }
    }

    if (clampedRow == menu.iconHintsRow && menu.centerCol >= 7)
    {
        if (clampedCol >= menu.centerCol - 6 && clampedCol <= menu.centerCol - 4)
        {
            return 5;
        }
    }

    if (clampedRow == menu.iconHintsRow)
    {
        const int aLeft = menu.centerCol + menu.outerWidth - 3;
        if (clampedCol >= aLeft && clampedCol <= aLeft + 2)
        {
            return 6;
        }
    }

    return -1;
}

int Renderer::hitTestPauseMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (windowWidth == 0 || windowHeight == 0 || textWidth == 0 || textHeight == 0)
    {
        return -1;
    }

    const int col = static_cast<int>(mouseX * static_cast<float>(textWidth) / static_cast<float>(windowWidth));
    const int row = static_cast<int>(mouseY * static_cast<float>(textHeight) / static_cast<float>(windowHeight));
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const int clampedCol = std::clamp(col, 0, tw - 1);
    const int clampedRow = std::clamp(row, 0, th - 1);

    constexpr int kWide = 32;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex)
    {
        const int row0 = 9 + buttonIndex * 4;
        if (clampedRow >= row0 && clampedRow <= row0 + 2 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
        {
            return buttonIndex;
        }
    }

    return -1;
}

int Renderer::hitTestPauseGameSettingsMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (windowWidth == 0 || windowHeight == 0 || textWidth == 0 || textHeight == 0)
    {
        return -1;
    }

    const int col = static_cast<int>(mouseX * static_cast<float>(textWidth) / static_cast<float>(windowWidth));
    const int row = static_cast<int>(mouseY * static_cast<float>(textHeight) / static_cast<float>(windowHeight));
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const int clampedCol = std::clamp(col, 0, tw - 1);
    const int clampedRow = std::clamp(row, 0, th - 1);

    constexpr int kWide = 42;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= 19 && clampedRow <= 21 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= 11 && clampedRow <= 13 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 1;
    }

    return -1;
}

int Renderer::hitTestPauseSoundMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (windowWidth == 0 || windowHeight == 0 || textWidth == 0 || textHeight == 0)
    {
        return -1;
    }

    const int col = static_cast<int>(mouseX * static_cast<float>(textWidth) / static_cast<float>(windowWidth));
    const int row = static_cast<int>(mouseY * static_cast<float>(textHeight) / static_cast<float>(windowHeight));
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const int clampedCol = std::clamp(col, 0, tw - 1);
    const int clampedRow = std::clamp(row, 0, th - 1);

    constexpr int kWide = 42;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= 19 && clampedRow <= 21 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= 9 && clampedRow <= 11 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        const int relCol = clampedCol - centerCol;
        if (relCol >= kWide - 9 && relCol <= kWide - 7)
        {
            return 1;
        }
        if (relCol >= kWide - 5 && relCol <= kWide - 3)
        {
            return 2;
        }
    }
    if (clampedRow >= 13 && clampedRow <= 15 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        const int relCol = clampedCol - centerCol;
        if (relCol >= kWide - 9 && relCol <= kWide - 7)
        {
            return 3;
        }
        if (relCol >= kWide - 5 && relCol <= kWide - 3)
        {
            return 4;
        }
    }

    return -1;

} // namespace vibecraft::render
