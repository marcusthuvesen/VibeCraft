#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <fmt/format.h>
#include <glm/common.hpp>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
namespace
{
constexpr float kCraftingTileInsetU = 0.5f / static_cast<float>(kChunkAtlasWidthPx);
constexpr float kCraftingTileInsetV = 0.5f / static_cast<float>(kChunkAtlasHeightPx);
}

void Renderer::drawCraftingOverlay(const FrameDebugData& frameDebugData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX
        || chunkAtlasTextureHandle_ == UINT16_MAX)
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

    const detail::CraftingOverlayLayoutPx layout =
        detail::computeCraftingOverlayLayoutPx(
            width_,
            height_,
            frameDebugData.craftingUiMode,
            frameDebugData.craftingUsesWorkbench);
    if (layout.slotSize <= 0.0f)
    {
        return;
    }

    const bgfx::Stats* const stats = bgfx::getStats();
    const std::uint16_t textWidth =
        stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
    const std::uint16_t textHeight = stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;
    const float charWidthPx = static_cast<float>(width_) / static_cast<float>(std::max<std::uint16_t>(1, textWidth));
    const float charHeightPx = static_cast<float>(height_) / static_cast<float>(std::max<std::uint16_t>(1, textHeight));
    const bool inventoryMode = frameDebugData.craftingUiMode == CraftingUiMode::Inventory;
    const bool workbenchMode = frameDebugData.craftingUiMode == CraftingUiMode::Workbench;
    const bool chestMode = frameDebugData.craftingUiMode == CraftingUiMode::Chest;
    const bool furnaceMode = frameDebugData.craftingUiMode == CraftingUiMode::Furnace;
    const std::uint16_t containerBackgroundTextureHandle =
        inventoryMode
        ? craftingContainerInventoryTextureHandle_
        : (workbenchMode
               ? craftingContainerWorkbenchTextureHandle_
               : (chestMode ? craftingContainerChestTextureHandle_ : craftingContainerFurnaceTextureHandle_));
    const bool hasContainerBackground = containerBackgroundTextureHandle != UINT16_MAX;
    const auto drawUiTextureRectUv = [&](const float x0,
                                         const float y0,
                                         const float x1,
                                         const float y1,
                                         const std::uint16_t textureHandle,
                                         const TextureUvRect& uvRect)
    {
        if (textureHandle == UINT16_MAX)
        {
            return;
        }
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return;
        }

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.minU, .v = uvRect.minV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.maxU, .v = uvRect.minV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.maxU, .v = uvRect.maxV, .abgr = 0xffffffff},
            detail::ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = uvRect.minU, .v = uvRect.maxV, .abgr = 0xffffffff},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;
        indices[3] = 0;
        indices[4] = 2;
        indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
        bgfx::submit(detail::kUiView, detail::toProgramHandle(inventoryUiProgramHandle_));
    };
    const bool canDrawSolid = inventoryUiSolidProgramHandle_ != UINT16_MAX;
    if (!canDrawSolid)
    {
        return;
    }

    const std::uint32_t dimAbgr =
        detail::packAbgr8(glm::vec3(0.01f, 0.01f, 0.01f), hasContainerBackground ? 0.46f : 0.56f);
    const std::uint32_t panelOuterAbgr = detail::packAbgr8(glm::vec3(0.08f, 0.08f, 0.08f), 0.95f);
    const std::uint32_t panelInnerAbgr = detail::packAbgr8(glm::vec3(0.20f, 0.20f, 0.20f), 0.95f);
    const std::uint32_t slotBorderAbgr = detail::packAbgr8(glm::vec3(0.10f, 0.10f, 0.10f), 0.97f);
    const std::uint32_t slotFillAbgr = detail::packAbgr8(glm::vec3(0.35f, 0.35f, 0.35f), 0.95f);
    const std::uint32_t slotTopHighlightAbgr = detail::packAbgr8(glm::vec3(0.48f, 0.48f, 0.48f), 0.90f);
    const std::uint32_t resultGlowAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.92f);
    const std::uint32_t cursorOutlineAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 0.95f);
    const auto drawStairAtlasIconInRect = [&](const float x0,
                                              const float y0,
                                              const float x1,
                                              const float y1,
                                              const std::uint8_t tileIndex)
    {
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        const TextureUvRect uvRect{
            .minU = tileX * tileWidth + kCraftingTileInsetU,
            .maxU = (tileX + 1.0f) * tileWidth - kCraftingTileInsetU,
            .minV = tileY * tileHeight + kCraftingTileInsetV,
            .maxV = (tileY + 1.0f) * tileHeight - kCraftingTileInsetV,
        };
        const float w = std::max(1.0f, x1 - x0);
        const float h = std::max(1.0f, y1 - y0);
        const float pad = std::max(1.0f, std::round(std::min(w, h) * 0.06f));
        const float iconX0 = x0 + pad;
        const float iconY0 = y0 + pad;
        const float iconX1 = x1 - pad;
        const float iconY1 = y1 - pad;
        const float iconW = iconX1 - iconX0;
        const float iconH = iconY1 - iconY0;
        drawUiTextureRectUv(
            iconX0,
            iconY0 + iconH * 0.50f,
            iconX1,
            iconY1,
            chunkAtlasTextureHandle_,
            uvRect);
        drawUiTextureRectUv(
            iconX0 + iconW * 0.44f,
            iconY0 + iconH * 0.16f,
            iconX1,
            iconY0 + iconH * 0.50f,
            chunkAtlasTextureHandle_,
            uvRect);
    };
    const auto drawAtlasIconInRect = [&](const float x0,
                                         const float y0,
                                         const float x1,
                                         const float y1,
                                         const std::uint8_t tileIndex)
    {
        const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
        const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
        const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
        const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
        drawUiTextureRectUv(
            x0,
            y0,
            x1,
            y1,
            chunkAtlasTextureHandle_,
            TextureUvRect{
                .minU = tileX * tileWidth + kCraftingTileInsetU,
                .maxU = (tileX + 1.0f) * tileWidth - kCraftingTileInsetU,
                .minV = tileY * tileHeight + kCraftingTileInsetV,
                .maxV = (tileY + 1.0f) * tileHeight - kCraftingTileInsetV,
            });
    };

    drawUiSolidRect(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), dimAbgr);
    if (hasContainerBackground)
    {
        const TextureUvRect containerBgUv =
            chestMode
            ? TextureUvRect{
                  .minU = 0.0f,
                  .maxU = 176.0f / 256.0f,
                  .minV = 0.0f,
                  .maxV = 222.0f / 256.0f,
              }
            : TextureUvRect{
                  .minU = 0.0f,
                  .maxU = 176.0f / 256.0f,
                  .minV = 0.0f,
                  .maxV = 166.0f / 256.0f,
              };
        drawUiTextureRectUv(
            layout.panelLeft,
            layout.panelTop,
            layout.panelRight,
            layout.panelBottom,
            containerBackgroundTextureHandle,
            containerBgUv);
    }
    if (!hasContainerBackground)
    {
        drawUiSolidRect(layout.panelLeft - 2.0f, layout.panelTop - 2.0f, layout.panelRight + 2.0f, layout.panelBottom + 2.0f, panelOuterAbgr);
        drawUiSolidRect(layout.panelLeft, layout.panelTop, layout.panelRight, layout.panelBottom, panelInnerAbgr);
    }

    const auto drawSlotFrame = [&](const float x, const float y, const bool highlight)
    {
        if (hasContainerBackground)
        {
            if (highlight)
            {
                drawUiSolidRect(
                    x - 1.0f,
                    y - 1.0f,
                    x + layout.slotSize + 1.0f,
                    y + layout.slotSize + 1.0f,
                    resultGlowAbgr);
            }
            return;
        }
        if (highlight)
        {
            drawUiSolidRect(x - 2.0f, y - 2.0f, x + layout.slotSize + 2.0f, y + layout.slotSize + 2.0f, resultGlowAbgr);
        }
        drawUiSolidRect(x, y, x + layout.slotSize, y + layout.slotSize, slotBorderAbgr);
        drawUiSolidRect(x + 1.0f, y + 1.0f, x + layout.slotSize - 1.0f, y + layout.slotSize - 1.0f, slotFillAbgr);
        drawUiSolidRect(
            x + 1.0f,
            y + 1.0f,
            x + layout.slotSize - 1.0f,
            y + std::max(2.0f, layout.slotSize * 0.22f),
            slotTopHighlightAbgr);
    };

    const auto drawSlotContents = [&](const FrameDebugData::HotbarSlotHud& slotHud, const float x, const float y)
    {
        if (slotHud.count == 0)
        {
            return;
        }

        const float iconInset = std::max(2.0f, std::round(layout.slotSize * 0.09f));
        const float iconMinX = x + iconInset;
        const float iconMinY = y + iconInset;
        const float iconMaxX = x + layout.slotSize - iconInset;
        const float iconMaxY = y + layout.slotSize - iconInset;
        const std::uint16_t textureHandle = hudItemKindTextureHandle(slotHud.itemKind);

        if (textureHandle != UINT16_MAX)
        {
            drawUiTextureRectUv(
                iconMinX,
                iconMinY,
                iconMaxX,
                iconMaxY,
                textureHandle,
                hudItemKindTextureUv(slotHud.itemKind));
        }
        else if (slotHud.blockType != vibecraft::world::BlockType::Air)
        {
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                slotHud.blockType,
                vibecraft::world::BlockFace::Side);
            if (vibecraft::world::isStairBlock(slotHud.blockType))
            {
                drawStairAtlasIconInRect(iconMinX, iconMinY, iconMaxX, iconMaxY, tileIndex);
            }
            else
            {
                drawAtlasIconInRect(iconMinX, iconMinY, iconMaxX, iconMaxY, tileIndex);
            }
        }

        if (slotHud.count > 1)
        {
            const std::string digits = fmt::format("{}", std::min(slotHud.count, 99u));
            const float textX1 = x + layout.slotSize - std::max(2.0f, layout.slotSize * 0.05f);
            const float textY1 = y + layout.slotSize - std::max(2.0f, layout.slotSize * 0.05f);
            int col = static_cast<int>(std::floor((textX1 - charWidthPx * 0.55f) / charWidthPx))
                - static_cast<int>(digits.size()) + 1;
            int row = static_cast<int>(std::floor((textY1 - charHeightPx * 0.95f) / charHeightPx));
            col = std::clamp(col, 0, static_cast<int>(textWidth) - static_cast<int>(digits.size()));
            row = std::clamp(row, 0, static_cast<int>(textHeight) - 1);
            bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), 0x0f, "%s", digits.c_str());
        }

        if (slotHud.durabilityMax > 0)
        {
            const float fraction = std::clamp(
                static_cast<float>(slotHud.durabilityRemaining) / static_cast<float>(slotHud.durabilityMax),
                0.0f,
                1.0f);
            const float trackH = std::max(3.0f, std::round(layout.slotSize * 0.08f));
            const float trackX0 = x + std::max(2.0f, std::round(layout.slotSize * 0.10f));
            const float trackX1 = x + layout.slotSize - std::max(2.0f, std::round(layout.slotSize * 0.10f));
            const float trackY1 = y + layout.slotSize - std::max(2.0f, std::round(layout.slotSize * 0.08f));
            const float trackY0 = trackY1 - trackH;
            const std::uint32_t trackAbgr = detail::packAbgr8(glm::vec3(0.04f, 0.05f, 0.06f), 0.95f);
            const glm::vec3 fillColor = glm::mix(
                glm::vec3(0.86f, 0.19f, 0.14f),
                glm::vec3(0.33f, 0.89f, 0.22f),
                fraction);
            const std::uint32_t fillAbgr = detail::packAbgr8(fillColor, 0.98f);
            drawUiSolidRect(trackX0, trackY0, trackX1, trackY1, trackAbgr);
            if (fraction > 0.001f)
            {
                drawUiSolidRect(trackX0, trackY0, trackX0 + (trackX1 - trackX0) * fraction, trackY1, fillAbgr);
            }
        }
    };

    constexpr std::array<const char*, 4> kEquipmentLabels{"HELM", "CHEST", "LEGS", "BOOTS"};
    if (inventoryMode)
    {
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.equipmentSlots.size(); ++slotIndex)
        {
            const float slotX = layout.equipmentOriginX;
            const float slotY = layout.equipmentOriginY + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
            drawSlotFrame(slotX, slotY, false);
            drawSlotContents(frameDebugData.equipmentSlots[slotIndex], slotX, slotY);

            const int labelCol = std::clamp(
                static_cast<int>(std::floor((slotX + layout.slotSize + layout.slotGap * 0.7f) / charWidthPx)),
                0,
                static_cast<int>(textWidth) - 1);
            const int labelRow = std::clamp(
                static_cast<int>(std::floor((slotY + layout.slotSize * 0.38f) / charHeightPx)),
                0,
                static_cast<int>(textHeight) - 1);
            if (!hasContainerBackground)
            {
                bgfx::dbgTextPrintf(
                    static_cast<std::uint16_t>(labelCol),
                    static_cast<std::uint16_t>(labelRow),
                    0x0e,
                    "%s",
                    kEquipmentLabels[slotIndex]);
            }
        }
    }

    const int craftingColumns = chestMode ? 9 : (workbenchMode ? 3 : 2);
    const int craftingRows = chestMode ? 3 : (workbenchMode ? 3 : 2);
    if (furnaceMode)
    {
        constexpr std::size_t kFurnaceInputGridIndex = 1;
        constexpr std::size_t kFurnaceFuelGridIndex = 7;
        drawSlotFrame(layout.craftingOriginX, layout.craftingOriginY, false);
        drawSlotContents(frameDebugData.craftingGridSlots[kFurnaceInputGridIndex], layout.craftingOriginX, layout.craftingOriginY);
        drawSlotFrame(layout.furnaceFuelSlotX, layout.furnaceFuelSlotY, false);
        drawSlotContents(frameDebugData.craftingGridSlots[kFurnaceFuelGridIndex], layout.furnaceFuelSlotX, layout.furnaceFuelSlotY);
    }
    else
    {
        for (int row = 0; row < craftingRows; ++row)
        {
            for (int col = 0; col < craftingColumns; ++col)
            {
                const std::size_t slotIndex = chestMode
                    ? static_cast<std::size_t>(row * 9 + col)
                    : static_cast<std::size_t>(row * 3 + col);
                const float slotX = layout.craftingOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
                const float slotY = layout.craftingOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
                drawSlotFrame(slotX, slotY, false);
                drawSlotContents(frameDebugData.craftingGridSlots[slotIndex], slotX, slotY);
            }
        }
    }

    if (frameDebugData.craftingMenuActive && inventoryMode && playerMobTextureHandle_ != UINT16_MAX
        && !hasContainerBackground)
    {
        float previewTop = layout.equipmentOriginY - layout.slotSize * 1.25f;
        const float minPreviewTop = layout.panelTop + layout.slotSize * 0.35f;
        if (previewTop < minPreviewTop)
        {
            previewTop = minPreviewTop;
        }
        const float previewCenterX = layout.equipmentOriginX + layout.slotSize * 0.85f;
        drawInventoryPlayerPreview(previewCenterX, previewTop, layout.slotSize);
    }

    if (furnaceMode)
    {
        const float progressBarX = layout.craftingOriginX + layout.slotSize + layout.slotGap * 0.9f;
        const float progressBarY = layout.resultSlotY + layout.slotSize * 0.42f;
        const float progressBarWidth = std::max(10.0f, layout.resultSlotX - progressBarX - layout.slotGap * 0.9f);
        const float progressBarHeight = std::max(6.0f, std::round(layout.slotSize * 0.16f));
        const float fuelBarX = layout.craftingOriginX + layout.slotSize * 0.38f;
        const float fuelBarY = layout.craftingOriginY + layout.slotSize + layout.slotGap * 0.18f;
        const float fuelBarWidth = std::max(6.0f, std::round(layout.slotSize * 0.24f));
        const float fuelBarHeight = std::max(10.0f, layout.furnaceFuelSlotY - fuelBarY - layout.slotGap * 0.18f);
        const std::uint32_t progressBackAbgr = detail::packAbgr8(glm::vec3(0.14f, 0.16f, 0.20f), 0.98f);
        const std::uint32_t progressFillAbgr = detail::packAbgr8(glm::vec3(0.86f, 0.86f, 0.84f), 0.98f);
        const std::uint32_t fuelFillAbgr = detail::packAbgr8(glm::vec3(0.95f, 0.58f, 0.17f), 0.98f);
        drawUiSolidRect(progressBarX, progressBarY, progressBarX + progressBarWidth, progressBarY + progressBarHeight, progressBackAbgr);
        drawUiSolidRect(
            progressBarX + 1.0f,
            progressBarY + 1.0f,
            progressBarX + 1.0f + std::max(0.0f, (progressBarWidth - 2.0f) * std::clamp(frameDebugData.craftingProgressFraction, 0.0f, 1.0f)),
            progressBarY + progressBarHeight - 1.0f,
            progressFillAbgr);
        drawUiSolidRect(fuelBarX, fuelBarY, fuelBarX + fuelBarWidth, fuelBarY + fuelBarHeight, progressBackAbgr);
        const float clampedFuel = std::clamp(frameDebugData.craftingFuelFraction, 0.0f, 1.0f);
        const float fuelFillTop = fuelBarY + 1.0f + (fuelBarHeight - 2.0f) * (1.0f - clampedFuel);
        drawUiSolidRect(
            fuelBarX + 1.0f,
            fuelFillTop,
            fuelBarX + fuelBarWidth - 1.0f,
            fuelBarY + fuelBarHeight - 1.0f,
            fuelFillAbgr);
        drawSlotFrame(layout.resultSlotX, layout.resultSlotY, frameDebugData.craftingResultSlot.count > 0);
        drawSlotContents(frameDebugData.craftingResultSlot, layout.resultSlotX, layout.resultSlotY);
    }
    else if (!chestMode)
    {
        const float arrowY = layout.resultSlotY + layout.slotSize * 0.5f;
        const float arrowX0 = layout.craftingOriginX
            + static_cast<float>(craftingColumns) * layout.slotSize
            + static_cast<float>(craftingColumns - 1) * layout.slotGap
            + layout.slotGap * 0.9f;
        const float arrowX1 = layout.resultSlotX - layout.slotGap * 0.9f;
        const float arrowMidX = arrowX1 - layout.slotSize * 0.18f;
        drawUiSolidRect(arrowX0, arrowY - 2.0f, arrowMidX, arrowY + 2.0f, cursorOutlineAbgr);
        drawUiSolidRect(arrowMidX, arrowY - layout.slotSize * 0.16f, arrowX1, arrowY, cursorOutlineAbgr);
        drawUiSolidRect(arrowMidX, arrowY, arrowX1, arrowY + layout.slotSize * 0.16f, cursorOutlineAbgr);

        drawSlotFrame(layout.resultSlotX, layout.resultSlotY, frameDebugData.craftingResultSlot.count > 0);
        drawSlotContents(frameDebugData.craftingResultSlot, layout.resultSlotX, layout.resultSlotY);
    }

    constexpr int kVisibleBagRows = 3;
    constexpr std::size_t kBagColumns = 9;
    const std::size_t maxBagStartRow =
        (FrameDebugData::kBagHudSlotCount / kBagColumns) > kVisibleBagRows
        ? (FrameDebugData::kBagHudSlotCount / kBagColumns) - kVisibleBagRows
        : 0;
    const std::size_t bagStartRow = std::min(static_cast<std::size_t>(frameDebugData.craftingBagStartRow), maxBagStartRow);
    for (int row = 0; row < kVisibleBagRows; ++row)
    {
        for (int col = 0; col < 9; ++col)
        {
            const std::size_t slotIndex =
                (bagStartRow + static_cast<std::size_t>(row)) * kBagColumns
                + static_cast<std::size_t>(col);
            const float slotX = layout.inventoryOriginX + static_cast<float>(col) * (layout.slotSize + layout.slotGap);
            const float slotY = layout.inventoryOriginY + static_cast<float>(row) * (layout.slotSize + layout.slotGap);
            drawSlotFrame(slotX, slotY, false);
            drawSlotContents(frameDebugData.bagSlots[slotIndex], slotX, slotY);
        }
    }

    for (int slotIndex = 0; slotIndex < 9; ++slotIndex)
    {
        const float slotX = layout.inventoryOriginX + static_cast<float>(slotIndex) * (layout.slotSize + layout.slotGap);
        const float slotY = layout.hotbarOriginY;
        const bool selected = static_cast<std::size_t>(slotIndex) == frameDebugData.hotbarSelectedIndex;
        drawSlotFrame(slotX, slotY, selected);
        drawSlotContents(frameDebugData.hotbarSlots[static_cast<std::size_t>(slotIndex)], slotX, slotY);
    }

    if (frameDebugData.craftingCursorSlot.count > 0)
    {
        const float cursorX = frameDebugData.uiCursorX - layout.slotSize * 0.34f;
        const float cursorY = frameDebugData.uiCursorY - layout.slotSize * 0.34f;
        drawSlotContents(frameDebugData.craftingCursorSlot, cursorX, cursorY);
    }
}
}  // namespace vibecraft::render
