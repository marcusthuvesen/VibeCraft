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

[[nodiscard]] int mainMenuLogoReservedDbgRows(
    const std::uint32_t windowWidth,
    const std::uint32_t windowHeight,
    const std::uint16_t textHeight,
    const std::uint16_t logoWidthPx,
    const std::uint16_t logoHeightPx)
{
    if (windowWidth == 0 || windowHeight == 0 || textHeight == 0 || logoWidthPx == 0 || logoHeightPx == 0)
    {
        return 0;
    }

    const float aspect = static_cast<float>(logoWidthPx) / static_cast<float>(logoHeightPx);
    constexpr float kMarginTop = 32.0f;
    const float maxWidth = std::min(640.0f, static_cast<float>(windowWidth) * 0.82f);
    float drawW = maxWidth;
    float drawH = drawW / aspect;
    const float maxHeight = std::min(static_cast<float>(windowHeight) * 0.17f, 200.0f);
    if (drawH > maxHeight)
    {
        drawH = maxHeight;
        drawW = drawH * aspect;
    }

    const float logoBottomPx = kMarginTop + drawH;
    const float cellHeightPx = static_cast<float>(windowHeight) / static_cast<float>(textHeight);
    const int rowsOccupied =
        static_cast<int>(std::ceil(logoBottomPx / std::max(cellHeightPx, 1.0f))) + 1;
    return std::clamp(rowsOccupied, 0, static_cast<int>(textHeight) - 1);
}

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
        const float equipmentSectionGap = std::max(14.0f, std::round(candidateSlotSize * 0.65f));
        const float equipmentSectionWidth = candidateSlotSize * 1.7f;
        const float topSectionWidth =
            equipmentSectionWidth + equipmentSectionGap + craftWidth + resultGap + candidateSlotSize;
        const float panelWidth = std::max(inventoryWidth, topSectionWidth) + candidateSlotSize * 1.8f;
        const float craftingSectionHeight = static_cast<float>(craftingRows) * candidateSlotSize
            + static_cast<float>(craftingRows - 1) * candidateGap;
        const float equipmentSectionHeight = 5.0f * candidateSlotSize + 4.0f * candidateGap;
        const float topSectionHeight = std::max(craftingSectionHeight, equipmentSectionHeight);
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
    const float craftingSectionHeight = static_cast<float>(craftingRows) * slotSize
        + static_cast<float>(craftingRows - 1) * slotGap;
    const float equipmentSectionHeight = 5.0f * slotSize + 4.0f * slotGap;
    const float topSectionHeight = std::max(craftingSectionHeight, equipmentSectionHeight);
    const float panelLeft = std::floor((static_cast<float>(windowWidth) - panelWidth) * 0.5f);
    const float panelTop = std::floor((static_cast<float>(windowHeight) - panelHeight) * 0.5f);
    const float panelInnerX = panelLeft + slotSize * 0.9f;
    const float equipmentSectionGap = std::max(14.0f, std::round(slotSize * 0.65f));
    const float equipmentSectionWidth = slotSize * 1.7f;
    const float topSectionOriginX = panelInnerX + std::floor((panelWidth - slotSize * 1.8f - topSectionWidth) * 0.5f);
    const float equipmentOriginX = topSectionOriginX;
    const float craftingOriginX = equipmentOriginX + equipmentSectionWidth + equipmentSectionGap;
    const float craftingOriginY = panelTop + slotSize * 1.5f;
    const float resultSlotX = craftingOriginX + craftWidth + resultGap;
    const float resultSlotY = craftingOriginY + (craftingSectionHeight - slotSize) * 0.5f;
    const float inventoryOriginX = panelInnerX;
    const float inventoryOriginY = craftingOriginY + topSectionHeight + slotSize * 1.55f;

    layout.panelLeft = panelLeft;
    layout.panelTop = panelTop;
    layout.panelRight = panelLeft + panelWidth;
    layout.panelBottom = panelTop + panelHeight;
    layout.slotSize = slotSize;
    layout.slotGap = slotGap;
    layout.equipmentOriginX = equipmentOriginX;
    layout.equipmentOriginY = craftingOriginY;
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

