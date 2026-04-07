#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace vibecraft::render
{
namespace
{
struct PixelRect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

struct TextGridMetrics
{
    float charWidthPx = 1.0f;
    float charHeightPx = 1.0f;
};

[[nodiscard]] TextGridMetrics makeTextGridMetrics(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    return {
        .charWidthPx = std::max(1.0f, static_cast<float>(windowWidth) / static_cast<float>(std::max(textWidth, static_cast<std::uint16_t>(1)))),
        .charHeightPx = std::max(1.0f, static_cast<float>(windowHeight) / static_cast<float>(std::max(textHeight, static_cast<std::uint16_t>(1)))),
    };
}

[[nodiscard]] PixelRect gridRectFromChars(
    const TextGridMetrics& metrics,
    const int col,
    const int row,
    const int widthChars,
    const int heightRows)
{
    const float x0 = std::floor(static_cast<float>(std::max(0, col)) * metrics.charWidthPx);
    const float y0 = std::floor(static_cast<float>(std::max(0, row)) * metrics.charHeightPx);
    const float x1 = std::ceil(static_cast<float>(std::max(0, col + std::max(0, widthChars))) * metrics.charWidthPx);
    const float y1 = std::ceil(static_cast<float>(std::max(0, row + std::max(0, heightRows))) * metrics.charHeightPx);
    return PixelRect{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
}

[[nodiscard]] PixelRect expandedRect(const PixelRect& rect, const float insetPx)
{
    return PixelRect{
        .x0 = rect.x0 - insetPx,
        .y0 = rect.y0 - insetPx,
        .x1 = rect.x1 + insetPx,
        .y1 = rect.y1 + insetPx,
    };
}

[[nodiscard]] bool pointInsideRect(const PixelRect& rect, const float x, const float y)
{
    return x >= rect.x0 && x <= rect.x1 && y >= rect.y0 && y <= rect.y1;
}

[[nodiscard]] PixelRect pauseButtonHitRect(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const int rowTop)
{
    const TextGridMetrics grid = makeTextGridMetrics(windowWidth, windowHeight, textWidth, textHeight);
    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
    const PixelRect rect = gridRectFromChars(
        grid,
        centerCol + 3,
        rowTop + 1,
        kWide - 6,
        std::max(2, detail::PauseMenuLayout::kButtonRowSpan - 2));
    const float paddingPx = std::max(12.0f, std::min(grid.charWidthPx, grid.charHeightPx) * 0.65f);
    return expandedRect(rect, paddingPx);
}
}  // namespace

int Renderer::hitTestMainMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const std::uint16_t menuLogoWidthPx,
    const std::uint16_t menuLogoHeightPx)
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
    const int titleMenuRowBias = detail::mainMenuLogoReservedDbgRows(
        windowWidth, windowHeight, textHeight, menuLogoWidthPx, menuLogoHeightPx);
    const detail::MainMenuComputedLayout menu = detail::computeMainMenuLayout(tw, th, titleMenuRowBias);

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

int Renderer::hitTestMainMenuSingleplayerPanel(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const std::uint16_t menuLogoWidthPx,
    const std::uint16_t menuLogoHeightPx)
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

    const int titleMenuRowBias = detail::mainMenuLogoReservedDbgRows(
        windowWidth, windowHeight, textHeight, menuLogoWidthPx, menuLogoHeightPx);
    const detail::MainMenuComputedLayout menu =
        detail::computeMainMenuLayout(tw, th, titleMenuRowBias);
    const int rowShift = detail::SingleplayerMenuLayout::singleplayerMenuRowShift(
        th, menu.buttonLineCount, titleMenuRowBias);
    for (int buttonIndex = 0; buttonIndex < detail::SingleplayerMenuLayout::kButtonCount; ++buttonIndex)
    {
        const int row0 =
            detail::SingleplayerMenuLayout::kFirstButtonRow + rowShift + buttonIndex * menu.buttonLineCount;
        if (clampedRow >= row0 && clampedRow <= row0 + menu.buttonLineCount - 1
            && clampedCol >= menu.centerCol && clampedCol <= menu.centerCol + menu.outerWidth - 1)
        {
            return buttonIndex;
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

    const int th = static_cast<int>(textHeight);
    const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
    constexpr int kPitch = detail::PauseMenuLayout::kButtonPitch;

    for (int buttonIndex = 0; buttonIndex < detail::PauseMenuLayout::kMainButtonCount; ++buttonIndex)
    {
        const int row0 = firstButtonRow + buttonIndex * kPitch;
        if (pointInsideRect(
                pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, row0),
                mouseX,
                mouseY))
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

    const int th = static_cast<int>(textHeight);
    const int backRow = detail::PauseMenuLayout::pauseGameBackButtonRow(th);
    const int mobRow = detail::PauseMenuLayout::pauseGameMobButtonRow(th);
    const int creativeRow = detail::PauseMenuLayout::pauseGameCreativeButtonRow(th);
    const int difficultyRow = detail::PauseMenuLayout::pauseGameDifficultyButtonRow(th);
    const int biomeRow = detail::PauseMenuLayout::pauseGameBiomeButtonRow(th);
    const int travelRow = detail::PauseMenuLayout::pauseGameTravelButtonRow(th);
    const int weatherRow = detail::PauseMenuLayout::pauseGameWeatherButtonRow(th);

    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, backRow),
            mouseX,
            mouseY))
    {
        return 0;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, mobRow),
            mouseX,
            mouseY))
    {
        return 1;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, creativeRow),
            mouseX,
            mouseY))
    {
        return 2;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, difficultyRow),
            mouseX,
            mouseY))
    {
        return 3;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, biomeRow),
            mouseX,
            mouseY))
    {
        return 4;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, travelRow),
            mouseX,
            mouseY))
    {
        return 5;
    }
    if (pointInsideRect(
            pauseButtonHitRect(windowWidth, windowHeight, textWidth, textHeight, weatherRow),
            mouseX,
            mouseY))
    {
        return 6;
    }

    return -1;
}

