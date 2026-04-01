#include "vibecraft/render/RendererDetail.hpp"

#include <bgfx/bgfx.h>

namespace vibecraft::render::detail
{
void drawCoordinateOverlay(const FrameDebugData& frameDebugData)
{
    if (!frameDebugData.showWorldOriginGuides)
    {
        return;
    }

    bgfx::dbgTextPrintf(0, 8, 0x0f, "%s", frameDebugData.debugCoordinatesLine.c_str());
    bgfx::dbgTextPrintf(0, 9, 0x0f, "%s", frameDebugData.debugBlockLine.c_str());
    bgfx::dbgTextPrintf(0, 10, 0x0f, "%s", frameDebugData.debugChunkLine.c_str());
    bgfx::dbgTextPrintf(0, 11, 0x0f, "%s", frameDebugData.debugFacingLine.c_str());
    bgfx::dbgTextPrintf(0, 12, 0x0f, "%s", frameDebugData.debugBiomeLine.c_str());
}
}  // namespace vibecraft::render::detail