[[nodiscard]] std::string sliderFillBar(const float value)
{
    const int fillChars = detail::PauseMenuLayout::kSoundSliderFillChars;
    const int filled = std::clamp(
        static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(fillChars))),
        0,
        fillChars);
    return std::string(static_cast<std::size_t>(filled), '=')
        + std::string(static_cast<std::size_t>(fillChars - filled), '-');
}

[[nodiscard]] std::string buildPauseSoundSliderLabel(const std::string& prefix, const int percent, const float value)
{
    const int innerWidth = detail::PauseMenuLayout::kWideChars - 2;
    std::string line(static_cast<std::size_t>(std::max(0, innerWidth)), ' ');
    const std::string left = fmt::format("{} {:3d}% ", prefix, percent);
    const std::string bar = "[" + sliderFillBar(value) + "]";

    for (std::size_t i = 0; i < left.size() && i < line.size(); ++i)
    {
        line[i] = left[i];
    }
    const int barStart = std::clamp(
        detail::PauseMenuLayout::kSoundSliderFillStartInner - 1,
        0,
        std::max(0, innerWidth - static_cast<int>(bar.size())));
    for (std::size_t i = 0; i < bar.size() && static_cast<int>(i) + barStart < static_cast<int>(line.size()); ++i)
    {
        line[static_cast<std::size_t>(barStart) + i] = bar[i];
    }
    return line;
}

[[nodiscard]] MainMenuComputedLayout computeMainMenuLayout(
    const int textWidth, const int textHeight, const int contentTopRowOffset)
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
    const int centeredBase = (textHeight - contentRows) / 2;
    layout.firstContentRow = std::clamp(
        centeredBase + contentTopRowOffset,
        1,
        std::max(1, textHeight - contentRows));

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

