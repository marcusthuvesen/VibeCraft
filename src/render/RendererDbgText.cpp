#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace vibecraft::render::detail
{


[[nodiscard]] std::string formatHotbarCell(const FrameDebugData::HotbarSlotHud& slot)
{
    if (slot.count == 0)
    {
        return std::string("[   ]");
    }

    const std::uint32_t displayCount = std::min(slot.count, 99u);
    return fmt::format("[{:02}]", displayCount);
}

/// Wider padded cells so the bag reads larger than the hotbar (same glyph + count).
[[nodiscard]] std::string formatBagSlotCell(const FrameDebugData::HotbarSlotHud& slot)
{
    if (slot.count == 0)
    {
        return std::string(" [---] ");
    }

    const std::uint32_t displayCount = std::min(slot.count, 99u);
    return fmt::format(" [{:02}] ", displayCount);
}

void dbgTextPrintfCenteredRow(
    const std::uint16_t row,
    const std::uint16_t attr,
    const std::string& text)
{
    const bgfx::Stats* stats = bgfx::getStats();
    const std::uint16_t textWidth = stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
    const int col =
        (static_cast<int>(textWidth) - static_cast<int>(text.size())) / 2;
    const int clampedCol = std::max(0, col);
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(clampedCol), row, attr, "%s", text.c_str());
}

[[nodiscard]] int computeCenteredColumnStart(const int totalChars)
{
    const bgfx::Stats* stats = bgfx::getStats();
    const std::uint16_t textWidth = stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
    const int startCol = (static_cast<int>(textWidth) - totalChars) / 2;
    return std::max(0, startCol);
}

[[nodiscard]] HotbarLayoutPx computeHotbarLayoutPx(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textHeight,
    const std::uint16_t hotbarRow)
{
    HotbarLayoutPx layout{};
    if (windowWidth == 0 || windowHeight == 0 || textHeight == 0)
    {
        return layout;
    }

    const float charH = static_cast<float>(windowHeight) / static_cast<float>(textHeight);
    // Triple the earlier HUD inventory target size, then scale down only if width would overflow.
    float slot = std::floor(charH * 4.26f);
    slot = std::clamp(slot, 72.0f, 132.0f);
    float gap = std::max(4.0f, std::round(slot * 0.15f));
    constexpr int kSlotCount = 9;
    float totalW = static_cast<float>(kSlotCount) * slot + static_cast<float>(kSlotCount - 1) * gap;
    const float maxW = static_cast<float>(windowWidth) * 0.94f;
    if (totalW > maxW && totalW > 1.0f)
    {
        const float scale = maxW / totalW;
        slot = std::max(28.0f, std::floor(slot * scale));
        gap = std::max(3.0f, std::round(gap * scale));
        totalW = static_cast<float>(kSlotCount) * slot + static_cast<float>(kSlotCount - 1) * gap;
    }

    const float originX = std::floor((static_cast<float>(windowWidth) - totalW) * 0.5f);
    const float rowTopY = static_cast<float>(hotbarRow) * charH;
    const float slotBottomY = rowTopY + charH * 0.9f;
    const float slotTopY = slotBottomY - slot;

    layout.originX = originX;
    layout.slotTopY = slotTopY;
    layout.slotSize = slot;
    layout.gap = gap;
    layout.totalWidth = totalW;
    return layout;
}

