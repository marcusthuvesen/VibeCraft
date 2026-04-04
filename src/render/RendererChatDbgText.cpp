#include "vibecraft/render/RendererDetail.hpp"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <string>

namespace vibecraft::render::detail
{
namespace
{
[[nodiscard]] std::string clampChatLine(const std::string& text, const std::size_t maxChars)
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

void drawChatFrame(
    const int row,
    const int col,
    const int outerWidth,
    const std::uint16_t borderAttr,
    const std::uint16_t fillAttr)
{
    if (outerWidth < 2)
    {
        return;
    }

    const int innerWidth = outerWidth - 2;
    const std::string border = "+" + std::string(static_cast<std::size_t>(innerWidth), '-') + "+";
    const std::string middle = "|" + std::string(static_cast<std::size_t>(innerWidth), ' ') + "|";
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), borderAttr, "%s", border.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col),
        static_cast<std::uint16_t>(row + 1),
        fillAttr,
        "%s",
        middle.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col),
        static_cast<std::uint16_t>(row + 2),
        borderAttr,
        "%s",
        border.c_str());
}
}  // namespace

void drawChatOverlay(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    if (textWidth < 24 || textHeight < 8)
    {
        return;
    }

    const int maxBubbleWidth = std::clamp(static_cast<int>(textWidth) - 4, 24, 72);
    const int promptWidth = std::clamp(maxBubbleWidth, 24, static_cast<int>(textWidth) - 2);
    const int inputRow = std::max(4, static_cast<int>(textHeight) - 4);
    const int historyStartRow = std::max(2, inputRow - static_cast<int>(frameDebugData.chatLines.size()) - 1);

    for (std::size_t i = 0; i < frameDebugData.chatLines.size(); ++i)
    {
        const FrameDebugData::ChatLineHud& line = frameDebugData.chatLines[i];
        const std::uint16_t attr = line.isError ? static_cast<std::uint16_t>(0x0c) : static_cast<std::uint16_t>(0x0f);
        const std::string prefix = line.isError ? "! " : "  ";
        bgfx::dbgTextPrintf(
            1,
            static_cast<std::uint16_t>(historyStartRow + static_cast<int>(i)),
            attr,
            "%s",
            clampChatLine(prefix + line.text, static_cast<std::size_t>(maxBubbleWidth)).c_str());
    }

    if (!frameDebugData.chatOpen)
    {
        return;
    }

    drawChatFrame(inputRow - 1, 0, promptWidth + 2, 0x1f, 0x17);
    const std::string prompt =
        clampChatLine("> " + frameDebugData.chatInputLine, static_cast<std::size_t>(promptWidth));
    bgfx::dbgTextPrintf(1, static_cast<std::uint16_t>(inputRow), 0x0f, "%s", prompt.c_str());
}
}  // namespace vibecraft::render::detail