void drawTextFrame(
    const int row,
    const int col,
    const int outerWidth,
    const int outerHeight,
    const std::uint16_t borderAttr,
    const std::uint16_t fillAttr)
{
    if (outerWidth < 2 || outerHeight < 2)
    {
        return;
    }

    const int innerWidth = outerWidth - 2;
    const std::string border = "+" + std::string(static_cast<std::size_t>(innerWidth), '-') + "+";
    const std::string middle = "|" + std::string(static_cast<std::size_t>(innerWidth), ' ') + "|";
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), borderAttr, "%s", border.c_str());
    for (int innerRow = 0; innerRow < outerHeight - 2; ++innerRow)
    {
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(col),
            static_cast<std::uint16_t>(row + 1 + innerRow),
            fillAttr,
            "%s",
            middle.c_str());
    }
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col),
        static_cast<std::uint16_t>(row + outerHeight - 1),
        borderAttr,
        "%s",
        border.c_str());
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
    const std::uint16_t textHeight,
    const int mainMenuTitleContentRowOffset)
{
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const MainMenuComputedLayout menu = computeMainMenuLayout(tw, th, mainMenuTitleContentRowOffset);
    const int centerCol = menu.centerCol;
    const int outerWidth = menu.outerWidth;
    const int btnLines = menu.buttonLineCount;
    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    constexpr std::uint16_t kMenuTitle = 0x07;
    constexpr std::uint16_t kMenuMuted = 0x08;

    const int hovered = frameDebugData.mainMenuMultiplayerHoveredControl;

    switch (frameDebugData.mainMenuMultiplayerPanel)
    {
    case FrameDebugData::MainMenuMultiplayerPanel::Hub:
    {
        const int rowShift = MultiplayerMenuLayout::multiplayerMenuRowShift(
            th, FrameDebugData::MainMenuMultiplayerPanel::Hub, 0, mainMenuTitleContentRowOffset);
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(5 + rowShift), kMenuTitle, "MULTIPLAYER");
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(7 + rowShift), kMenuTitle, "Play with friends on the same Wi-Fi");
        drawMainMenuFramedButton5(
            MultiplayerMenuLayout::kHubHostRow + rowShift,
            centerCol,
            outerWidth,
            btnLines,
            "Host game",
            hovered == 0);
        drawMainMenuFramedButton5(
            MultiplayerMenuLayout::kHubJoinRow + rowShift,
            centerCol,
            outerWidth,
            btnLines,
            "Join game",
            hovered == 1);
        drawMainMenuFramedButton5(
            MultiplayerMenuLayout::kHubBackRow + rowShift,
            centerCol,
            outerWidth,
            btnLines,
            "Back",
            hovered == 2);
        dbgTextPrintfCenteredRow(footerRow, kMenuTitle, "Esc: back to title");
        break;
    }

    case FrameDebugData::MainMenuMultiplayerPanel::Host:
    {
        const int rowShift = MultiplayerMenuLayout::multiplayerMenuRowShift(
            th, FrameDebugData::MainMenuMultiplayerPanel::Host, 0, mainMenuTitleContentRowOffset);
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(5 + rowShift), kMenuTitle, "HOST MULTIPLAYER");
        if (frameDebugData.mainMenuMultiplayerLanAddress.empty())
        {
            dbgTextPrintfCenteredRow(
                static_cast<std::uint16_t>(8 + rowShift),
                kMenuTitle,
                clampDbgTextLine("LAN IP not detected — check Wi-Fi IP in System Settings", static_cast<std::size_t>(tw - 2)));
        }
        else
        {
            dbgTextPrintfCenteredRow(
                static_cast<std::uint16_t>(8 + rowShift),
                kMenuTitle,
                clampDbgTextLine(
                    fmt::format(
                        "Friend on Wi-Fi: {}:{}",
                        frameDebugData.mainMenuMultiplayerLanAddress,
                        frameDebugData.mainMenuMultiplayerPortDisplay),
                    static_cast<std::size_t>(tw - 2)));
        }
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(9 + rowShift), kMenuMuted, "Same machine test: 127.0.0.1");
        drawMainMenuFramedButton5(
            MultiplayerMenuLayout::kHostStartRow + rowShift,
            centerCol,
            outerWidth,
            btnLines,
            "Start hosting",
            hovered == 0);
        drawMainMenuFramedButton5(
            MultiplayerMenuLayout::kHostBackRow + rowShift,
            centerCol,
            outerWidth,
            btnLines,
            "Back",
            hovered == 1);
        dbgTextPrintfCenteredRow(footerRow, kMenuTitle, "Esc: back");
        break;
    }

    case FrameDebugData::MainMenuMultiplayerPanel::Join:
    {
        const int presetSlots =
            MultiplayerMenuLayout::joinPresetSlotCountForLayout(frameDebugData.mainMenuJoinPresetButtonLabels.size());
        const int rowShift = MultiplayerMenuLayout::multiplayerMenuRowShift(
            th, FrameDebugData::MainMenuMultiplayerPanel::Join, presetSlots, mainMenuTitleContentRowOffset);
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(5 + rowShift), kMenuTitle, "JOIN MULTIPLAYER");
        for (int i = 0; i < presetSlots; ++i)
        {
            const std::string label =
                clampDbgTextLine(frameDebugData.mainMenuJoinPresetButtonLabels[static_cast<std::size_t>(i)], 80);
            drawMainMenuFramedButton5(
                MultiplayerMenuLayout::joinPresetButtonStartRow(i) + rowShift,
                centerCol,
                outerWidth,
                btnLines,
                label,
                hovered == i);
        }

        int addrFieldRow = 0;
        int portLabelRow = 0;
        int portFieldRow = 0;
        int connectRow = 0;
        int backRow = 0;
        MultiplayerMenuLayout::joinManualSectionRows(presetSlots, addrFieldRow, portLabelRow, portFieldRow, connectRow, backRow);
        addrFieldRow += rowShift;
        portLabelRow += rowShift;
        portFieldRow += rowShift;
        connectRow += rowShift;
        backRow += rowShift;

        const int addrHoverId = presetSlots;
        const int portHoverId = presetSlots + 1;
        const int connectHoverId = presetSlots + 2;
        const int backHoverId = presetSlots + 3;

        const int maxInner = std::max(8, outerWidth - 2);
        const int addrLabelRow = std::max(1, addrFieldRow - 1);
        const std::uint16_t addrLabelAttr =
            frameDebugData.mainMenuJoinFocusedField == 0 ? 0x3f : kMenuTitle;
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(centerCol),
            static_cast<std::uint16_t>(addrLabelRow),
            addrLabelAttr,
            "%s",
            "Host address (click to edit)");
        drawMainMenuFramedButton5(
            addrFieldRow,
            centerCol,
            outerWidth,
            MultiplayerMenuLayout::kFieldButtonLineCount,
            clampDbgTextLine(frameDebugData.mainMenuJoinAddressField, static_cast<std::size_t>(maxInner)),
            hovered == addrHoverId || frameDebugData.mainMenuJoinFocusedField == 0);
        const std::uint16_t portLabelAttr =
            frameDebugData.mainMenuJoinFocusedField == 1 ? 0x3f : kMenuTitle;
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(centerCol),
            static_cast<std::uint16_t>(portLabelRow),
            portLabelAttr,
            "%s",
            "Port (click to edit)");
        drawMainMenuFramedButton5(
            portFieldRow,
            centerCol,
            outerWidth,
            MultiplayerMenuLayout::kFieldButtonLineCount,
            clampDbgTextLine(frameDebugData.mainMenuJoinPortField, static_cast<std::size_t>(maxInner)),
            hovered == portHoverId || frameDebugData.mainMenuJoinFocusedField == 1);
        drawMainMenuFramedButton5(
            connectRow,
            centerCol,
            outerWidth,
            btnLines,
            "Connect",
            hovered == connectHoverId);
        drawMainMenuFramedButton5(
            backRow,
            centerCol,
            outerWidth,
            btnLines,
            "Back",
            hovered == backHoverId);
        dbgTextPrintfCenteredRow(
            footerRow,
            kMenuTitle,
            "Esc: back   Tab: field   Type: IP/port   Presets: connect immediately");
        break;
    }

    case FrameDebugData::MainMenuMultiplayerPanel::None:
    default:
        break;
    }
}