[[nodiscard]] CraftingOverlayLayoutPx computeCraftingOverlayLayoutPx(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const bool useWorkbench)
{
    CraftingOverlayLayoutPx layout{};
    if (windowWidth == 0 || windowHeight == 0)
    {
        return layout;
    }

    const float maxWidth = static_cast<float>(windowWidth) * 0.98f;
    const float maxHeight = static_cast<float>(windowHeight) * 0.98f;
    const int craftingColumns = useWorkbench ? 3 : 2;
    const int craftingRows = useWorkbench ? 3 : 2;
    constexpr int kVisibleInventoryRows = 3;
    constexpr int kVisibleHotbarRows = 1;

    const auto panelMetricsForSlot = [&](const float candidateSlotSize)
    {
        const float candidateGap = std::max(3.0f, std::round(candidateSlotSize * 0.14f));
        const float craftWidth = static_cast<float>(craftingColumns) * candidateSlotSize
            + static_cast<float>(craftingColumns - 1) * candidateGap;
        const float inventoryWidth = 9.0f * candidateSlotSize + 8.0f * candidateGap;
        const float resultGap = std::max(18.0f, std::round(candidateSlotSize * 1.1f));
        const float topSectionWidth = craftWidth + resultGap + candidateSlotSize;
        const float panelWidth = std::max(inventoryWidth, topSectionWidth) + candidateSlotSize * 1.8f;
        const float topSectionHeight = static_cast<float>(craftingRows) * candidateSlotSize
            + static_cast<float>(craftingRows - 1) * candidateGap;
        const float inventoryHeight =
            static_cast<float>(kVisibleInventoryRows + kVisibleHotbarRows) * candidateSlotSize
            + static_cast<float>(kVisibleInventoryRows + kVisibleHotbarRows - 1) * candidateGap;
        const float panelHeight = topSectionHeight + inventoryHeight + candidateSlotSize * 3.2f;
        return std::array<float, 6>{
            candidateGap,
            craftWidth,
            resultGap,
            topSectionWidth,
            panelWidth,
            panelHeight,
        };
    };

    const float previousBaselineSize =
        std::floor(std::min(maxWidth / 12.8f, maxHeight / 15.2f));
    float slotSize = std::clamp(previousBaselineSize * 3.0f, 22.0f, 160.0f);
    auto metrics = panelMetricsForSlot(slotSize);
    while (slotSize > 22.0f && (metrics[4] > maxWidth || metrics[5] > maxHeight))
    {
        slotSize -= 1.0f;
        metrics = panelMetricsForSlot(slotSize);
    }

    const float slotGap = metrics[0];
    const float craftWidth = metrics[1];
    const float resultGap = metrics[2];
    const float topSectionWidth = metrics[3];
    const float panelWidth = metrics[4];
    const float panelHeight = metrics[5];
    const float topSectionHeight = static_cast<float>(craftingRows) * slotSize
        + static_cast<float>(craftingRows - 1) * slotGap;
    const float panelLeft = std::floor((static_cast<float>(windowWidth) - panelWidth) * 0.5f);
    const float panelTop = std::floor((static_cast<float>(windowHeight) - panelHeight) * 0.5f);
    const float panelInnerX = panelLeft + slotSize * 0.9f;
    const float craftingOriginX = panelInnerX + std::floor((panelWidth - slotSize * 1.8f - topSectionWidth) * 0.5f);
    const float craftingOriginY = panelTop + slotSize * 1.5f;
    const float resultSlotX = craftingOriginX + craftWidth + resultGap;
    const float resultSlotY = craftingOriginY + (topSectionHeight - slotSize) * 0.5f;
    const float inventoryOriginX = panelInnerX;
    const float inventoryOriginY = craftingOriginY + topSectionHeight + slotSize * 1.55f;

    layout.panelLeft = panelLeft;
    layout.panelTop = panelTop;
    layout.panelRight = panelLeft + panelWidth;
    layout.panelBottom = panelTop + panelHeight;
    layout.slotSize = slotSize;
    layout.slotGap = slotGap;
    layout.craftingOriginX = craftingOriginX;
    layout.craftingOriginY = craftingOriginY;
    layout.resultSlotX = resultSlotX;
    layout.resultSlotY = resultSlotY;
    layout.inventoryOriginX = inventoryOriginX;
    layout.inventoryOriginY = inventoryOriginY;
    return layout;
}

[[nodiscard]] int computeHotbarGridWidthChars()
{
    constexpr int kCellChars = 5;
    constexpr int kGap = 1;
    constexpr int kSlotCount = 9;
    return kSlotCount * kCellChars + (kSlotCount - 1) * kGap;
}

[[nodiscard]] int computeBagGridWidthChars()
{
    constexpr int kCellChars = 9;
    constexpr int kGap = 1;
    constexpr int kSlotCount = 9;
    return kSlotCount * kCellChars + (kSlotCount - 1) * kGap;
}

[[nodiscard]] int computeCenteredHotbarStartColumn()
{
    return computeCenteredColumnStart(computeHotbarGridWidthChars());
}

[[nodiscard]] int computeHealthHudWidthChars(const int heartCount)
{
    constexpr int kCellChars = 2;
    constexpr int kGap = 1;
    return std::max(0, heartCount * kCellChars + std::max(0, heartCount - 1) * kGap);
}

void drawHealthHud(const std::uint16_t row, const FrameDebugData& frameDebugData)
{
    const float clampedMaxHealth = std::max(0.0f, frameDebugData.maxHealth);
    const float clampedHealth = std::clamp(frameDebugData.health, 0.0f, clampedMaxHealth);
    const int heartCount = std::max(1, static_cast<int>(std::ceil(clampedMaxHealth * 0.5f)));
    constexpr int kCellChars = 2;
    constexpr int kGap = 1;
    int col = computeCenteredColumnStart(computeHealthHudWidthChars(heartCount));

    for (int heartIndex = 0; heartIndex < heartCount; ++heartIndex)
    {
        const float heartHealth = clampedHealth - static_cast<float>(heartIndex * 2);
        const char* glyph = "..";
        std::uint16_t attr = 0x08;
        if (heartHealth >= 2.0f)
        {
            glyph = "++";
            attr = 0x0c;
        }
        else if (heartHealth >= 1.0f)
        {
            glyph = "+-";
            attr = 0x0e;
        }

        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), row, attr, "%s", glyph);
        col += kCellChars + kGap;
    }
}

