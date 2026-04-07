#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::render
{
namespace
{
struct TextGridMetrics
{
    float charWidthPx = 1.0f;
    float charHeightPx = 1.0f;
};

struct PixelRect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

[[nodiscard]] TextGridMetrics makeTextGridMetrics(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    return TextGridMetrics{
        .charWidthPx = static_cast<float>(windowWidth) / static_cast<float>(std::max<std::uint16_t>(1, textWidth)),
        .charHeightPx = static_cast<float>(windowHeight) / static_cast<float>(std::max<std::uint16_t>(1, textHeight)),
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
} // namespace

void Renderer::drawMainMenuChrome(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const int mainMenuTitleContentRowOffset)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0)
    {
        return;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const TextGridMetrics grid = makeTextGridMetrics(width_, height_, textWidth, textHeight);
    const auto drawPanel = [&](const PixelRect& rect,
                               const glm::vec3& fillTint,
                               const glm::vec3& edgeTint,
                               const glm::vec3& glowTint,
                               const float fillAlpha)
    {
        const PixelRect shadow = expandedRect(rect, 12.0f);
        const PixelRect outer = expandedRect(rect, 5.0f);
        const PixelRect inner = expandedRect(rect, 2.0f);
        drawUiSolidRect(shadow.x0, shadow.y0 + 6.0f, shadow.x1, shadow.y1 + 16.0f, detail::packAbgr8(glm::vec3(0.01f, 0.02f, 0.03f), 0.34f));
        drawUiSolidRect(outer.x0, outer.y0, outer.x1, outer.y1, detail::packAbgr8(glowTint, 0.72f));
        drawUiSolidRect(inner.x0, inner.y0, inner.x1, inner.y1, detail::packAbgr8(edgeTint, 0.96f));
        drawUiSolidRect(rect.x0, rect.y0, rect.x1, rect.y1, detail::packAbgr8(fillTint, fillAlpha));
        drawUiSolidRect(
            rect.x0 + 3.0f,
            rect.y0 + 3.0f,
            rect.x1 - 3.0f,
            rect.y0 + std::max(7.0f, (rect.y1 - rect.y0) * 0.18f),
            detail::packAbgr8(glm::mix(fillTint, glm::vec3(1.0f), 0.14f), 0.30f));
    };
    const auto drawButtonPanel = [&](const PixelRect& rect, const bool hovered)
    {
        const glm::vec3 fillTint = hovered ? glm::vec3(0.36f, 0.28f, 0.12f) : glm::vec3(0.09f, 0.14f, 0.20f);
        const glm::vec3 edgeTint = hovered ? glm::vec3(0.92f, 0.76f, 0.40f) : glm::vec3(0.32f, 0.46f, 0.58f);
        const glm::vec3 glowTint = hovered ? glm::vec3(0.70f, 0.56f, 0.20f) : glm::vec3(0.14f, 0.24f, 0.34f);
        drawPanel(rect, fillTint, edgeTint, glowTint, hovered ? 0.92f : 0.84f);
    };

    drawUiSolidRect(
        0.0f,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.02f, 0.07f, 0.10f), 0.16f));
    drawUiSolidRect(
        0.0f,
        static_cast<float>(height_) * 0.72f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.03f, 0.05f, 0.03f), 0.20f));

    if (frameDebugData.mainMenuLoadingActive)
    {
        const int tw = static_cast<int>(textWidth);
        const int th = static_cast<int>(textHeight);
        const int panelWidth = std::clamp(tw - 10, 52, std::min(100, std::max(52, tw - 4)));
        const int panelHeight = std::min(15, std::max(13, th - 2));
        const int panelCol = std::max(0, (tw - panelWidth) / 2);
        const int panelRow = std::max(1, (th - panelHeight) / 2);
        const int barRow = panelRow + 10;

        // Full-screen dark vignette so the loading panel reads on top of the menu background.
        drawUiSolidRect(
            0.0f,
            0.0f,
            static_cast<float>(width_),
            static_cast<float>(height_),
            detail::packAbgr8(glm::vec3(0.02f, 0.04f, 0.08f), 0.55f));

        // Main panel.
        drawPanel(
            gridRectFromChars(grid, panelCol, panelRow, panelWidth, panelHeight),
            glm::vec3(0.06f, 0.09f, 0.14f),
            glm::vec3(0.40f, 0.54f, 0.66f),
            glm::vec3(0.90f, 0.78f, 0.36f),
            0.96f);

        // Accent strip behind the brand row (row +1).
        {
            const PixelRect brandRect = gridRectFromChars(grid, panelCol + 1, panelRow + 1, panelWidth - 2, 1);
            drawUiSolidRect(brandRect.x0, brandRect.y0, brandRect.x1, brandRect.y1,
                detail::packAbgr8(glm::vec3(0.18f, 0.14f, 0.06f), 0.60f));
        }

        // Accent strip behind the progress bar row.
        {
            const PixelRect barRect = gridRectFromChars(grid, panelCol + 1, barRow, panelWidth - 2, 1);
            drawUiSolidRect(barRect.x0, barRect.y0, barRect.x1, barRect.y1,
                detail::packAbgr8(glm::vec3(0.08f, 0.16f, 0.10f), 0.55f));
        }

        return;
    }

    if (frameDebugData.mainMenuOptionsActive)
    {
        constexpr int kWide = 96;
        const int optCenterCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
        drawPanel(
            gridRectFromChars(grid, optCenterCol - 3, 4, kWide + 6, 28),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.34f, 0.46f, 0.56f),
            glm::vec3(0.82f, 0.70f, 0.32f),
            0.92f);
        drawButtonPanel(gridRectFromChars(grid, optCenterCol + 2, 10, kWide - 4, 3), frameDebugData.mainMenuOptionsHoveredControl == 1);
        drawButtonPanel(gridRectFromChars(grid, optCenterCol + 2, 17, kWide - 4, 3), frameDebugData.mainMenuOptionsHoveredControl == 2);
        drawButtonPanel(gridRectFromChars(grid, optCenterCol + 2, 26, kWide - 4, 3), frameDebugData.mainMenuOptionsHoveredControl == 0);
        return;
    }

    if (frameDebugData.mainMenuSoundSettingsActive)
    {
        constexpr int kWide = 96;
        const int soundCenterCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
        drawPanel(
            gridRectFromChars(grid, soundCenterCol - 3, 4, kWide + 6, 28),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.34f, 0.46f, 0.56f),
            glm::vec3(0.82f, 0.70f, 0.32f),
            0.92f);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 10, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 1 || frameDebugData.mainMenuSoundSettingsHoveredControl == 2);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 17, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 3 || frameDebugData.mainMenuSoundSettingsHoveredControl == 4);
        drawButtonPanel(gridRectFromChars(grid, soundCenterCol + 2, 26, kWide - 4, 3), frameDebugData.mainMenuSoundSettingsHoveredControl == 0);
        return;
    }

    if (frameDebugData.mainMenuSingleplayerPanelActive)
    {
        const detail::MainMenuComputedLayout menu =
            detail::computeMainMenuLayout(static_cast<int>(textWidth), static_cast<int>(textHeight), mainMenuTitleContentRowOffset);
        const int rowShift = detail::SingleplayerMenuLayout::singleplayerMenuRowShift(
            static_cast<int>(textHeight), menu.buttonLineCount, mainMenuTitleContentRowOffset);
        const int panelTopRow = detail::SingleplayerMenuLayout::kTitleAnchorRow + rowShift - 1;
        const int panelBottomRow =
            detail::SingleplayerMenuLayout::kFirstButtonRow
            + rowShift
            + detail::SingleplayerMenuLayout::kButtonCount * menu.buttonLineCount;
        drawPanel(
            gridRectFromChars(
                grid,
                menu.centerCol - 3,
                panelTopRow,
                menu.outerWidth + 6,
                std::max(8, panelBottomRow - panelTopRow + 1)),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.33f, 0.44f, 0.54f),
            glm::vec3(0.80f, 0.70f, 0.34f),
            0.92f);
        for (int buttonIndex = 0; buttonIndex < detail::SingleplayerMenuLayout::kButtonCount; ++buttonIndex)
        {
            const int buttonRow =
                detail::SingleplayerMenuLayout::kFirstButtonRow + rowShift + buttonIndex * menu.buttonLineCount;
            drawButtonPanel(
                gridRectFromChars(
                    grid,
                    menu.centerCol + 2,
                    buttonRow + std::max(0, menu.buttonLineCount / 2) - 1,
                    menu.outerWidth - 4,
                    3),
                frameDebugData.mainMenuSingleplayerHoveredControl == buttonIndex);
        }
        return;
    }

    if (frameDebugData.mainMenuMultiplayerPanel != FrameDebugData::MainMenuMultiplayerPanel::None)
    {
        const detail::MainMenuComputedLayout menu =
            detail::computeMainMenuLayout(static_cast<int>(textWidth), static_cast<int>(textHeight), mainMenuTitleContentRowOffset);
        const int joinPresetSlots = static_cast<int>(std::min<std::size_t>(frameDebugData.mainMenuJoinPresetButtonLabels.size(), 3));
        const int rowShift = detail::MultiplayerMenuLayout::multiplayerMenuRowShift(
            static_cast<int>(textHeight), frameDebugData.mainMenuMultiplayerPanel, joinPresetSlots, mainMenuTitleContentRowOffset);
        int panelTopRow = 4 + rowShift;
        int panelBottomRow = static_cast<int>(textHeight) - 3;
        switch (frameDebugData.mainMenuMultiplayerPanel)
        {
        case FrameDebugData::MainMenuMultiplayerPanel::Hub:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::kHubBackRow + menu.buttonLineCount + rowShift + 1;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::Host:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::kHostBackRow + menu.buttonLineCount + rowShift + 1;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::Join:
            panelTopRow = 4 + rowShift;
            panelBottomRow = detail::MultiplayerMenuLayout::multiplayerJoinBottomRow(joinPresetSlots) + rowShift;
            break;
        case FrameDebugData::MainMenuMultiplayerPanel::None:
        default:
            break;
        }
        drawPanel(
            gridRectFromChars(
                grid,
                menu.centerCol - 4,
                panelTopRow,
                menu.outerWidth + 8,
                std::max(8, panelBottomRow - panelTopRow + 1)),
            glm::vec3(0.08f, 0.12f, 0.18f),
            glm::vec3(0.32f, 0.43f, 0.54f),
            glm::vec3(0.78f, 0.72f, 0.36f),
            0.92f);
        return;
    }

    const detail::MainMenuComputedLayout menu =
        detail::computeMainMenuLayout(static_cast<int>(textWidth), static_cast<int>(textHeight), mainMenuTitleContentRowOffset);
    drawPanel(
        gridRectFromChars(
            grid,
            menu.centerCol - 4,
            menu.subtitleRow - 2,
            menu.outerWidth + 8,
            std::max(8, menu.iconHintsRow - menu.subtitleRow + 4)),
        glm::vec3(0.08f, 0.12f, 0.18f),
        glm::vec3(0.34f, 0.47f, 0.58f),
        glm::vec3(0.82f, 0.72f, 0.34f),
        0.90f);
    drawPanel(
        gridRectFromChars(
            grid,
            menu.centerCol + menu.outerWidth / 2 - 16,
            menu.subtitleRow - 1,
            32,
            2),
        glm::vec3(0.14f, 0.20f, 0.28f),
        glm::vec3(0.56f, 0.68f, 0.78f),
        glm::vec3(0.88f, 0.76f, 0.34f),
        0.94f);

    for (std::size_t buttonIndex = 0; buttonIndex < menu.buttonTopRows.size(); ++buttonIndex)
    {
        drawButtonPanel(
            gridRectFromChars(
                grid,
                menu.centerCol + 3,
                menu.buttonTopRows[buttonIndex] + 1,
                menu.outerWidth - 6,
                std::max(2, menu.buttonLineCount - 2)),
            frameDebugData.mainMenuHoveredButton == static_cast<int>(buttonIndex));
    }

    if (menu.centerCol >= 8)
    {
        drawButtonPanel(
            gridRectFromChars(grid, menu.centerCol - 8, menu.iconHintsRow - 1, 6, 2),
            frameDebugData.mainMenuHoveredButton == 5);
    }
    drawButtonPanel(
        gridRectFromChars(grid, menu.centerCol + menu.outerWidth + 2, menu.iconHintsRow - 1, 6, 2),
        frameDebugData.mainMenuHoveredButton == 6);
}