void drawMainMenuOverlay(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight,
    const int mainMenuTitleContentRowOffset)
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
        drawMainMenuMultiplayerOverlay(frameDebugData, textWidth, textHeight, mainMenuTitleContentRowOffset);
        return;
    }

    if (frameDebugData.mainMenuLoadingActive)
    {
        const int percent =
            static_cast<int>(std::round(std::clamp(frameDebugData.mainMenuLoadingProgress, 0.0f, 1.0f) * 100.0f));
        const int panelWidth = std::clamp(tw - 12, 40, std::max(40, tw - 4));
        const int panelHeight = std::min(13, std::max(11, th - 2));
        const int panelCol = std::max(0, (tw - panelWidth) / 2);
        const int panelRow = std::max(1, (th - panelHeight) / 2);
        const int panelInnerWidth = panelWidth - 2;
        const int titleRow = panelRow + 2;
        const int labelRow = panelRow + 5;
        const int percentRow = panelRow + 7;
        const int barRow = panelRow + 9;

        drawTextFrame(panelRow, panelCol, panelWidth, panelHeight, 0x1f, 0x17);
        const std::string loadingTitle = frameDebugData.mainMenuLoadingTitle.empty()
            ? std::string(" LOADING WORLD ")
            : " " + frameDebugData.mainMenuLoadingTitle + " ";
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(panelCol),
            static_cast<std::uint16_t>(titleRow),
            0x3f,
            "%s",
            ("|" + padLabelToInnerWidth(clampDbgTextLine(loadingTitle, panelInnerWidth), panelInnerWidth) + "|").c_str());
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(panelCol),
            static_cast<std::uint16_t>(labelRow),
            0x17,
            "%s",
            ("|" + padLabelToInnerWidth(clampDbgTextLine(frameDebugData.mainMenuLoadingLabel, panelInnerWidth), panelInnerWidth) + "|").c_str());
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(panelCol),
            static_cast<std::uint16_t>(percentRow),
            0x3f,
            "%s",
            ("|" + padLabelToInnerWidth(fmt::format("{}%", percent), panelInnerWidth) + "|").c_str());

        const int barWidth = std::clamp(panelInnerWidth - 10, 24, 56);
        const int fillChars = std::clamp(
            static_cast<int>(std::round(frameDebugData.mainMenuLoadingProgress * static_cast<float>(barWidth))),
            0,
            barWidth);
        const std::string bar = "["
            + std::string(static_cast<std::size_t>(fillChars), '=')
            + std::string(static_cast<std::size_t>(barWidth - fillChars), ' ')
            + "]";
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(panelCol),
            static_cast<std::uint16_t>(barRow),
            0x2f,
            "%s",
            ("|" + padLabelToInnerWidth(bar, panelInnerWidth) + "|").c_str());
        return;
    }

    if (frameDebugData.mainMenuSingleplayerPanelActive)
    {
        constexpr int kWide = 72;
        constexpr int kPanelHeight = 31;
        const int panelCol = std::max(0, (tw - kWide) / 2);
        const int panelRow = std::max(1, (th - kPanelHeight) / 2);
        const int hovered = frameDebugData.mainMenuSingleplayerHoveredControl;
        drawTextFrame(panelRow, panelCol, kWide, kPanelHeight, 0x1f, 0x17);
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(panelRow + 2), 0x3f, " SINGLEPLAYER ");
        dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(panelRow + 4), 0x07, "Choose how to start");
        dbgTextPrintfCenteredRow(
            static_cast<std::uint16_t>(panelRow + 6),
            0x0f,
            clampDbgTextLine(
                fmt::format(
                    "Selected world: {}",
                    frameDebugData.mainMenuSelectedWorldLabel.empty() ? std::string("No world selected")
                                                                      : frameDebugData.mainMenuSelectedWorldLabel),
                tw - 2));
        drawFramedButton3(panelRow + 9, panelCol, kWide, "Start from saved world", hovered == 0, 0x17, 0x17, 0x3f, 0x3f);
        drawFramedButton3(panelRow + 16, panelCol, kWide, "Start new world", hovered == 1, 0x17, 0x17, 0x3f, 0x3f);
        drawFramedButton3(panelRow + 23, panelCol, kWide, "Back", hovered == 2, 0x17, 0x17, 0x3f, 0x3f);
        dbgTextPrintfCenteredRow(
            footerRow,
            0x07,
            "Click: select option   Esc: back");
        return;
    }

    const MainMenuComputedLayout menu = computeMainMenuLayout(tw, th, mainMenuTitleContentRowOffset);
    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(menu.subtitleRow), 0x07, "DESKTOP EDITION");

    const int ruleWidth = std::clamp(menu.outerWidth, 24, tw - 4);
    const std::string ruleLine(static_cast<std::size_t>(ruleWidth), '-');
    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(menu.ruleRow), 0x08, ruleLine);

    const int hovered = frameDebugData.mainMenuHoveredButton;
    const std::array<std::string, 5> mainMenuLabels{
        "Singleplayer",
        "Multiplayer",
        fmt::format("Creative mode: {}", frameDebugData.mainMenuCreativeModeEnabled ? "ON" : "OFF"),
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
            mainMenuLabels[static_cast<std::size_t>(i)],
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
            "[C]");
    }
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(menu.centerCol + menu.outerWidth - 3),
        static_cast<std::uint16_t>(menu.iconHintsRow),
        iconAttrA,
        "[V]");

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

    if (!frameDebugData.mainMenuSelectedWorldLabel.empty() && footerRow >= 4)
    {
        const std::uint16_t worldRow =
            footerRow >= 28 ? static_cast<std::uint16_t>(footerRow - 3) : static_cast<std::uint16_t>(1);
        dbgTextPrintfCenteredRow(
            worldRow,
            0x0f,
            clampDbgTextLine(fmt::format("Selected world: {}", frameDebugData.mainMenuSelectedWorldLabel), tw - 2));
    }

    dbgTextPrintfCenteredRow(
        footerRow,
        0x07,
        fmt::format(
            "Spawn: {}   C/[C]: creative   V/[V]: cycle spawn",
            frameDebugData.mainMenuSpawnPresetLabel.empty() ? "Origin" : frameDebugData.mainMenuSpawnPresetLabel));
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
    const int th = static_cast<int>(textHeight);
    constexpr int kWide = PauseMenuLayout::kWideChars;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);

    if (frameDebugData.pauseSoundSettingsActive)
    {
        dbgTextPrintfCenteredRow(
            static_cast<std::uint16_t>(PauseMenuLayout::pauseSoundTitleRow(th)),
            0x1f,
            " SOUND SETTINGS ");
        const int hovered = frameDebugData.pauseSoundSettingsHoveredControl;
        const int musicPercent = static_cast<int>(std::round(std::clamp(frameDebugData.pauseSoundMusicVolume, 0.0f, 1.0f) * 100.0f));
        const int sfxPercent = static_cast<int>(std::round(std::clamp(frameDebugData.pauseSoundSfxVolume, 0.0f, 1.0f) * 100.0f));
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseSoundMusicButtonRow(th),
            centerCol,
            kWide,
            buildPauseSoundSliderLabel("Music volume:", musicPercent, frameDebugData.pauseSoundMusicVolume),
            hovered == 1);
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseSoundSfxButtonRow(th),
            centerCol,
            kWide,
            buildPauseSoundSliderLabel("SFX volume:", sfxPercent, frameDebugData.pauseSoundSfxVolume),
            hovered == 3);
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseSoundBackButtonRow(th),
            centerCol,
            kWide,
            "Back",
            hovered == 0);
    }
    else if (frameDebugData.pauseGameSettingsActive)
    {
        dbgTextPrintfCenteredRow(
            static_cast<std::uint16_t>(PauseMenuLayout::pauseGameTitleRow(th)),
            0x1f,
            " GAME OPTIONS ");
        const int hovered = frameDebugData.pauseGameSettingsHoveredControl;
        const char* const mobState = frameDebugData.mobSpawningEnabled ? "ON" : "OFF";
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseGameMobButtonRow(th),
            centerCol,
            kWide,
            fmt::format("Mob spawning: {}", mobState),
            hovered == 1);
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseGameBiomeButtonRow(th),
            centerCol,
            kWide,
            fmt::format(
                "Biome preset: {}",
                frameDebugData.pauseSpawnBiomeLabel.empty() ? "Any" : frameDebugData.pauseSpawnBiomeLabel),
            hovered == 2);
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseGameWeatherButtonRow(th),
            centerCol,
            kWide,
            fmt::format(
                "Weather: {}",
                frameDebugData.pauseWeatherLabel.empty() ? "Unknown" : frameDebugData.pauseWeatherLabel),
            hovered == 3);
        drawPauseMenuFramedButton(
            PauseMenuLayout::pauseGameBackButtonRow(th),
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
        drawPauseMenuFramedButton(firstButtonRow + 2 * kPitch, centerCol, kWide, "Game options...", hovered == 2);
        drawPauseMenuFramedButton(firstButtonRow + 3 * kPitch, centerCol, kWide, "Quit to title", hovered == 3);
        drawPauseMenuFramedButton(firstButtonRow + 4 * kPitch, centerCol, kWide, "Quit game", hovered == 4);
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