void drawHotbarHud(const std::uint16_t row, const FrameDebugData& frameDebugData)
{
    constexpr int kCellChars = 5;
    constexpr int kGap = 1;
    int col = computeCenteredHotbarStartColumn();
    for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
    {
        const bool selected = slotIndex == frameDebugData.hotbarSelectedIndex;
        const bool empty = frameDebugData.hotbarSlots[slotIndex].count == 0;
        const std::uint16_t attr = selected ? 0x0f : (empty ? 0x08 : 0x0b);
        const std::string cell = formatHotbarCell(frameDebugData.hotbarSlots[slotIndex]);
        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), row, attr, "%s", cell.c_str());
        col += kCellChars + kGap;
    }
}

void drawHotbarKeyHintsRow(
    const std::uint16_t row,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const std::uint16_t hotbarRow)
{
    if (textWidth == 0 || textHeight == 0 || windowWidth == 0 || windowHeight == 0)
    {
        return;
    }

    const float charW = static_cast<float>(windowWidth) / static_cast<float>(textWidth);
    const HotbarLayoutPx layout = computeHotbarLayoutPx(windowWidth, windowHeight, textHeight, hotbarRow);
    if (layout.slotSize <= 0.0f)
    {
        return;
    }

    for (int keyIndex = 1; keyIndex <= 9; ++keyIndex)
    {
        const int i = keyIndex - 1;
        const float slotCenterX = layout.originX + static_cast<float>(i) * (layout.slotSize + layout.gap) + layout.slotSize * 0.5f;
        const int col = static_cast<int>(std::floor((slotCenterX / charW) - 0.5f));
        const int clampedCol = std::clamp(col, 0, static_cast<int>(textWidth) - 1);
        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(clampedCol), row, 0x08, "%d", keyIndex);
    }
}

void drawHotbarStackCounts(
    const std::uint16_t row,
    const FrameDebugData& frameDebugData,
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const std::uint16_t hotbarRow)
{
    if (textWidth == 0 || textHeight == 0 || windowWidth == 0 || windowHeight == 0)
    {
        return;
    }

    const float charW = static_cast<float>(windowWidth) / static_cast<float>(textWidth);
    const HotbarLayoutPx layout = computeHotbarLayoutPx(windowWidth, windowHeight, textHeight, hotbarRow);
    if (layout.slotSize <= 0.0f)
    {
        return;
    }

    for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
    {
        const std::uint32_t count = frameDebugData.hotbarSlots[slotIndex].count;
        if (count <= 1)
        {
            continue;
        }

        const float slotLeft = layout.originX + static_cast<float>(slotIndex) * (layout.slotSize + layout.gap);
        const float slotRight = slotLeft + layout.slotSize;
        const std::uint32_t displayCount = std::min(count, 99u);
        const std::string digits = fmt::format("{}", displayCount);
        const float textRightPx = slotRight - charW * 0.35f;
        int col = static_cast<int>(std::floor(textRightPx / charW)) - static_cast<int>(digits.size()) + 1;
        col = std::clamp(col, 0, static_cast<int>(textWidth) - static_cast<int>(digits.size()));
        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), row, 0x0f, "%s", digits.c_str());
    }
}

void drawBagRow(
    const std::uint16_t row,
    const std::size_t slotOffset,
    const FrameDebugData& frameDebugData)
{
    constexpr int kCellChars = 9;
    constexpr int kGap = 1;
    int col = computeCenteredColumnStart(computeBagGridWidthChars());
    for (int i = 0; i < 9; ++i)
    {
        const FrameDebugData::HotbarSlotHud& slot = frameDebugData.bagSlots[slotOffset + static_cast<std::size_t>(i)];
        const bool empty = slot.count == 0;
        const std::uint16_t attr = empty ? 0x08 : 0x0b;
        const std::string cell = " " + formatBagSlotCell(slot) + " ";
        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), row, attr, "%s", cell.c_str());
        col += kCellChars + kGap;
    }
}

void drawBagHud(
    const std::uint16_t titleRow,
    const std::uint16_t separatorRow,
    const std::uint16_t row0,
    const std::uint16_t row1,
    const std::uint16_t row2,
    const FrameDebugData& frameDebugData)
{
    std::uint32_t usedSlots = 0;
    std::uint32_t totalItems = 0;
    for (const FrameDebugData::HotbarSlotHud& slot : frameDebugData.bagSlots)
    {
        if (slot.count > 0)
        {
            ++usedSlots;
            totalItems += slot.count;
        }
    }

    const std::string title = fmt::format(
        "  INVENTORY   {}/{} slots   {} item{}  ",
        usedSlots,
        FrameDebugData::kBagHudSlotCount,
        totalItems,
        totalItems == 1 ? "" : "s");
    dbgTextPrintfCenteredRow(titleRow, 0x0e, title);

    const int gridWidth = computeBagGridWidthChars();
    const std::string sep =
        std::string("+") + std::string(static_cast<std::size_t>(std::max(0, gridWidth - 2)), '-') + std::string("+");
    dbgTextPrintfCenteredRow(separatorRow, 0x08, sep);

    drawBagRow(row0, 0, frameDebugData);
    drawBagRow(row1, 9, frameDebugData);
    drawBagRow(row2, 18, frameDebugData);
}