void Renderer::drawPauseMenuChrome(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (inventoryUiSolidProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX || width_ == 0 || height_ == 0)
    {
        return;
    }

    float view[16];
    bx::mtxIdentity(view);
    float proj[16];
    bx::mtxOrtho(
        proj,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f,
        0.0f,
        1000.0f,
        0.0f,
        bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(detail::kUiView, view, proj);

    const TextGridMetrics grid = makeTextGridMetrics(width_, height_, textWidth, textHeight);
    const int th = static_cast<int>(textHeight);
    constexpr int kWide = detail::PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
    const auto drawPanel = [&](const PixelRect& rect,
                               const glm::vec3& fillTint,
                               const glm::vec3& edgeTint,
                               const glm::vec3& glowTint,
                               const float fillAlpha)
    {
        const PixelRect shadow = expandedRect(rect, 14.0f);
        const PixelRect outer = expandedRect(rect, 5.0f);
        const PixelRect inner = expandedRect(rect, 2.0f);
        drawUiSolidRect(shadow.x0, shadow.y0 + 8.0f, shadow.x1, shadow.y1 + 18.0f, detail::packAbgr8(glm::vec3(0.01f, 0.01f, 0.02f), 0.42f));
        drawUiSolidRect(outer.x0, outer.y0, outer.x1, outer.y1, detail::packAbgr8(glowTint, 0.74f));
        drawUiSolidRect(inner.x0, inner.y0, inner.x1, inner.y1, detail::packAbgr8(edgeTint, 0.97f));
        drawUiSolidRect(rect.x0, rect.y0, rect.x1, rect.y1, detail::packAbgr8(fillTint, fillAlpha));
        drawUiSolidRect(
            rect.x0 + 3.0f,
            rect.y0 + 3.0f,
            rect.x1 - 3.0f,
            rect.y0 + std::max(7.0f, (rect.y1 - rect.y0) * 0.16f),
            detail::packAbgr8(glm::mix(fillTint, glm::vec3(1.0f), 0.12f), 0.28f));
    };
    const auto drawButtonPanel = [&](const int rowTop, const bool hovered)
    {
        const PixelRect rect = gridRectFromChars(
            grid,
            centerCol + 3,
            rowTop + 1,
            kWide - 6,
            std::max(2, detail::PauseMenuLayout::kButtonRowSpan - 2));
        const glm::vec3 fillTint = hovered ? glm::vec3(0.36f, 0.28f, 0.12f) : glm::vec3(0.09f, 0.14f, 0.21f);
        const glm::vec3 edgeTint = hovered ? glm::vec3(0.92f, 0.76f, 0.40f) : glm::vec3(0.33f, 0.46f, 0.60f);
        const glm::vec3 glowTint = hovered ? glm::vec3(0.72f, 0.54f, 0.20f) : glm::vec3(0.15f, 0.23f, 0.34f);
        drawPanel(rect, fillTint, edgeTint, glowTint, hovered ? 0.94f : 0.86f);
    };

    drawUiSolidRect(
        0.0f,
        0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        detail::packAbgr8(glm::vec3(0.02f, 0.05f, 0.09f), 0.44f));

    int panelTopRow = 3;
    int panelBottomRow = th - 3;
    if (frameDebugData.pauseSoundSettingsActive)
    {
        panelTopRow = detail::PauseMenuLayout::pauseSoundTitleRow(th) - 2;
        panelBottomRow = detail::PauseMenuLayout::pauseSoundBackButtonRow(th) + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }
    else if (frameDebugData.pauseGameSettingsActive)
    {
        panelTopRow = detail::PauseMenuLayout::pauseGameTitleRow(th) - 2;
        panelBottomRow = detail::PauseMenuLayout::pauseGameBackButtonRow(th) + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }
    else
    {
        const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
        panelTopRow = std::max(2, firstButtonRow - 4);
        panelBottomRow = firstButtonRow + 4 * detail::PauseMenuLayout::kButtonPitch + detail::PauseMenuLayout::kButtonRowSpan + 1;
    }

    drawPanel(
        gridRectFromChars(
            grid,
            centerCol - 4,
            panelTopRow,
            kWide + 8,
            std::max(10, panelBottomRow - panelTopRow + 1)),
        glm::vec3(0.08f, 0.12f, 0.18f),
        glm::vec3(0.34f, 0.47f, 0.62f),
        glm::vec3(0.84f, 0.74f, 0.36f),
        0.93f);

    if (frameDebugData.pauseSoundSettingsActive)
    {
        const int hovered = frameDebugData.pauseSoundSettingsHoveredControl;
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundMusicButtonRow(th), hovered == 1 || hovered == 2);
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundSfxButtonRow(th), hovered == 3 || hovered == 4);
        drawButtonPanel(detail::PauseMenuLayout::pauseSoundBackButtonRow(th), hovered == 0);
        return;
    }

    if (frameDebugData.pauseGameSettingsActive)
    {
        const int hovered = frameDebugData.pauseGameSettingsHoveredControl;
        drawButtonPanel(detail::PauseMenuLayout::pauseGameMobButtonRow(th), hovered == 1);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameCreativeButtonRow(th), hovered == 2);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameDifficultyButtonRow(th), hovered == 3);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameBiomeButtonRow(th), hovered == 4);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameTravelButtonRow(th), hovered == 5);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameWeatherButtonRow(th), hovered == 6);
        drawButtonPanel(detail::PauseMenuLayout::pauseGameBackButtonRow(th), hovered == 0);
        return;
    }

    const int firstButtonRow = detail::PauseMenuLayout::mainPauseMenuFirstButtonRow(th);
    const int hovered = frameDebugData.pauseMenuHoveredButton;
    constexpr int kPitch = detail::PauseMenuLayout::kButtonPitch;
    for (int buttonIndex = 0; buttonIndex < 5; ++buttonIndex)
    {
        drawButtonPanel(firstButtonRow + buttonIndex * kPitch, hovered == buttonIndex);
    }
}

} // namespace vibecraft::render
