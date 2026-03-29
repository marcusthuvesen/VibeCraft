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
        if (clampedRow >= row0 && clampedRow <= row0 + menu.buttonLineCount - 1
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

    constexpr int kWide = 96;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    constexpr int kButtonCount = 5;
    constexpr int kButtonRowSpan = 5;
    constexpr int kButtonGap = 2;
    constexpr int kButtonPitch = kButtonRowSpan + kButtonGap;
    const int totalButtonRows = kButtonCount * kButtonRowSpan + (kButtonCount - 1) * kButtonGap;
    const int firstButtonRow = std::clamp((th - totalButtonRows) / 2, 7, std::max(7, th - totalButtonRows - 2));

    for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex)
    {
        const int row0 = firstButtonRow + buttonIndex * kButtonPitch;
        if (clampedRow >= row0 && clampedRow <= row0 + 4 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
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

    constexpr int kWide = 96;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= 25 && clampedRow <= 29 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= 11 && clampedRow <= 15 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
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

    constexpr int kWide = 96;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= 25 && clampedRow <= 29 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= 9 && clampedRow <= 13 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
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
    if (clampedRow >= 16 && clampedRow <= 20 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
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
}

int Renderer::hitTestMainMenuMultiplayerHub(
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

    constexpr int kWide = detail::MultiplayerMenuLayout::kWide;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= detail::MultiplayerMenuLayout::kHubHostRow
        && clampedRow <= detail::MultiplayerMenuLayout::kHubHostRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHubJoinRow
        && clampedRow <= detail::MultiplayerMenuLayout::kHubJoinRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 1;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHubBackRow
        && clampedRow <= detail::MultiplayerMenuLayout::kHubBackRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 2;
    }

    return -1;
}

int Renderer::hitTestMainMenuMultiplayerHost(
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

    constexpr int kWide = detail::MultiplayerMenuLayout::kWide;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= detail::MultiplayerMenuLayout::kHostStartRow
        && clampedRow <= detail::MultiplayerMenuLayout::kHostStartRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHostBackRow
        && clampedRow <= detail::MultiplayerMenuLayout::kHostBackRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 1;
    }

    return -1;
}

int Renderer::hitTestMainMenuMultiplayerJoin(
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

    constexpr int kWide = detail::MultiplayerMenuLayout::kWide;
    const int centerCol = std::max(0, (tw - kWide) / 2);

    if (clampedRow >= detail::MultiplayerMenuLayout::kJoinAddrFieldRow
        && clampedRow <= detail::MultiplayerMenuLayout::kJoinAddrFieldRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kJoinPortFieldRow
        && clampedRow <= detail::MultiplayerMenuLayout::kJoinPortFieldRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 1;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kJoinConnectRow
        && clampedRow <= detail::MultiplayerMenuLayout::kJoinConnectRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 2;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kJoinBackRow
        && clampedRow <= detail::MultiplayerMenuLayout::kJoinBackRow + 4 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 3;
    }

    return -1;
}

int Renderer::hitTestCraftingMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const bool useWorkbench)
{
    if (windowWidth == 0 || windowHeight == 0)
    {
        return -1;
    }

    const detail::CraftingOverlayLayoutPx layout =
        detail::computeCraftingOverlayLayoutPx(windowWidth, windowHeight, useWorkbench);
    if (layout.slotSize <= 0.0f)
    {
        return -1;
    }

    const auto insideRect = [&](const float x0, const float y0)
    {
        return mouseX >= x0 && mouseX <= x0 + layout.slotSize
            && mouseY >= y0 && mouseY <= y0 + layout.slotSize;
    };

    const int craftingColumns = useWorkbench ? 3 : 2;
    const int craftingRows = useWorkbench ? 3 : 2;
    for (int row = 0; row < craftingRows; ++row)
    {
        for (int col = 0; col < craftingColumns; ++col)
        {
            const float slotX = layout.craftingOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
            const float slotY = layout.craftingOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
            if (insideRect(slotX, slotY))
            {
                return kCraftingGridHitBase + row * 3 + col;
            }
        }
    }

    if (insideRect(layout.resultSlotX, layout.resultSlotY))
    {
        return kCraftingResultHit;
    }

    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        const float slotX = layout.inventoryOriginX + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
        const float slotY = layout.inventoryOriginY + 9.0f * (layout.slotSize + layout.slotGap);
        if (insideRect(slotX, slotY))
        {
            return kCraftingHotbarHitBase + slotIndex;
        }
    }

    for (int row = 0; row < 9; ++row)
    {
        for (int col = 0; col < 9; ++col)
        {
            const float slotX = layout.inventoryOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
            const float slotY = layout.inventoryOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
            if (insideRect(slotX, slotY))
            {
                return kCraftingBagHitBase + row * 9 + col;
            }
        }
    }

    return -1;
}

}  // namespace vibecraft::render