[[nodiscard]] std::string padLabelToInnerWidth(const std::string& text, const int innerWidth)
{
    if (innerWidth <= 0)
    {
        return {};
    }
    if (static_cast<int>(text.size()) >= innerWidth)
    {
        return text.substr(0, static_cast<std::size_t>(innerWidth));
    }
    const int totalPad = innerWidth - static_cast<int>(text.size());
    const int leftPad = totalPad / 2;
    const int rightPad = totalPad - leftPad;
    return std::string(static_cast<std::size_t>(leftPad), ' ') + text
        + std::string(static_cast<std::size_t>(rightPad), ' ');
}

[[nodiscard]] MainMenuComputedLayout computeMainMenuLayout(const int textWidth, const int textHeight)
{
    using namespace MainMenuLayout;
    MainMenuComputedLayout layout{};
    layout.outerWidth = std::clamp(textWidth - 8, kMinOuterWidth, std::min(kMaxOuterWidth, std::max(kMinOuterWidth, textWidth - 4)));
    layout.centerCol = std::max(0, (textWidth - layout.outerWidth) / 2);

    constexpr int kButtonCount = 5;
    int buttonLineCount = kPreferredButtonLineCount;
    int contentRows = kSubtitleRuleAndGapRows + kButtonCount * buttonLineCount + 1;
    while (contentRows > std::max(10, textHeight - 2) && buttonLineCount > kMinButtonLineCount)
    {
        buttonLineCount -= 2;
        contentRows = kSubtitleRuleAndGapRows + kButtonCount * buttonLineCount + 1;
    }
    layout.buttonLineCount = buttonLineCount;
    layout.firstContentRow =
        std::clamp((textHeight - contentRows) / 2, 1, std::max(1, textHeight - contentRows));

    layout.subtitleRow = layout.firstContentRow;
    layout.ruleRow = layout.firstContentRow + 1;
    const int firstButtonRow = layout.firstContentRow + kSubtitleRuleAndGapRows;
    for (int i = 0; i < kButtonCount; ++i)
    {
        layout.buttonTopRows[static_cast<std::size_t>(i)] = firstButtonRow + i * layout.buttonLineCount;
    }
    layout.iconHintsRow = firstButtonRow + kButtonCount * layout.buttonLineCount;
    return layout;
}

void drawMainMenuFramedButton5(
    const int row,
    const int col,
    const int outerWidth,
    const int lineCount,
    const std::string& label,
    const bool hovered)
{
    // Solid-filled button colors for stronger visual hierarchy.
    constexpr std::uint16_t kBorderGray = 0x17;
    constexpr std::uint16_t kLabelGray = 0x17;
    constexpr std::uint16_t kBorderHi = 0x3f;
    constexpr std::uint16_t kLabelHi = 0x3f;

    const std::uint16_t borderAttr = hovered ? kBorderHi : kBorderGray;
    const std::uint16_t labelAttr = hovered ? kLabelHi : kLabelGray;
    const int inner = outerWidth - 2;
    const std::string borderLine = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
    const std::string midEmpty = "|" + std::string(static_cast<std::size_t>(inner), ' ') + "|";
    const std::string midLabel = "|" + padLabelToInnerWidth(label, inner) + "|";

    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), borderAttr, "%s", borderLine.c_str());
    const int innerRows = std::max(1, lineCount - 2);
    const int labelInnerRow = innerRows / 2;
    for (int innerRow = 0; innerRow < innerRows; ++innerRow)
    {
        const bool isLabelRow = innerRow == labelInnerRow;
        const std::uint16_t attr = isLabelRow ? labelAttr : borderAttr;
        const std::string& line = isLabelRow ? midLabel : midEmpty;
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(col),
            static_cast<std::uint16_t>(row + 1 + innerRow),
            attr,
            "%s",
            line.c_str());
    }
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col),
        static_cast<std::uint16_t>(row + lineCount - 1),
        borderAttr,
        "%s",
        borderLine.c_str());
}

void drawFramedButton3(
    const int row,
    const int col,
    const int outerWidth,
    const std::string& label,
    const bool hovered,
    const std::uint16_t borderAttrNormal = 0x17,
    const std::uint16_t midAttrNormal = 0x17,
    const std::uint16_t borderAttrHover = 0x3f,
    const std::uint16_t midAttrHover = 0x3f)
{
    const std::uint16_t borderAttr = hovered ? borderAttrHover : borderAttrNormal;
    const std::uint16_t midAttr = hovered ? midAttrHover : midAttrNormal;
    const int inner = outerWidth - 2;
    const std::string border = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
    const std::string midEmpty = "|" + std::string(static_cast<std::size_t>(inner), ' ') + "|";
    const std::string midLabel = "|" + padLabelToInnerWidth(label, inner) + "|";
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), borderAttr, "%s", border.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 1), borderAttr, "%s", midEmpty.c_str());
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 2), midAttr, "%s", midLabel.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 3), borderAttr, "%s", midEmpty.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 4), borderAttr, "%s", border.c_str());
}