int Renderer::hitTestMainMenuOptions(
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
    constexpr int kSpan = 7;

    // Back button at row 25, Sound settings at row 9, Display name at row 16
    constexpr int kBackRow = 25;
    constexpr int kSoundRow = 9;
    constexpr int kDisplayNameRow = 16;

    if (clampedRow >= kBackRow && clampedRow <= kBackRow + kSpan - 1
        && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= kSoundRow && clampedRow <= kSoundRow + kSpan - 1
        && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 1;
    }
    if (clampedRow >= kDisplayNameRow && clampedRow <= kDisplayNameRow + kSpan - 1
        && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
    {
        return 2;
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
    const int backRow = detail::PauseMenuLayout::pauseSoundBackButtonRow(th);
    const int musicRow = detail::PauseMenuLayout::pauseSoundMusicButtonRow(th);
    const int sfxRow = detail::PauseMenuLayout::pauseSoundSfxButtonRow(th);

    if (clampedRow >= backRow && clampedRow <= backRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        return 0;
    }
    if (clampedRow >= musicRow && clampedRow <= musicRow + kSpan - 1 && clampedCol >= centerCol
        && clampedCol <= centerCol + kWide - 1)
    {
        if (clampedRow == musicRow + detail::PauseMenuLayout::kButtonLabelRowOffset)
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
        if (clampedRow == sfxRow + detail::PauseMenuLayout::kButtonLabelRowOffset)
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

std::optional<float> Renderer::pauseSoundSliderValueFromMouse(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const bool musicSlider)
{
    if (windowWidth == 0 || windowHeight == 0 || textWidth == 0 || textHeight == 0)
    {
        return std::nullopt;
    }

    const int col = static_cast<int>(mouseX * static_cast<float>(textWidth) / static_cast<float>(windowWidth));
    const int row = static_cast<int>(mouseY * static_cast<float>(textHeight) / static_cast<float>(windowHeight));
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const int clampedCol = std::clamp(col, 0, tw - 1);
    const int clampedRow = std::clamp(row, 0, th - 1);

    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    const int sliderRow = (musicSlider ? detail::PauseMenuLayout::pauseSoundMusicButtonRow(th)
                                       : detail::PauseMenuLayout::pauseSoundSfxButtonRow(th))
        + detail::PauseMenuLayout::kButtonLabelRowOffset;
    if (clampedRow != sliderRow)
    {
        return std::nullopt;
    }

    const int innerStartCol = centerCol + 1;
    const int fillStartCol = innerStartCol + detail::PauseMenuLayout::kSoundSliderFillStartInner;
    const int fillEndCol = fillStartCol + detail::PauseMenuLayout::kSoundSliderFillChars - 1;
    if (clampedCol < fillStartCol || clampedCol > fillEndCol)
    {
        return std::nullopt;
    }

    const int columnSpan = detail::PauseMenuLayout::kSoundSliderFillChars;
    if (columnSpan <= 1)
    {
        return clampedCol == fillStartCol ? std::optional<float>(0.0f) : std::optional<float>(1.0f);
    }

    const float t = static_cast<float>(clampedCol - fillStartCol)
        / static_cast<float>(columnSpan - 1);
    return std::clamp(t, 0.0f, 1.0f);
}

int Renderer::multiplayerMenuRowShift(
    const std::uint16_t textHeight,
    const FrameDebugData::MainMenuMultiplayerPanel panel,
    const int joinPresetSlotCount,
    const int mainMenuContentTopBias)
{
    return detail::MultiplayerMenuLayout::multiplayerMenuRowShift(
        static_cast<int>(textHeight), panel, joinPresetSlotCount, mainMenuContentTopBias);
}

int Renderer::hitTestMainMenuMultiplayerHub(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const int multiplayerRowShift,
    const int mainMenuContentTopBias)
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

    const detail::MainMenuComputedLayout menu = detail::computeMainMenuLayout(tw, th, mainMenuContentTopBias);
    const int centerCol = menu.centerCol;
    const int rightEdge = centerCol + menu.outerWidth - 1;
    const int rs = multiplayerRowShift;
    constexpr int kBtnH = detail::MultiplayerMenuLayout::kMainButtonLineCount;

    if (clampedRow >= detail::MultiplayerMenuLayout::kHubHostRow + rs
        && clampedRow <= detail::MultiplayerMenuLayout::kHubHostRow + kBtnH - 1 + rs && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return 0;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHubJoinRow + rs
        && clampedRow <= detail::MultiplayerMenuLayout::kHubJoinRow + kBtnH - 1 + rs && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return 1;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHubBackRow + rs
        && clampedRow <= detail::MultiplayerMenuLayout::kHubBackRow + kBtnH - 1 + rs && clampedCol >= centerCol
        && clampedCol <= rightEdge)
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
    const std::uint16_t textHeight,
    const int multiplayerRowShift,
    const int mainMenuContentTopBias)
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

    const detail::MainMenuComputedLayout menu = detail::computeMainMenuLayout(tw, th, mainMenuContentTopBias);
    const int centerCol = menu.centerCol;
    const int rightEdge = centerCol + menu.outerWidth - 1;
    const int rs = multiplayerRowShift;
    constexpr int kBtnH = detail::MultiplayerMenuLayout::kMainButtonLineCount;

    if (clampedRow >= detail::MultiplayerMenuLayout::kHostStartRow + rs
        && clampedRow <= detail::MultiplayerMenuLayout::kHostStartRow + kBtnH - 1 + rs && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return 0;
    }
    if (clampedRow >= detail::MultiplayerMenuLayout::kHostBackRow + rs
        && clampedRow <= detail::MultiplayerMenuLayout::kHostBackRow + kBtnH - 1 + rs && clampedCol >= centerCol
        && clampedCol <= rightEdge)
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
    const std::uint16_t textHeight,
    const int joinPresetSlotCount,
    const int multiplayerRowShift,
    const int mainMenuContentTopBias)
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

    const detail::MainMenuComputedLayout menu = detail::computeMainMenuLayout(tw, th, mainMenuContentTopBias);
    const int centerCol = menu.centerCol;
    const int rightEdge = centerCol + menu.outerWidth - 1;
    const int rs = multiplayerRowShift;
    constexpr int kBtnH = detail::MultiplayerMenuLayout::kMainButtonLineCount;
    constexpr int kFieldH = detail::MultiplayerMenuLayout::kFieldButtonLineCount;

    const int presetSlots = std::clamp(joinPresetSlotCount, 0, detail::MultiplayerMenuLayout::kJoinPresetSlotMax);
    for (int i = 0; i < presetSlots; ++i)
    {
        const int startRow = detail::MultiplayerMenuLayout::joinPresetButtonStartRow(i) + rs;
        if (clampedRow >= startRow && clampedRow <= startRow + kBtnH - 1 && clampedCol >= centerCol
            && clampedCol <= rightEdge)
        {
            return i;
        }
    }

    int addrFieldRow = 0;
    [[maybe_unused]] int portLabelUnused = 0;
    int portFieldRow = 0;
    int connectRow = 0;
    int backRow = 0;
    detail::MultiplayerMenuLayout::joinManualSectionRows(
        presetSlots, addrFieldRow, portLabelUnused, portFieldRow, connectRow, backRow);
    addrFieldRow += rs;
    portFieldRow += rs;
    connectRow += rs;
    backRow += rs;

    if (clampedRow >= addrFieldRow && clampedRow <= addrFieldRow + kFieldH - 1 && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return presetSlots;
    }
    if (clampedRow >= portFieldRow && clampedRow <= portFieldRow + kFieldH - 1 && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return presetSlots + 1;
    }
    if (clampedRow >= connectRow && clampedRow <= connectRow + kBtnH - 1 && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return presetSlots + 2;
    }
    if (clampedRow >= backRow && clampedRow <= backRow + kBtnH - 1 && clampedCol >= centerCol
        && clampedCol <= rightEdge)
    {
        return presetSlots + 3;
    }

    return -1;
}

int Renderer::hitTestCraftingMenu(
    const float mouseX,
    const float mouseY,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const CraftingUiMode mode,
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
        detail::computeCraftingOverlayLayoutPx(windowWidth, windowHeight, mode, useWorkbench);
    if (layout.slotSize <= 0.0f)
    {
        return -1;
    }

    const auto insideRect = [&](const float x0, const float y0)
    {
        return mouseX >= x0 && mouseX <= x0 + layout.slotSize
            && mouseY >= y0 && mouseY <= y0 + layout.slotSize;
    };

    if (mode == CraftingUiMode::Furnace)
    {
        if (insideRect(layout.craftingOriginX, layout.craftingOriginY))
        {
            return kCraftingGridHitBase + 1;
        }
        if (insideRect(layout.furnaceFuelSlotX, layout.furnaceFuelSlotY))
        {
            return kCraftingGridHitBase + 7;
        }
    }
    else
    {
        const int craftingColumns = mode == CraftingUiMode::Chest ? 9 : (mode == CraftingUiMode::Workbench ? 3 : 2);
        const int craftingRows = mode == CraftingUiMode::Chest ? 3 : (mode == CraftingUiMode::Workbench ? 3 : 2);
        for (int row = 0; row < craftingRows; ++row)
        {
            for (int col = 0; col < craftingColumns; ++col)
            {
                const float slotX = layout.craftingOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
                const float slotY = layout.craftingOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
                if (insideRect(slotX, slotY))
                {
                    return kCraftingGridHitBase + (mode == CraftingUiMode::Chest ? row * 9 + col : row * 3 + col);
                }
            }
        }
    }

    if (mode != CraftingUiMode::Chest && insideRect(layout.resultSlotX, layout.resultSlotY))
    {
        return kCraftingResultHit;
    }

    if (mode == CraftingUiMode::Inventory)
    {
        constexpr int kEquipmentSlotCount = 4;
        for (int slotIndex = 0; slotIndex < kEquipmentSlotCount; ++slotIndex)
        {
            const float slotX = layout.equipmentOriginX;
            const float slotY = layout.equipmentOriginY + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
            if (insideRect(slotX, slotY))
            {
                return kCraftingEquipmentHitBase + slotIndex;
            }
        }
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
        const float slotY = layout.hotbarOriginY;
        if (insideRect(slotX, slotY))
        {
            return kCraftingHotbarHitBase + slotIndex;
        }
    }

    return -1;
}

}  // namespace vibecraft::render
