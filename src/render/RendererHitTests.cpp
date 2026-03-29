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

    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
    constexpr int kPitch = detail::PauseMenuLayout::kButtonPitch;
    constexpr int kSpan = detail::PauseMenuLayout::kButtonRowSpan;

    for (int buttonIndex = 0; buttonIndex < detail::PauseMenuLayout::kMainButtonCount; ++buttonIndex)
    {
        const int row0 = firstButtonRow + buttonIndex * kPitch;
        if (clampedRow >= row0 && clampedRow <= row0 + kSpan - 1 && clampedCol >= centerCol
            && clampedCol <= centerCol + kWide - 1)
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

    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    constexpr int kSpan = detail::PauseMenuLayout::kButtonRowSpan;
    const int backRow = detail::PauseMenuLayout::kGameBackButtonRow;
    const int mobRow = detail::PauseMenuLayout::kGameMobButtonRow;

    if (clampedRow >= backRow && clampedRow <= backRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= mobRow && clampedRow <= mobRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
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

    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    constexpr int kSpan = detail::PauseMenuLayout::kButtonRowSpan;
    const int backRow = detail::PauseMenuLayout::kSoundBackButtonRow;
    const int musicRow = detail::PauseMenuLayout::kSoundMusicButtonRow;
    const int sfxRow = detail::PauseMenuLayout::kSoundSfxButtonRow;

    if (clampedRow >= backRow && clampedRow <= backRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= musicRow && clampedRow <= musicRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        if (clampedRow == musicRow + 1)
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
    }
    if (clampedRow >= sfxRow && clampedRow <= sfxRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        if (clampedRow == sfxRow + 1)
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
    const bool useWorkbench,
    const std::size_t bagStartRow)
{
    if (windowWidth == 0 || windowHeight == 0)
    {
        return -1;
    }
    constexpr int kVisibleBagRows = 3;
    constexpr std::size_t kBagColumns = 9;
    const std::size_t maxBagStartRow =
        (FrameDebugData::kBagHudSlotCount / kBagColumns) > kVisibleBagRows
        ? (FrameDebugData::kBagHudSlotCount / kBagColumns) - kVisibleBagRows
        : 0;
    const std::size_t clampedBagStartRow = std::min(bagStartRow, maxBagStartRow);

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

    for (int row = 0; row < kVisibleBagRows; ++row)
    {
        for (int col = 0; col < 9; ++col)
        {
            const float slotX = layout.inventoryOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
            const float slotY = layout.inventoryOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
            if (insideRect(slotX, slotY))
            {
                const std::size_t globalIndex =
                    (clampedBagStartRow + static_cast<std::size_t>(row)) * kBagColumns
                    + static_cast<std::size_t>(col);
                return kCraftingBagHitBase + static_cast<int>(globalIndex);
            }
        }
    }

    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        const float slotX = layout.inventoryOriginX + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
        const float slotY = layout.inventoryOriginY + static_cast<float>(kVisibleBagRows) * (layout.slotSize + layout.slotGap);
        if (insideRect(slotX, slotY))
        {
            return kCraftingHotbarHitBase + slotIndex;
        }
    }

    return -1;
}

}  // namespace vibecraft::render