void drawBottomButtonPair(
    const int row,
    const int centerCol,
    const std::string& leftLabel,
    const std::string& rightLabel,
    const bool hoverLeft,
    const bool hoverRight)
{
    constexpr int kInner = 13;
    constexpr int kOuter = 15;
    const std::string border = "+" + std::string(static_cast<std::size_t>(kInner), '-') + "+";
    const std::string midL = "|" + padLabelToInnerWidth(leftLabel, kInner) + "|";
    const std::string midR = "|" + padLabelToInnerWidth(rightLabel, kInner) + "|";

    const std::uint16_t borderAttrL = hoverLeft ? 0x1f : 0x08;
    const std::uint16_t midAttrL = hoverLeft ? 0x1f : 0x0b;
    const std::uint16_t borderAttrR = hoverRight ? 0x1f : 0x08;
    const std::uint16_t midAttrR = hoverRight ? 0x1f : 0x0b;

    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol), static_cast<std::uint16_t>(row), borderAttrL, "%s", border.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol + kOuter + 1),
        static_cast<std::uint16_t>(row),
        borderAttrR,
        "%s",
        border.c_str());

    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol), static_cast<std::uint16_t>(row + 1), midAttrL, "%s", midL.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol + kOuter + 1),
        static_cast<std::uint16_t>(row + 1),
        midAttrR,
        "%s",
        midR.c_str());

    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol), static_cast<std::uint16_t>(row + 2), borderAttrL, "%s", border.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(centerCol + kOuter + 1),
        static_cast<std::uint16_t>(row + 2),
        borderAttrR,
        "%s",
        border.c_str());
}

[[nodiscard]] std::string clampDbgTextLine(const std::string& text, const std::size_t maxChars)
{
    if (text.size() <= maxChars)
    {
        return text;
    }
    if (maxChars <= 3)
    {
        return text.substr(0, maxChars);
    }
    return text.substr(0, maxChars - 3) + "...";
}

void drawMainMenuMultiplayerOverlay(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    const int tw = static_cast<int>(textWidth);
    constexpr int kWide = MultiplayerMenuLayout::kWide;
    const int centerCol = std::max(0, (tw - kWide) / 2);
    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    constexpr std::uint16_t kGrayBorder = 0x08;
    constexpr std::uint16_t kGrayLabel = 0x07;
    constexpr std::uint16_t kHiBorder = 0x08;
    constexpr std::uint16_t kHiLabel = 0x0f;

    const int hovered = frameDebugData.mainMenuMultiplayerHoveredControl;

    switch (frameDebugData.mainMenuMultiplayerPanel)
    {
    case FrameDebugData::MainMenuMultiplayerPanel::Hub:
        dbgTextPrintfCenteredRow(5, 0x0f, "MULTIPLAYER");
        dbgTextPrintfCenteredRow(7, 0x07, "Play with friends on the same Wi-Fi");
        drawFramedButton3(
            MultiplayerMenuLayout::kHubHostRow,
            centerCol,
            kWide,
            "Host game",
            hovered == 0,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(
            MultiplayerMenuLayout::kHubJoinRow,
            centerCol,
            kWide,
            "Join game",
            hovered == 1,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(
            MultiplayerMenuLayout::kHubBackRow,
            centerCol,
            kWide,
            "Back",
            hovered == 2,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        dbgTextPrintfCenteredRow(footerRow, 0x07, "Esc: back to title");
        break;

    case FrameDebugData::MainMenuMultiplayerPanel::Host:
        dbgTextPrintfCenteredRow(5, 0x0f, "HOST MULTIPLAYER");
        if (frameDebugData.mainMenuMultiplayerLanAddress.empty())
        {
            dbgTextPrintfCenteredRow(
                8,
                0x07,
                clampDbgTextLine("LAN IP not detected — check Wi-Fi IP in System Settings", static_cast<std::size_t>(tw - 2)));
        }
        else
        {
            dbgTextPrintfCenteredRow(
                8,
                0x07,
                clampDbgTextLine(
                    fmt::format(
                        "Friend on Wi-Fi: {}:{}",
                        frameDebugData.mainMenuMultiplayerLanAddress,
                        frameDebugData.mainMenuMultiplayerPortDisplay),
                    static_cast<std::size_t>(tw - 2)));
        }
        dbgTextPrintfCenteredRow(9, 0x08, "Same machine test: 127.0.0.1");
        drawFramedButton3(
            MultiplayerMenuLayout::kHostStartRow,
            centerCol,
            kWide,
            "Start hosting",
            hovered == 0,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(
            MultiplayerMenuLayout::kHostBackRow,
            centerCol,
            kWide,
            "Back",
            hovered == 1,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        dbgTextPrintfCenteredRow(footerRow, 0x07, "Esc: back");
        break;

    case FrameDebugData::MainMenuMultiplayerPanel::Join:
        dbgTextPrintfCenteredRow(5, 0x0f, "JOIN MULTIPLAYER");
        {
            const std::uint16_t addrLabelAttr = frameDebugData.mainMenuJoinFocusedField == 0 ? 0x0f : 0x07;
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                8,
                addrLabelAttr,
                "%s",
                "Host address");
            const int inner = kWide - 2;
            const std::string addrMid =
                "|" + padLabelToInnerWidth(clampDbgTextLine(frameDebugData.mainMenuJoinAddressField, 80), inner) + "|";
            const std::uint16_t addrBorderAttr =
                (hovered == 0 || frameDebugData.mainMenuJoinFocusedField == 0) ? kHiBorder : kGrayBorder;
            const std::uint16_t addrMidAttr =
                (hovered == 0 || frameDebugData.mainMenuJoinFocusedField == 0) ? kHiLabel : kGrayLabel;
            const std::string addrBorder = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinAddrFieldRow),
                addrBorderAttr,
                "%s",
                addrBorder.c_str());
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinAddrFieldRow + 1),
                addrMidAttr,
                "%s",
                addrMid.c_str());
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinAddrFieldRow + 2),
                addrBorderAttr,
                "%s",
                addrBorder.c_str());
        }
        {
            const std::uint16_t portLabelAttr = frameDebugData.mainMenuJoinFocusedField == 1 ? 0x0f : 0x07;
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                12,
                portLabelAttr,
                "%s",
                "Port");
            const int inner = kWide - 2;
            const std::string portMid =
                "|" + padLabelToInnerWidth(clampDbgTextLine(frameDebugData.mainMenuJoinPortField, 12), inner) + "|";
            const std::uint16_t portBorderAttr =
                (hovered == 1 || frameDebugData.mainMenuJoinFocusedField == 1) ? kHiBorder : kGrayBorder;
            const std::uint16_t portMidAttr =
                (hovered == 1 || frameDebugData.mainMenuJoinFocusedField == 1) ? kHiLabel : kGrayLabel;
            const std::string portBorder = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinPortFieldRow),
                portBorderAttr,
                "%s",
                portBorder.c_str());
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinPortFieldRow + 1),
                portMidAttr,
                "%s",
                portMid.c_str());
            bgfx::dbgTextPrintf(
                static_cast<std::uint16_t>(centerCol),
                static_cast<std::uint16_t>(MultiplayerMenuLayout::kJoinPortFieldRow + 2),
                portBorderAttr,
                "%s",
                portBorder.c_str());
        }
        drawFramedButton3(
            MultiplayerMenuLayout::kJoinConnectRow,
            centerCol,
            kWide,
            "Connect",
            hovered == 2,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(
            MultiplayerMenuLayout::kJoinBackRow,
            centerCol,
            kWide,
            "Back",
            hovered == 3,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        dbgTextPrintfCenteredRow(footerRow, 0x07, "Esc: back   Tab: switch field   Type: edit");
        break;

    case FrameDebugData::MainMenuMultiplayerPanel::None:
    default:
        break;
    }
}

void drawMainMenuOverlay(const FrameDebugData& frameDebugData, const std::uint16_t textWidth, const std::uint16_t textHeight)
{
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);

    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;

    if (frameDebugData.mainMenuSoundSettingsActive)
    {
        constexpr int kWide = 96;
        const int soundCenterCol = std::max(0, (tw - kWide) / 2);
        dbgTextPrintfCenteredRow(5, 0x07, "SOUND SETTINGS");
        const int hovered = frameDebugData.mainMenuSoundSettingsHoveredControl;
        const int musicPercent = static_cast<int>(std::round(std::clamp(frameDebugData.mainMenuSoundMusicVolume, 0.0f, 1.0f) * 100.0f));
        const int sfxPercent = static_cast<int>(std::round(std::clamp(frameDebugData.mainMenuSoundSfxVolume, 0.0f, 1.0f) * 100.0f));
        constexpr std::uint16_t kGrayBorder = 0x17;
        constexpr std::uint16_t kGrayLabel = 0x17;
        constexpr std::uint16_t kHiBorder = 0x3f;
        constexpr std::uint16_t kHiLabel = 0x3f;
        drawFramedButton3(
            9,
            soundCenterCol,
            kWide,
            fmt::format("Music volume: {:3d}%   [-] [+]", musicPercent),
            hovered == 1 || hovered == 2,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(
            16,
            soundCenterCol,
            kWide,
            fmt::format("SFX volume:   {:3d}%   [-] [+]", sfxPercent),
            hovered == 3 || hovered == 4,
            kGrayBorder,
            kGrayLabel,
            kHiBorder,
            kHiLabel);
        drawFramedButton3(25, soundCenterCol, kWide, "Back", hovered == 0, kGrayBorder, kGrayLabel, kHiBorder, kHiLabel);
        if (!frameDebugData.mainMenuNotice.empty() && footerRow >= 3)
        {
            const std::uint16_t noticeRow =
                footerRow >= 26 ? static_cast<std::uint16_t>(footerRow - 2) : static_cast<std::uint16_t>(0);
            dbgTextPrintfCenteredRow(noticeRow, 0x07, frameDebugData.mainMenuNotice);
        }
        dbgTextPrintfCenteredRow(footerRow, 0x07, "Esc: back to title menu   Click: adjust");
        return;
    }

    if (frameDebugData.mainMenuMultiplayerPanel != FrameDebugData::MainMenuMultiplayerPanel::None)
    {
        drawMainMenuMultiplayerOverlay(frameDebugData, textWidth, textHeight);
        return;
    }

    if (frameDebugData.mainMenuLoadingActive)
    {
        dbgTextPrintfCenteredRow(6, 0x0f, "LOADING SINGLEPLAYER");
        const int percent =
            static_cast<int>(std::round(std::clamp(frameDebugData.mainMenuLoadingProgress, 0.0f, 1.0f) * 100.0f));
        dbgTextPrintfCenteredRow(9, 0x07, frameDebugData.mainMenuLoadingLabel);
        dbgTextPrintfCenteredRow(11, 0x0f, fmt::format("{}%", percent));

        constexpr int kBarWidth = 34;
        const int fillChars = std::clamp(
            static_cast<int>(std::round(frameDebugData.mainMenuLoadingProgress * static_cast<float>(kBarWidth))),
            0,
            kBarWidth);
        const std::string bar = "["
            + std::string(static_cast<std::size_t>(fillChars), '=')
            + std::string(static_cast<std::size_t>(kBarWidth - fillChars), ' ')
            + "]";
        dbgTextPrintfCenteredRow(13, 0x0a, bar);
        dbgTextPrintfCenteredRow(footerRow, 0x07, "Preparing world...");
        return;
    }

    const MainMenuComputedLayout menu = computeMainMenuLayout(tw, th);
    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(menu.subtitleRow), 0x07, "DESKTOP EDITION");

    const int ruleWidth = std::clamp(menu.outerWidth, 24, tw - 4);
    const std::string ruleLine(static_cast<std::size_t>(ruleWidth), '-');
    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(menu.ruleRow), 0x08, ruleLine);

    const int hovered = frameDebugData.mainMenuHoveredButton;
    static constexpr const char* const kMainMenuLabels[5] = {
        "Singleplayer",
        "Multiplayer",
        "VibeCraft Realms  * !",
        "Options...",
        "Quit game",
    };
    for (int i = 0; i < 5; ++i)
    {
        drawMainMenuFramedButton5(
            menu.buttonTopRows[static_cast<std::size_t>(i)],
            menu.centerCol,
            menu.outerWidth,
            menu.buttonLineCount,
            kMainMenuLabels[static_cast<std::size_t>(i)],
            hovered == i);
    }

    const std::uint16_t iconAttrG = hovered == 5 ? 0x0f : 0x07;
    const std::uint16_t iconAttrA = hovered == 6 ? 0x0f : 0x07;
    if (menu.centerCol >= 7)
    {
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(menu.centerCol - 6),
            static_cast<std::uint16_t>(menu.iconHintsRow),
            iconAttrG,
            "[G]");
    }
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(menu.centerCol + menu.outerWidth - 3),
        static_cast<std::uint16_t>(menu.iconHintsRow),
        iconAttrA,
        "[A]");

    const bool splashBright =
        static_cast<int>(frameDebugData.mainMenuTimeSeconds * 3.0f) % 2 == 0;
    const std::uint16_t splashAttr = splashBright ? 0x07 : 0x08;
    const std::string splash = "Also try building!";
    const int splashRow =
        std::clamp(std::max(th - 5, menu.iconHintsRow + 2), 0, std::max(0, th - 3));
    const int splashCol = std::max(0, tw - static_cast<int>(splash.size()) - 2);
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(splashCol), static_cast<std::uint16_t>(splashRow), splashAttr, "%s", splash.c_str());

    if (!frameDebugData.mainMenuNotice.empty() && footerRow >= 3)
    {
        const std::uint16_t noticeRow =
            footerRow >= 26 ? static_cast<std::uint16_t>(footerRow - 2) : static_cast<std::uint16_t>(0);
        dbgTextPrintfCenteredRow(noticeRow, 0x07, frameDebugData.mainMenuNotice);
    }

    dbgTextPrintfCenteredRow(footerRow, 0x07, "Tab: capture mouse   Esc: pause menu");
}

void drawPauseMenuFramedButton(
    const int rowTop,
    const int col,
    const int outerWidth,
    const std::string& label,
    const bool hovered)
{
    // High-contrast VGA attrs: 0x1f white-on-blue; hover 0x3f white-on-cyan for the whole control.
    const std::uint16_t borderAttr = hovered ? static_cast<std::uint16_t>(0x3f) : static_cast<std::uint16_t>(0x1f);
    const std::uint16_t labelAttr = hovered ? static_cast<std::uint16_t>(0x3f) : static_cast<std::uint16_t>(0x1f);
    const int inner = outerWidth - 2;
    const std::string border = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
    const std::string mid = "|" + padLabelToInnerWidth(label, inner) + "|";
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(rowTop), borderAttr, "%s", border.c_str());
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(rowTop + 1), labelAttr, "%s", mid.c_str());
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(rowTop + 2), borderAttr, "%s", border.c_str());
}

void drawPauseMenuOverlay(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    constexpr int kWide = PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);

    if (frameDebugData.pauseSoundSettingsActive)
    {
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(PauseMenuLayout::kSoundTitleRow), 0x1f, " SOUND SETTINGS ");
        const int hovered = frameDebugData.pauseSoundSettingsHoveredControl;
        const int musicPercent = static_cast<int>(std::round(std::clamp(frameDebugData.pauseSoundMusicVolume, 0.0f, 1.0f) * 100.0f));
        const int sfxPercent = static_cast<int>(std::round(std::clamp(frameDebugData.pauseSoundSfxVolume, 0.0f, 1.0f) * 100.0f));
        drawPauseMenuFramedButton(
            PauseMenuLayout::kSoundMusicButtonRow,
            centerCol,
            kWide,
            fmt::format("Music volume: {:3d}%   [-] [+]", musicPercent),
            hovered == 1 || hovered == 2);
        drawPauseMenuFramedButton(
            PauseMenuLayout::kSoundSfxButtonRow,
            centerCol,
            kWide,
            fmt::format("SFX volume:   {:3d}%   [-] [+]", sfxPercent),
            hovered == 3 || hovered == 4);
        drawPauseMenuFramedButton(
            PauseMenuLayout::kSoundBackButtonRow,
            centerCol,
            kWide,
            "Back",
            hovered == 0);
    }
    else if (frameDebugData.pauseGameSettingsActive)
    {
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(PauseMenuLayout::kGameTitleRow), 0x1f, " GAME OPTIONS ");
        const int hovered = frameDebugData.pauseGameSettingsHoveredControl;
        const char* const mobState = frameDebugData.mobSpawningEnabled ? "ON" : "OFF";
        drawPauseMenuFramedButton(
            PauseMenuLayout::kGameMobButtonRow,
            centerCol,
            kWide,
            fmt::format("Mob spawning: {}", mobState),
            hovered == 1);
        drawPauseMenuFramedButton(
            PauseMenuLayout::kGameBackButtonRow,
            centerCol,
            kWide,
            "Back",
            hovered == 0);
    }
    else
    {
        const int firstButtonRow = PauseMenuLayout::mainPauseMenuFirstButtonRow(static_cast<int>(textHeight));
        const int titleRow = std::max(2, firstButtonRow - 3);
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(titleRow), 0x1f, " GAME MENU ");

        const int hovered = frameDebugData.pauseMenuHoveredButton;
        constexpr int kPitch = PauseMenuLayout::kButtonPitch;
        drawPauseMenuFramedButton(firstButtonRow + 0 * kPitch, centerCol, kWide, "Back to game", hovered == 0);
        drawPauseMenuFramedButton(firstButtonRow + 1 * kPitch, centerCol, kWide, "Sound settings...", hovered == 1);
        drawPauseMenuFramedButton(firstButtonRow + 2 * kPitch, centerCol, kWide, "Quit to title", hovered == 2);
        drawPauseMenuFramedButton(firstButtonRow + 3 * kPitch, centerCol, kWide, "Quit game", hovered == 3);
        drawPauseMenuFramedButton(firstButtonRow + 4 * kPitch, centerCol, kWide, "Game options...", hovered == 4);
    }

    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    if (!frameDebugData.pauseMenuNotice.empty() && footerRow >= 3)
    {
        const std::uint16_t noticeRow =
            footerRow >= 26 ? static_cast<std::uint16_t>(footerRow - 2) : static_cast<std::uint16_t>(0);
        dbgTextPrintfCenteredRow(noticeRow, 0x0b, frameDebugData.pauseMenuNotice);
    }
    const char* footerHint = "Esc: back to game   Click: choose";
    if (frameDebugData.pauseSoundSettingsActive)
    {
        footerHint = "Esc: back   Click: adjust";
    }
    else if (frameDebugData.pauseGameSettingsActive)
    {
        footerHint = "Esc: back   Click: toggle";
    }
    dbgTextPrintfCenteredRow(footerRow, 0x0f, footerHint);
}

} // namespace vibecraft::render::detail
