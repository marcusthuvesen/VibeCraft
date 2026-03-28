#include "vibecraft/render/Renderer.hpp"

#include <SDL3/SDL_filesystem.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <utility>
#include <vector>

#include "debugdraw.h"

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::render
{
namespace
{
constexpr bgfx::ViewId kMainView = 0;
constexpr std::uint32_t kDefaultResetFlags = BGFX_RESET_VSYNC;
constexpr std::uint64_t kChunkRenderState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;
constexpr std::uint16_t kChunkAtlasWidth = vibecraft::kChunkAtlasWidthPx;
constexpr std::uint16_t kChunkAtlasHeight = vibecraft::kChunkAtlasHeightPx;

/// Main menu debug-text layout (rows/cols). Must stay in sync with `hitTestMainMenu`.
namespace MainMenuLayout
{
constexpr int kStackOuterWidth = 38;
constexpr int kBottomBlockHalfWidth = 15;

struct Geometry
{
    int centerCol = 0;
    int subtitleRow = 0;
    int ruleRow = 0;
    int stack0Row = 0;
    int stack1Row = 0;
    int stack2Row = 0;
    int bottomPairRow = 0;
    int iconRow = 0;
    int splashRow = 0;
};

[[nodiscard]] Geometry computeGeometry(const int textWidth, const int textHeight)
{
    Geometry geometry;
    geometry.centerCol = std::max(0, (textWidth - kStackOuterWidth) / 2);

    const int minTopRow = 14;
    const int maxTopRow = std::max(minTopRow, textHeight - 14);
    geometry.stack0Row = std::clamp(textHeight / 2 + 1, minTopRow, maxTopRow);
    geometry.stack1Row = geometry.stack0Row + 4;
    geometry.stack2Row = geometry.stack0Row + 8;
    geometry.bottomPairRow = geometry.stack0Row + 11;
    geometry.iconRow = geometry.bottomPairRow + 1;
    geometry.ruleRow = std::max(8, geometry.stack0Row - 4);
    geometry.subtitleRow = std::max(6, geometry.ruleRow - 2);
    geometry.splashRow = std::clamp(
        std::max(textHeight - 5, geometry.iconRow + 2),
        0,
        std::max(0, textHeight - 3));
    return geometry;
}
}  // namespace MainMenuLayout

struct ChunkVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    std::uint32_t abgr = 0xffffffff;

    static bgfx::VertexLayout layout()
    {
        bgfx::VertexLayout vertexLayout;
        vertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
        return vertexLayout;
    }
};

[[nodiscard]] bgfx::VertexBufferHandle toVertexBufferHandle(const std::uint16_t handleIndex)
{
    return bgfx::VertexBufferHandle{handleIndex};
}

[[nodiscard]] bgfx::IndexBufferHandle toIndexBufferHandle(const std::uint16_t handleIndex)
{
    return bgfx::IndexBufferHandle{handleIndex};
}

[[nodiscard]] bgfx::ProgramHandle toProgramHandle(const std::uint16_t handleIndex)
{
    return bgfx::ProgramHandle{handleIndex};
}

[[nodiscard]] bgfx::TextureHandle toTextureHandle(const std::uint16_t handleIndex)
{
    return bgfx::TextureHandle{handleIndex};
}

[[nodiscard]] bgfx::UniformHandle toUniformHandle(const std::uint16_t handleIndex)
{
    return bgfx::UniformHandle{handleIndex};
}

[[nodiscard]] std::uint32_t packRgba8(const glm::vec3& color, const float alpha = 1.0f)
{
    const auto toByte = [](const float channel)
    {
        return static_cast<std::uint32_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
    };

    const std::uint32_t r = toByte(color.r);
    const std::uint32_t g = toByte(color.g);
    const std::uint32_t b = toByte(color.b);
    const std::uint32_t a = toByte(alpha);
    return (r << 24U) | (g << 16U) | (b << 8U) | a;
}

[[nodiscard]] std::uint32_t packAbgr8(const glm::vec3& color, const float alpha = 1.0f)
{
    const auto toByte = [](const float channel)
    {
        return static_cast<std::uint32_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
    };

    const std::uint32_t r = toByte(color.r);
    const std::uint32_t g = toByte(color.g);
    const std::uint32_t b = toByte(color.b);
    const std::uint32_t a = toByte(alpha);
    return (a << 24U) | (b << 16U) | (g << 8U) | r;
}

void setVec4Uniform(
    const std::uint16_t handleIndex,
    const glm::vec3& xyz,
    const float w)
{
    if (handleIndex == UINT16_MAX)
    {
        return;
    }

    const float values[4] = {
        xyz.x,
        xyz.y,
        xyz.z,
        w,
    };
    bgfx::setUniform(toUniformHandle(handleIndex), values);
}

[[nodiscard]] std::uint32_t hashUint32(
    const std::int32_t a,
    const std::int32_t b,
    const std::int32_t c)
{
    std::uint32_t value = static_cast<std::uint32_t>(a) * 0x9e3779b9U;
    value ^= static_cast<std::uint32_t>(b) * 0x85ebca6bU + 0x7f4a7c15U;
    value ^= static_cast<std::uint32_t>(c) * 0xc2b2ae35U + 0x165667b1U;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] float hashUnitFloat(
    const std::int32_t a,
    const std::int32_t b,
    const std::int32_t c)
{
    return static_cast<float>(hashUint32(a, b, c) & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

[[nodiscard]] glm::vec2 normalizeOrFallback(const glm::vec2& vector, const glm::vec2& fallback)
{
    const float lengthSquared = glm::dot(vector, vector);
    return lengthSquared > 0.0f ? glm::normalize(vector) : fallback;
}

void drawWeatherClouds(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.cloudCoverage <= 0.02f)
    {
        return;
    }

    const glm::vec2 windDirection =
        normalizeOrFallback(cameraFrameData.weatherWindDirectionXZ, glm::vec2(1.0f, 0.0f));
    const glm::vec2 windOffset = windDirection * cameraFrameData.weatherTimeSeconds * cameraFrameData.weatherWindSpeed;
    constexpr float kCloudCellSize = 34.0f;
    constexpr int kCloudRadiusInCells = 4;
    const float cloudHeight = glm::max(72.0f, cameraFrameData.position.y + 40.0f);
    const int baseCellX = static_cast<int>(std::floor((cameraFrameData.position.x + windOffset.x) / kCloudCellSize));
    const int baseCellZ = static_cast<int>(std::floor((cameraFrameData.position.z + windOffset.y) / kCloudCellSize));

    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(true);

    for (int cellZ = -kCloudRadiusInCells; cellZ <= kCloudRadiusInCells; ++cellZ)
    {
        for (int cellX = -kCloudRadiusInCells; cellX <= kCloudRadiusInCells; ++cellX)
        {
            const int gridX = baseCellX + cellX;
            const int gridZ = baseCellZ + cellZ;
            const float density = hashUnitFloat(gridX, gridZ, 11);
            if (density > cameraFrameData.cloudCoverage)
            {
                continue;
            }

            const float centerX =
                static_cast<float>(gridX) * kCloudCellSize
                - windOffset.x
                + (hashUnitFloat(gridX, gridZ, 21) - 0.5f) * 14.0f;
            const float centerZ =
                static_cast<float>(gridZ) * kCloudCellSize
                - windOffset.y
                + (hashUnitFloat(gridX, gridZ, 31) - 0.5f) * 14.0f;
            const float y = cloudHeight + (hashUnitFloat(gridX, gridZ, 41) - 0.5f) * 4.0f;
            const float baseSize = 20.0f + hashUnitFloat(gridX, gridZ, 51) * 20.0f;
            const float stretch = 0.8f + hashUnitFloat(gridX, gridZ, 61) * 0.7f;
            const float secondaryOffset = 6.0f + hashUnitFloat(gridX, gridZ, 71) * 8.0f;
            const glm::vec3 primaryTint =
                glm::mix(cameraFrameData.cloudTint, cameraFrameData.horizonTint, 0.15f + density * 0.15f);
            const glm::vec3 secondaryTint = glm::mix(primaryTint, cameraFrameData.skyTint, 0.18f);

            debugDrawEncoder.setColor(packAbgr8(primaryTint, 1.0f));
            debugDrawEncoder.drawQuad(
                bx::Vec3(0.0f, 1.0f, 0.0f),
                bx::Vec3(centerX, y, centerZ),
                baseSize * stretch);

            debugDrawEncoder.setColor(packAbgr8(secondaryTint, 1.0f));
            debugDrawEncoder.drawQuad(
                bx::Vec3(0.0f, 1.0f, 0.0f),
                bx::Vec3(centerX + secondaryOffset, y - 1.0f, centerZ - secondaryOffset * 0.5f),
                baseSize * 0.75f);
        }
    }

    debugDrawEncoder.pop();
}

void drawWeatherRain(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData)
{
    if (cameraFrameData.rainIntensity <= 0.02f)
    {
        return;
    }

    const glm::vec2 windDirection =
        normalizeOrFallback(cameraFrameData.weatherWindDirectionXZ, glm::vec2(1.0f, 0.0f));
    constexpr float kRainGridSpacing = 3.5f;
    constexpr int kRainRadiusInCells = 6;
    const float rainFallDistance = 5.5f + cameraFrameData.rainIntensity * 3.5f;
    const float rainSpeed = 14.0f + cameraFrameData.rainIntensity * 10.0f;
    const glm::vec3 rainVector(
        windDirection.x * 0.35f,
        -1.0f,
        windDirection.y * 0.35f);
    const glm::vec3 normalizedRainVector = glm::normalize(rainVector);
    const int baseCellX = static_cast<int>(std::floor(cameraFrameData.position.x / kRainGridSpacing));
    const int baseCellZ = static_cast<int>(std::floor(cameraFrameData.position.z / kRainGridSpacing));
    const glm::vec3 rainTint = glm::mix(cameraFrameData.cloudTint, glm::vec3(0.70f, 0.82f, 1.0f), 0.65f);

    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(true);
    debugDrawEncoder.setColor(packAbgr8(rainTint, 1.0f));

    for (int cellZ = -kRainRadiusInCells; cellZ <= kRainRadiusInCells; ++cellZ)
    {
        for (int cellX = -kRainRadiusInCells; cellX <= kRainRadiusInCells; ++cellX)
        {
            const int gridX = baseCellX + cellX;
            const int gridZ = baseCellZ + cellZ;
            if (hashUnitFloat(gridX, gridZ, 101) > cameraFrameData.rainIntensity * 0.92f)
            {
                continue;
            }

            const float offsetX = (hashUnitFloat(gridX, gridZ, 111) - 0.5f) * 1.6f;
            const float offsetZ = (hashUnitFloat(gridX, gridZ, 121) - 0.5f) * 1.6f;
            const float phase = hashUnitFloat(gridX, gridZ, 131);
            const float dropCycle =
                std::fmod(cameraFrameData.weatherTimeSeconds * rainSpeed + phase * rainFallDistance, rainFallDistance);
            const float startY = cameraFrameData.position.y + 14.0f - dropCycle;
            const glm::vec3 start(
                static_cast<float>(gridX) * kRainGridSpacing + offsetX,
                startY,
                static_cast<float>(gridZ) * kRainGridSpacing + offsetZ);
            const glm::vec3 end = start + normalizedRainVector * rainFallDistance;

            debugDrawEncoder.moveTo(start.x, start.y, start.z);
            debugDrawEncoder.lineTo(end.x, end.y, end.z);
        }
    }

    debugDrawEncoder.pop();
}

[[nodiscard]] std::filesystem::path runtimeAssetPath(const std::filesystem::path& relativePath)
{
    const char* const basePathCStr = SDL_GetBasePath();
    const std::filesystem::path basePath = basePathCStr != nullptr ? basePathCStr : "";
    return basePath / relativePath;
}

[[nodiscard]] bool isAabbInsideFrustum(const bx::Plane* const frustumPlanes, const bx::Aabb& aabb)
{
    for (int planeIndex = 0; planeIndex < 6; ++planeIndex)
    {
        const bx::Plane& plane = frustumPlanes[planeIndex];
        const bx::Vec3 positiveVertex(
            plane.normal.x >= 0.0f ? aabb.max.x : aabb.min.x,
            plane.normal.y >= 0.0f ? aabb.max.y : aabb.min.y,
            plane.normal.z >= 0.0f ? aabb.max.z : aabb.min.z);

        if (bx::distance(plane, positiveVertex) < 0.0f)
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] char hotbarGlyphForBlock(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
        return 'G';
    case BlockType::Dirt:
        return 'D';
    case BlockType::Stone:
        return 'S';
    case BlockType::Deepslate:
        return 'd';
    case BlockType::CoalOre:
        return 'C';
    case BlockType::Sand:
        return 's';
    case BlockType::Bedrock:
        return 'B';
    case BlockType::Water:
        return 'W';
    case BlockType::IronOre:
        return 'I';
    case BlockType::GoldOre:
        return 'g';
    case BlockType::DiamondOre:
        return '*';
    case BlockType::EmeraldOre:
        return 'E';
    case BlockType::Lava:
        return 'L';
    case BlockType::Air:
    default:
        return '?';
    }
}

[[nodiscard]] std::string formatHotbarCell(const FrameDebugData::HotbarSlotHud& slot)
{
    if (slot.count == 0)
    {
        return std::string("[   ]");
    }

    const char glyph = hotbarGlyphForBlock(slot.blockType);
    const std::uint32_t displayCount = std::min(slot.count, 99u);
    return fmt::format("[{}{:02}]", glyph, displayCount);
}

/// Wider padded cells so the bag reads larger than the hotbar (same glyph + count).
[[nodiscard]] std::string formatBagSlotCell(const FrameDebugData::HotbarSlotHud& slot)
{
    if (slot.count == 0)
    {
        return std::string(" [---] ");
    }

    const char glyph = hotbarGlyphForBlock(slot.blockType);
    const std::uint32_t displayCount = std::min(slot.count, 99u);
    return fmt::format(" [{}{:02}] ", glyph, displayCount);
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

[[nodiscard]] int computeHotbarGridWidthChars()
{
    constexpr int kCellChars = 5;
    constexpr int kGap = 1;
    constexpr int kSlotCount = 9;
    return kSlotCount * kCellChars + (kSlotCount - 1) * kGap;
}

[[nodiscard]] int computeBagGridWidthChars()
{
    constexpr int kCellChars = 7;
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
            glyph = "<3";
            attr = 0x0c;
        }
        else if (heartHealth >= 1.0f)
        {
            glyph = "</";
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

void drawHotbarKeyHintsRow(const std::uint16_t row)
{
    constexpr int kCellChars = 5;
    constexpr int kGap = 1;
    int col = computeCenteredHotbarStartColumn();
    for (int keyIndex = 1; keyIndex <= 9; ++keyIndex)
    {
        bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col + 2), row, 0x06, "%d", keyIndex);
        col += kCellChars + kGap;
    }
}

void drawBagRow(
    const std::uint16_t row,
    const std::size_t slotOffset,
    const FrameDebugData& frameDebugData)
{
    constexpr int kCellChars = 7;
    constexpr int kGap = 1;
    int col = computeCenteredColumnStart(computeBagGridWidthChars());
    for (int i = 0; i < 9; ++i)
    {
        const FrameDebugData::HotbarSlotHud& slot = frameDebugData.bagSlots[slotOffset + static_cast<std::size_t>(i)];
        const bool empty = slot.count == 0;
        const std::uint16_t attr = empty ? 0x08 : 0x0b;
        const std::string cell = formatBagSlotCell(slot);
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

void drawFramedButton3(
    const int row,
    const int col,
    const int outerWidth,
    const std::string& label,
    const bool hovered)
{
    const std::uint16_t borderAttr = hovered ? 0x1f : 0x08;
    const std::uint16_t midAttr = hovered ? 0x1f : 0x0b;
    const int inner = outerWidth - 2;
    const std::string border = "+" + std::string(static_cast<std::size_t>(inner), '-') + "+";
    const std::string mid = "|" + padLabelToInnerWidth(label, inner) + "|";
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row), borderAttr, "%s", border.c_str());
    bgfx::dbgTextPrintf(static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 1), midAttr, "%s", mid.c_str());
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(col), static_cast<std::uint16_t>(row + 2), borderAttr, "%s", border.c_str());
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

void drawMainMenuOverlay(const FrameDebugData& frameDebugData, const std::uint16_t textWidth, const std::uint16_t textHeight)
{
    using namespace MainMenuLayout;
    const int tw = static_cast<int>(textWidth);
    const int th = static_cast<int>(textHeight);
    const Geometry geometry = computeGeometry(tw, th);

    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(geometry.subtitleRow), 0x0e, "DESKTOP EDITION");

    const int ruleWidth = std::clamp(tw - 8, 24, 48);
    const std::string ruleLine(static_cast<std::size_t>(ruleWidth), '-');
    dbgTextPrintfCenteredRow(static_cast<std::uint16_t>(geometry.ruleRow), 0x08, ruleLine);

    const int hovered = frameDebugData.mainMenuHoveredButton;
    drawFramedButton3(geometry.stack0Row, geometry.centerCol, kStackOuterWidth, "Singleplayer", hovered == 0);
    drawFramedButton3(geometry.stack1Row, geometry.centerCol, kStackOuterWidth, "Multiplayer", hovered == 1);
    drawFramedButton3(geometry.stack2Row, geometry.centerCol, kStackOuterWidth, "VibeCraft Realms  * !", hovered == 2);

    drawBottomButtonPair(
        geometry.bottomPairRow,
        geometry.centerCol,
        "Options...",
        "Quit game",
        hovered == 3,
        hovered == 4);

    const std::uint16_t iconAttrG = hovered == 5 ? 0x1f : 0x0b;
    const std::uint16_t iconAttrA = hovered == 6 ? 0x1f : 0x0b;
    if (geometry.centerCol >= 7)
    {
        bgfx::dbgTextPrintf(
            static_cast<std::uint16_t>(geometry.centerCol - 6),
            static_cast<std::uint16_t>(geometry.iconRow),
            iconAttrG,
            "[G]");
    }
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(geometry.centerCol + 36),
        static_cast<std::uint16_t>(geometry.iconRow),
        iconAttrA,
        "[A]");

    const bool splashBright =
        static_cast<int>(frameDebugData.mainMenuTimeSeconds * 3.0f) % 2 == 0;
    const std::uint16_t splashAttr = splashBright ? 0x0e : 0x06;
    const std::string splash = "Also try building!";
    const int splashCol = std::max(0, tw - static_cast<int>(splash.size()) - 2);
    bgfx::dbgTextPrintf(
        static_cast<std::uint16_t>(splashCol),
        static_cast<std::uint16_t>(geometry.splashRow),
        splashAttr,
        "%s",
        splash.c_str());

    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    if (!frameDebugData.mainMenuNotice.empty() && footerRow >= 3)
    {
        const std::uint16_t noticeRow =
            footerRow >= 26 ? static_cast<std::uint16_t>(footerRow - 2) : static_cast<std::uint16_t>(0);
        dbgTextPrintfCenteredRow(noticeRow, 0x0b, frameDebugData.mainMenuNotice);
    }

    dbgTextPrintfCenteredRow(footerRow, 0x06, "Tab: capture mouse   Esc: pause menu");
}

void drawPauseMenuOverlay(
    const FrameDebugData& frameDebugData,
    const std::uint16_t textWidth,
    const std::uint16_t textHeight)
{
    constexpr int kWide = 32;
    const int centerCol = std::max(0, (static_cast<int>(textWidth) - kWide) / 2);
    dbgTextPrintfCenteredRow(5, 0x0f, "GAME MENU");

    const int hovered = frameDebugData.pauseMenuHoveredButton;
    drawFramedButton3(9, centerCol, kWide, "Back to game", hovered == 0);
    drawFramedButton3(13, centerCol, kWide, "Options...", hovered == 1);
    drawFramedButton3(17, centerCol, kWide, "Quit to title", hovered == 2);
    drawFramedButton3(21, centerCol, kWide, "Quit game", hovered == 3);

    const std::uint16_t footerRow =
        textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    if (!frameDebugData.pauseMenuNotice.empty() && footerRow >= 3)
    {
        const std::uint16_t noticeRow =
            footerRow >= 26 ? static_cast<std::uint16_t>(footerRow - 2) : static_cast<std::uint16_t>(0);
        dbgTextPrintfCenteredRow(noticeRow, 0x0b, frameDebugData.pauseMenuNotice);
    }
    dbgTextPrintfCenteredRow(footerRow, 0x06, "Esc: back to game   Click: choose");
}

[[nodiscard]] const char* shaderProfileDirectory(const bgfx::RendererType::Enum rendererType)
{
    switch (rendererType)
    {
    case bgfx::RendererType::Direct3D11:
        return "dxbc";
    case bgfx::RendererType::Direct3D12:
        return "dxil";
    case bgfx::RendererType::Metal:
        return "metal";
    case bgfx::RendererType::OpenGL:
        return "glsl";
    case bgfx::RendererType::OpenGLES:
        return "essl";
    case bgfx::RendererType::Vulkan:
        return "spirv";
    case bgfx::RendererType::WebGPU:
        return "wgsl";
    case bgfx::RendererType::Noop:
    case bgfx::RendererType::Count:
    default:
        return nullptr;
    }
}

[[nodiscard]] bgfx::ShaderHandle loadShader(const std::string& shaderName)
{
    const char* const profileDirectory = shaderProfileDirectory(bgfx::getRendererType());
    if (profileDirectory == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }

    const std::filesystem::path shaderPath =
        runtimeAssetPath(std::filesystem::path("shaders") / profileDirectory / (shaderName + ".bin"));

    std::ifstream shaderStream(shaderPath, std::ios::binary);
    if (!shaderStream)
    {
        return BGFX_INVALID_HANDLE;
    }

    const std::vector<char> shaderBytes(
        (std::istreambuf_iterator<char>(shaderStream)),
        std::istreambuf_iterator<char>());

    if (shaderBytes.empty())
    {
        return BGFX_INVALID_HANDLE;
    }

    const bgfx::Memory* const shaderMemory =
        bgfx::copy(shaderBytes.data(), static_cast<std::uint32_t>(shaderBytes.size()));
    return bgfx::createShader(shaderMemory);
}

[[nodiscard]] bgfx::ProgramHandle loadProgram(
    const std::string& vertexShaderName,
    const std::string& fragmentShaderName)
{
    const bgfx::ShaderHandle vertexShader = loadShader(vertexShaderName);
    const bgfx::ShaderHandle fragmentShader = loadShader(fragmentShaderName);

    if (!bgfx::isValid(vertexShader) || !bgfx::isValid(fragmentShader))
    {
        if (bgfx::isValid(vertexShader))
        {
            bgfx::destroy(vertexShader);
        }
        if (bgfx::isValid(fragmentShader))
        {
            bgfx::destroy(fragmentShader);
        }
        return BGFX_INVALID_HANDLE;
    }

    return bgfx::createProgram(vertexShader, fragmentShader, true);
}

[[nodiscard]] bgfx::TextureHandle createFallbackChunkAtlasTexture()
{
    constexpr std::uint16_t kAtlasSize = 16;
    constexpr std::uint16_t kTileSize = 8;
    std::vector<std::uint8_t> atlasPixels(kAtlasSize * kAtlasSize * 4, 255);

    for (std::uint16_t y = 0; y < kAtlasSize; ++y)
    {
        for (std::uint16_t x = 0; x < kAtlasSize; ++x)
        {
            const std::size_t pixelOffset = static_cast<std::size_t>((y * kAtlasSize + x) * 4);
            const int tileX = x / kTileSize;
            const int tileY = y / kTileSize;
            const int tileIndex = tileY * 2 + tileX;
            const bool darkChecker = ((x + y) & 1U) == 0U;
            const float shade = darkChecker ? 0.86f : 1.0f;

            std::uint8_t r = 90;
            std::uint8_t g = 160;
            std::uint8_t b = 90;
            switch (tileIndex)
            {
            case 0:  // grass
                r = 92;
                g = 164;
                b = 86;
                break;
            case 1:  // dirt
                r = 124;
                g = 90;
                b = 60;
                break;
            case 2:  // stone
                r = 132;
                g = 132;
                b = 132;
                break;
            case 3:  // sand
                r = 188;
                g = 172;
                b = 120;
                break;
            default:
                break;
            }

            atlasPixels[pixelOffset + 0] = static_cast<std::uint8_t>(b * shade);  // BGRA8
            atlasPixels[pixelOffset + 1] = static_cast<std::uint8_t>(g * shade);
            atlasPixels[pixelOffset + 2] = static_cast<std::uint8_t>(r * shade);
            atlasPixels[pixelOffset + 3] = 255;
        }
    }

    const bgfx::Memory* const atlasMemory =
        bgfx::copy(atlasPixels.data(), static_cast<std::uint32_t>(atlasPixels.size()));
    return bgfx::createTexture2D(
        kAtlasSize,
        kAtlasSize,
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE,
        atlasMemory);
}

[[nodiscard]] bgfx::TextureHandle createChunkAtlasTexture()
{
    const std::filesystem::path atlasPath = runtimeAssetPath("textures/chunk_atlas.bgra");
    if (!std::filesystem::exists(atlasPath))
    {
        return createFallbackChunkAtlasTexture();
    }

    constexpr std::size_t kExpectedBytes =
        static_cast<std::size_t>(kChunkAtlasWidth) * static_cast<std::size_t>(kChunkAtlasHeight) * 4U;
    if (std::filesystem::file_size(atlasPath) != kExpectedBytes)
    {
        return createFallbackChunkAtlasTexture();
    }

    std::ifstream atlasStream(atlasPath, std::ios::binary);
    if (!atlasStream)
    {
        return createFallbackChunkAtlasTexture();
    }

    std::vector<std::uint8_t> atlasPixels(kExpectedBytes);
    atlasStream.read(reinterpret_cast<char*>(atlasPixels.data()), static_cast<std::streamsize>(atlasPixels.size()));
    if (!atlasStream)
    {
        return createFallbackChunkAtlasTexture();
    }

    const bgfx::Memory* const atlasMemory =
        bgfx::copy(atlasPixels.data(), static_cast<std::uint32_t>(atlasPixels.size()));
    return bgfx::createTexture2D(
        kChunkAtlasWidth,
        kChunkAtlasHeight,
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        atlasMemory);
}

[[nodiscard]] bgfx::TextureHandle createLogoTextureFromPng(
    bx::AllocatorI& allocator,
    std::uint16_t& outWidth,
    std::uint16_t& outHeight)
{
    outWidth = 0;
    outHeight = 0;
    const std::filesystem::path path = runtimeAssetPath("textures/ui/vibecraft_logo.png");
    if (!std::filesystem::exists(path))
    {
        return BGFX_INVALID_HANDLE;
    }

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return BGFX_INVALID_HANDLE;
    }
    const std::streamsize fileSize = stream.tellg();
    if (fileSize <= 0)
    {
        return BGFX_INVALID_HANDLE;
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    stream.read(reinterpret_cast<char*>(bytes.data()), fileSize);
    if (!stream)
    {
        return BGFX_INVALID_HANDLE;
    }

    bimg::ImageContainer* image = bimg::imageParse(
        &allocator,
        bytes.data(),
        static_cast<std::uint32_t>(bytes.size()),
        bimg::TextureFormat::RGBA8);
    if (image == nullptr)
    {
        return BGFX_INVALID_HANDLE;
    }

    outWidth = static_cast<std::uint16_t>(image->m_width);
    outHeight = static_cast<std::uint16_t>(image->m_height);
    const bgfx::Memory* const memory = bgfx::copy(image->m_data, image->m_size);
    bimg::imageFree(image);

    return bgfx::createTexture2D(
        outWidth,
        outHeight,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
        memory);
}
}

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

    using namespace MainMenuLayout;
    const Geometry geometry = computeGeometry(tw, static_cast<int>(textHeight));

    for (int buttonIndex = 0; buttonIndex < 3; ++buttonIndex)
    {
        const int row0 = geometry.stack0Row + buttonIndex * 4;
        if (clampedRow >= row0 && clampedRow <= row0 + 2 && clampedCol >= geometry.centerCol
            && clampedCol <= geometry.centerCol + kStackOuterWidth - 1)
        {
            return buttonIndex;
        }
    }

    if (clampedRow >= geometry.bottomPairRow && clampedRow <= geometry.bottomPairRow + 2)
    {
        if (clampedCol >= geometry.centerCol && clampedCol <= geometry.centerCol + kBottomBlockHalfWidth - 1)
        {
            return 3;
        }
        if (clampedCol >= geometry.centerCol + kBottomBlockHalfWidth + 1
            && clampedCol <= geometry.centerCol + kBottomBlockHalfWidth + 1 + kBottomBlockHalfWidth - 1)
        {
            return 4;
        }
    }

    if (clampedRow == geometry.iconRow && geometry.centerCol >= 7)
    {
        if (clampedCol >= geometry.centerCol - 6 && clampedCol <= geometry.centerCol - 4)
        {
            return 5;
        }
    }

    if (clampedRow == geometry.iconRow)
    {
        if (clampedCol >= geometry.centerCol + 36 && clampedCol <= geometry.centerCol + 38)
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

    for (int buttonIndex = 0; buttonIndex < 4; ++buttonIndex)
    {
        const int row0 = 9 + buttonIndex * 4;
        if (clampedRow >= row0 && clampedRow <= row0 + 2 && clampedCol >= centerCol && clampedCol <= centerCol + kWide - 1)
        {
            return buttonIndex;
        }
    }

    return -1;
}

bool Renderer::initialize(void* const nativeWindowHandle, const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;

    bgfx::PlatformData platformData{};
    platformData.nwh = nativeWindowHandle;
    bgfx::setPlatformData(platformData);

    bgfx::Init init;
    init.type = bgfx::RendererType::Count;
    init.platformData = platformData;
    init.resolution.width = width_;
    init.resolution.height = height_;
    init.resolution.reset = kDefaultResetFlags;

    if (!bgfx::init(init))
    {
        return false;
    }

    initialized_ = true;
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x263238ff, 1.0f, 0);
    ddInit();

    const bgfx::ProgramHandle chunkProgram = loadProgram("vs_chunk", "fs_chunk");
    if (!bgfx::isValid(chunkProgram))
    {
        ddShutdown();
        bgfx::shutdown();
        initialized_ = false;
        return false;
    }

    const bgfx::TextureHandle chunkAtlasTexture = createChunkAtlasTexture();
    if (!bgfx::isValid(chunkAtlasTexture))
    {
        bgfx::destroy(chunkProgram);
        ddShutdown();
        bgfx::shutdown();
        initialized_ = false;
        return false;
    }

    const bgfx::UniformHandle chunkAtlasSampler = bgfx::createUniform("s_chunkAtlas", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(chunkAtlasSampler))
    {
        bgfx::destroy(chunkAtlasTexture);
        bgfx::destroy(chunkProgram);
        ddShutdown();
        bgfx::shutdown();
        initialized_ = false;
        return false;
    }

    const bgfx::UniformHandle chunkSunDirection = bgfx::createUniform("u_sunDirection", bgfx::UniformType::Vec4);
    const bgfx::UniformHandle chunkSunLightColor = bgfx::createUniform("u_sunLightColor", bgfx::UniformType::Vec4);
    const bgfx::UniformHandle chunkMoonDirection = bgfx::createUniform("u_moonDirection", bgfx::UniformType::Vec4);
    const bgfx::UniformHandle chunkMoonLightColor =
        bgfx::createUniform("u_moonLightColor", bgfx::UniformType::Vec4);
    const bgfx::UniformHandle chunkAmbientLight = bgfx::createUniform("u_ambientLight", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(chunkSunDirection)
        || !bgfx::isValid(chunkSunLightColor)
        || !bgfx::isValid(chunkMoonDirection)
        || !bgfx::isValid(chunkMoonLightColor)
        || !bgfx::isValid(chunkAmbientLight))
    {
        if (bgfx::isValid(chunkSunDirection))
        {
            bgfx::destroy(chunkSunDirection);
        }
        if (bgfx::isValid(chunkSunLightColor))
        {
            bgfx::destroy(chunkSunLightColor);
        }
        if (bgfx::isValid(chunkMoonDirection))
        {
            bgfx::destroy(chunkMoonDirection);
        }
        if (bgfx::isValid(chunkMoonLightColor))
        {
            bgfx::destroy(chunkMoonLightColor);
        }
        if (bgfx::isValid(chunkAmbientLight))
        {
            bgfx::destroy(chunkAmbientLight);
        }
        bgfx::destroy(chunkAtlasSampler);
        bgfx::destroy(chunkAtlasTexture);
        bgfx::destroy(chunkProgram);
        ddShutdown();
        bgfx::shutdown();
        initialized_ = false;
        return false;
    }

    chunkProgramHandle_ = chunkProgram.idx;
    chunkAtlasTextureHandle_ = chunkAtlasTexture.idx;
    chunkAtlasSamplerHandle_ = chunkAtlasSampler.idx;
    chunkSunDirectionUniformHandle_ = chunkSunDirection.idx;
    chunkSunLightColorUniformHandle_ = chunkSunLightColor.idx;
    chunkMoonDirectionUniformHandle_ = chunkMoonDirection.idx;
    chunkMoonLightColorUniformHandle_ = chunkMoonLightColor.idx;
    chunkAmbientLightUniformHandle_ = chunkAmbientLight.idx;

    bx::DefaultAllocator logoAllocator;
    const bgfx::TextureHandle logoTexture =
        createLogoTextureFromPng(logoAllocator, logoWidthPx_, logoHeightPx_);
    if (bgfx::isValid(logoTexture))
    {
        logoTextureHandle_ = logoTexture.idx;
        const bgfx::UniformHandle logoSampler = bgfx::createUniform("s_logo", bgfx::UniformType::Sampler);
        if (!bgfx::isValid(logoSampler))
        {
            bgfx::destroy(logoTexture);
            logoTextureHandle_ = UINT16_MAX;
            logoWidthPx_ = 0;
            logoHeightPx_ = 0;
        }
        else
        {
            logoSamplerHandle_ = logoSampler.idx;
            const bgfx::ProgramHandle logoProgram = loadProgram("vs_chunk", "fs_logo");
            if (!bgfx::isValid(logoProgram))
            {
                bgfx::destroy(logoSampler);
                bgfx::destroy(logoTexture);
                logoSamplerHandle_ = UINT16_MAX;
                logoTextureHandle_ = UINT16_MAX;
                logoWidthPx_ = 0;
                logoHeightPx_ = 0;
            }
            else
            {
                logoProgramHandle_ = logoProgram.idx;
            }
        }
    }
    return true;
}

void Renderer::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    destroySceneMeshes();
    if (logoProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toProgramHandle(logoProgramHandle_));
        logoProgramHandle_ = UINT16_MAX;
    }
    if (logoTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toTextureHandle(logoTextureHandle_));
        logoTextureHandle_ = UINT16_MAX;
    }
    if (logoSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(logoSamplerHandle_));
        logoSamplerHandle_ = UINT16_MAX;
    }
    logoWidthPx_ = 0;
    logoHeightPx_ = 0;
    if (chunkProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toProgramHandle(chunkProgramHandle_));
        chunkProgramHandle_ = UINT16_MAX;
    }
    if (chunkAtlasTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toTextureHandle(chunkAtlasTextureHandle_));
        chunkAtlasTextureHandle_ = UINT16_MAX;
    }
    if (chunkAtlasSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkAtlasSamplerHandle_));
        chunkAtlasSamplerHandle_ = UINT16_MAX;
    }
    if (chunkSunDirectionUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkSunDirectionUniformHandle_));
        chunkSunDirectionUniformHandle_ = UINT16_MAX;
    }
    if (chunkSunLightColorUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkSunLightColorUniformHandle_));
        chunkSunLightColorUniformHandle_ = UINT16_MAX;
    }
    if (chunkMoonDirectionUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkMoonDirectionUniformHandle_));
        chunkMoonDirectionUniformHandle_ = UINT16_MAX;
    }
    if (chunkMoonLightColorUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkMoonLightColorUniformHandle_));
        chunkMoonLightColorUniformHandle_ = UINT16_MAX;
    }
    if (chunkAmbientLightUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toUniformHandle(chunkAmbientLightUniformHandle_));
        chunkAmbientLightUniformHandle_ = UINT16_MAX;
    }
    ddShutdown();
    bgfx::shutdown();
    initialized_ = false;
}

void Renderer::resize(const std::uint32_t width, const std::uint32_t height)
{
    if (!initialized_ || (width_ == width && height_ == height))
    {
        return;
    }

    width_ = width;
    height_ = height;
    bgfx::reset(width_, height_, kDefaultResetFlags);
}

void Renderer::updateSceneMeshes(
    const std::vector<SceneMeshData>& sceneMeshes,
    const std::vector<std::uint64_t>& removedMeshIds)
{
    if (!initialized_)
    {
        return;
    }

    for (const std::uint64_t removedMeshId : removedMeshIds)
    {
        destroySceneMesh(removedMeshId);
    }

    for (const SceneMeshData& sceneMesh : sceneMeshes)
    {
        if (sceneMesh.vertices.empty() || sceneMesh.indices.empty())
        {
            continue;
        }

        destroySceneMesh(sceneMesh.id);

        std::vector<ChunkVertex> chunkVertices;
        chunkVertices.reserve(sceneMesh.vertices.size());

        for (const SceneMeshData::Vertex& vertex : sceneMesh.vertices)
        {
            chunkVertices.push_back(ChunkVertex{
                .x = vertex.position.x,
                .y = vertex.position.y,
                .z = vertex.position.z,
                .nx = vertex.normal.x,
                .ny = vertex.normal.y,
                .nz = vertex.normal.z,
                .u = vertex.uv.x,
                .v = vertex.uv.y,
                .abgr = vertex.abgr,
            });
        }

        const bgfx::VertexBufferHandle vertexBuffer = bgfx::createVertexBuffer(
            bgfx::copy(chunkVertices.data(), static_cast<std::uint32_t>(chunkVertices.size() * sizeof(ChunkVertex))),
            ChunkVertex::layout());
        const bgfx::IndexBufferHandle indexBuffer = bgfx::createIndexBuffer(
            bgfx::copy(
                sceneMesh.indices.data(),
                static_cast<std::uint32_t>(sceneMesh.indices.size() * sizeof(std::uint32_t))),
            BGFX_BUFFER_INDEX32);

        sceneMeshes_[sceneMesh.id] = Renderer::SceneGpuMesh{
            .vertexBufferHandle = vertexBuffer.idx,
            .indexBufferHandle = indexBuffer.idx,
            .indexCount = static_cast<std::uint32_t>(sceneMesh.indices.size()),
            .boundsMin = sceneMesh.boundsMin,
            .boundsMax = sceneMesh.boundsMax,
        };
    }
}

void Renderer::renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData)
{
    if (!initialized_ || width_ == 0 || height_ == 0)
    {
        return;
    }

    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x263238ff, 1.0f, 0);

    const bx::Vec3 eye(cameraFrameData.position.x, cameraFrameData.position.y, cameraFrameData.position.z);
    const bx::Vec3 at(
        cameraFrameData.position.x + cameraFrameData.forward.x,
        cameraFrameData.position.y + cameraFrameData.forward.y,
        cameraFrameData.position.z + cameraFrameData.forward.z);
    const bx::Vec3 up(cameraFrameData.up.x, cameraFrameData.up.y, cameraFrameData.up.z);

    float view[16];
    float projection[16];
    bx::mtxLookAt(view, eye, at, up);
    bx::mtxProj(
        projection,
        cameraFrameData.verticalFovDegrees,
        static_cast<float>(width_) / static_cast<float>(height_),
        cameraFrameData.nearClip,
        cameraFrameData.farClip,
        bgfx::getCaps()->homogeneousDepth);

    const std::uint32_t clearColor = packRgba8(cameraFrameData.skyTint);
    bgfx::setViewClear(kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::setViewRect(kMainView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
    bgfx::setViewTransform(kMainView, view, projection);
    bgfx::touch(kMainView);

    const glm::vec3 ambientLight = glm::clamp(
        cameraFrameData.skyTint * 0.45f
            + cameraFrameData.horizonTint * 0.15f
            + cameraFrameData.sunLightTint * (0.08f * cameraFrameData.sunVisibility)
            + cameraFrameData.moonLightTint * (0.05f * cameraFrameData.moonVisibility),
        glm::vec3(0.04f),
        glm::vec3(0.72f));
    const glm::vec3 sunLightColor =
        cameraFrameData.sunLightTint * glm::max(cameraFrameData.sunVisibility, 0.0f);
    const glm::vec3 moonLightColor =
        cameraFrameData.moonLightTint * glm::max(cameraFrameData.moonVisibility * 0.35f, 0.0f);

    std::uint32_t visibleChunkCount = 0;
    if (chunkProgramHandle_ != UINT16_MAX)
    {
        const float identityTransform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        float viewProjection[16];
        bx::mtxMul(viewProjection, view, projection);
        bx::Plane frustumPlanes[6] = {
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
            bx::Plane(bx::InitNone),
        };
        bx::buildFrustumPlanes(frustumPlanes, viewProjection);

        for (const auto& [sceneMeshId, sceneMesh] : sceneMeshes_)
        {
            static_cast<void>(sceneMeshId);
            const bx::Aabb aabb{
                bx::Vec3(sceneMesh.boundsMin.x, sceneMesh.boundsMin.y, sceneMesh.boundsMin.z),
                bx::Vec3(sceneMesh.boundsMax.x, sceneMesh.boundsMax.y, sceneMesh.boundsMax.z),
            };

            if (!isAabbInsideFrustum(frustumPlanes, aabb))
            {
                continue;
            }

            ++visibleChunkCount;
            bgfx::setTransform(identityTransform);
            bgfx::setVertexBuffer(0, toVertexBufferHandle(sceneMesh.vertexBufferHandle));
            bgfx::setIndexBuffer(
                toIndexBufferHandle(sceneMesh.indexBufferHandle),
                0,
                sceneMesh.indexCount);
            if (chunkAtlasTextureHandle_ != UINT16_MAX && chunkAtlasSamplerHandle_ != UINT16_MAX)
            {
                bgfx::setTexture(
                    0,
                    toUniformHandle(chunkAtlasSamplerHandle_),
                    toTextureHandle(chunkAtlasTextureHandle_));
            }
            setVec4Uniform(chunkSunDirectionUniformHandle_, cameraFrameData.sunDirection, 0.0f);
            setVec4Uniform(chunkSunLightColorUniformHandle_, sunLightColor, 0.0f);
            setVec4Uniform(chunkMoonDirectionUniformHandle_, cameraFrameData.moonDirection, 0.0f);
            setVec4Uniform(chunkMoonLightColorUniformHandle_, moonLightColor, 0.0f);
            setVec4Uniform(chunkAmbientLightUniformHandle_, ambientLight, 0.0f);
            bgfx::setState(kChunkRenderState);
            bgfx::submit(kMainView, toProgramHandle(chunkProgramHandle_));
        }
    }

    DebugDrawEncoder debugDrawEncoder;
    debugDrawEncoder.begin(kMainView);
    drawWeatherClouds(debugDrawEncoder, cameraFrameData);
    debugDrawEncoder.push();
    debugDrawEncoder.setDepthTestLess(false);
    constexpr float kCelestialDistance = 240.0f;
    constexpr float kSunRadius = 20.0f;
    constexpr float kMoonRadius = 13.0f;
    if (cameraFrameData.sunVisibility > 0.01f)
    {
        const glm::vec3 sunPosition = cameraFrameData.position + cameraFrameData.sunDirection * kCelestialDistance;
        const bx::Vec3 sunCenter(sunPosition.x, sunPosition.y, sunPosition.z);
        const bx::Vec3 sunFacingNormal(
            cameraFrameData.position.x - sunPosition.x,
            cameraFrameData.position.y - sunPosition.y,
            cameraFrameData.position.z - sunPosition.z);
        bx::Sphere sunSphere;
        sunSphere.center = sunCenter;
        sunSphere.radius = kSunRadius;
        debugDrawEncoder.setColor(packAbgr8(glm::mix(cameraFrameData.horizonTint, cameraFrameData.sunLightTint, 0.65f), 1.0f));
        debugDrawEncoder.drawQuad(sunFacingNormal, sunCenter, kSunRadius * 3.6f);
        debugDrawEncoder.setColor(packAbgr8(cameraFrameData.sunLightTint, 1.0f));
        debugDrawEncoder.draw(sunSphere);
        debugDrawEncoder.setColor(packAbgr8(cameraFrameData.horizonTint, 1.0f));
        debugDrawEncoder.drawCircle(sunFacingNormal, sunCenter, kSunRadius * 1.45f, 2.5f);
    }
    if (cameraFrameData.moonVisibility > 0.01f)
    {
        const glm::vec3 moonPosition = cameraFrameData.position + cameraFrameData.moonDirection * kCelestialDistance;
        const bx::Vec3 moonCenter(moonPosition.x, moonPosition.y, moonPosition.z);
        const bx::Vec3 moonFacingNormal(
            cameraFrameData.position.x - moonPosition.x,
            cameraFrameData.position.y - moonPosition.y,
            cameraFrameData.position.z - moonPosition.z);
        bx::Sphere moonSphere;
        moonSphere.center = moonCenter;
        moonSphere.radius = kMoonRadius;
        debugDrawEncoder.setColor(packAbgr8(glm::mix(cameraFrameData.skyTint, cameraFrameData.moonLightTint, 0.55f), 1.0f));
        debugDrawEncoder.drawQuad(moonFacingNormal, moonCenter, kMoonRadius * 2.8f);
        debugDrawEncoder.setColor(packAbgr8(cameraFrameData.moonLightTint, 1.0f));
        debugDrawEncoder.draw(moonSphere);
    }
    debugDrawEncoder.pop();
    drawWeatherRain(debugDrawEncoder, cameraFrameData);
    if (!frameDebugData.mainMenuActive)
    {
        debugDrawEncoder.setColor(0xff455a64);
        debugDrawEncoder.drawGrid(Axis::Y, bx::Vec3(0.0f, 0.0f, 0.0f), 48, 1.0f);
        debugDrawEncoder.drawAxis(0.0f, 0.0f, 0.0f, 2.0f);
    }

    if (frameDebugData.hasTarget && !frameDebugData.mainMenuActive)
    {
        const bx::Aabb targetAabb{
            bx::Vec3(
                static_cast<float>(frameDebugData.targetBlock.x),
                static_cast<float>(frameDebugData.targetBlock.y),
                static_cast<float>(frameDebugData.targetBlock.z)),
            bx::Vec3(
                static_cast<float>(frameDebugData.targetBlock.x + 1),
                static_cast<float>(frameDebugData.targetBlock.y + 1),
                static_cast<float>(frameDebugData.targetBlock.z + 1)),
        };

        debugDrawEncoder.setWireframe(true);
        debugDrawEncoder.setColor(0xff26c6da);
        debugDrawEncoder.draw(targetAabb);
        debugDrawEncoder.setWireframe(false);
    }

    debugDrawEncoder.end();

    bgfx::dbgTextClear();
    const bgfx::Stats* const bgfxStats = bgfx::getStats();
    const std::uint16_t textHeight = bgfxStats != nullptr ? bgfxStats->textHeight : 30;
    const std::uint16_t textWidthForHud =
        bgfxStats != nullptr && bgfxStats->textWidth > 0 ? bgfxStats->textWidth : 100;

    if (frameDebugData.mainMenuActive)
    {
        drawMainMenuLogo();
        drawMainMenuOverlay(frameDebugData, textWidthForHud, textHeight);
        bgfx::frame();
        return;
    }

    if (frameDebugData.pauseMenuActive)
    {
        drawPauseMenuOverlay(frameDebugData, textWidthForHud, textHeight);
        const std::uint16_t pauseHealthRow = textHeight > 2 ? static_cast<std::uint16_t>(textHeight - 3) : 0;
        drawHealthHud(pauseHealthRow, frameDebugData);
        bgfx::frame();
        return;
    }

    const std::uint16_t hotbarRow = textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    const std::uint16_t hotbarKeyRow = textHeight > 1 ? static_cast<std::uint16_t>(textHeight - 2) : 0;
    const std::uint16_t healthRow = textHeight > 2 ? static_cast<std::uint16_t>(textHeight - 3) : 0;
    const std::uint16_t bagRow2 = textHeight > 3 ? static_cast<std::uint16_t>(textHeight - 4) : 0;
    const std::uint16_t bagRow1 = textHeight > 4 ? static_cast<std::uint16_t>(textHeight - 5) : 0;
    const std::uint16_t bagRow0 = textHeight > 5 ? static_cast<std::uint16_t>(textHeight - 6) : 0;
    const std::uint16_t bagSepRow = textHeight > 6 ? static_cast<std::uint16_t>(textHeight - 7) : 0;
    const std::uint16_t bagTitleRow = textHeight > 7 ? static_cast<std::uint16_t>(textHeight - 8) : 0;
    const std::uint16_t controlsRow1 = textHeight > 8 ? static_cast<std::uint16_t>(textHeight - 9) : 0;
    const std::uint16_t controlsRow0 = textHeight > 9 ? static_cast<std::uint16_t>(textHeight - 10) : 0;

    bgfx::dbgTextPrintf(0, 1, 0x0f, "VibeCraft foundation slice");
    bgfx::dbgTextPrintf(0, 3, 0x0a, "%s", frameDebugData.statusLine.c_str());
    bgfx::dbgTextPrintf(
        0,
        5,
        0x0f,
        "Chunks: %u  Dirty: %u  Resident: %u",
        frameDebugData.chunkCount,
        frameDebugData.dirtyChunkCount,
        frameDebugData.residentChunkCount);
    bgfx::dbgTextPrintf(0, 6, 0x0f, "Faces: %u  Visible chunks: %u", frameDebugData.totalFaces, visibleChunkCount);
    if (bgfxStats != nullptr)
    {
        const double cpuFrameMs = bgfxStats->cpuTimerFreq > 0
            ? static_cast<double>(bgfxStats->cpuTimeFrame) * 1000.0
                / static_cast<double>(bgfxStats->cpuTimerFreq)
            : 0.0;
        const double gpuFrameMs =
            (bgfxStats->gpuTimerFreq > 0 && bgfxStats->gpuTimeEnd >= bgfxStats->gpuTimeBegin)
            ? static_cast<double>(bgfxStats->gpuTimeEnd - bgfxStats->gpuTimeBegin) * 1000.0
                / static_cast<double>(bgfxStats->gpuTimerFreq)
            : 0.0;
        bgfx::dbgTextPrintf(
            0,
            7,
            0x0f,
            "bgfx: cpu %.2f ms  gpu %.2f ms  draw %u  tri %u",
            cpuFrameMs,
            gpuFrameMs,
            bgfxStats->numDraw,
            bgfxStats->numPrims[bgfx::Topology::TriList]);
    }
    else
    {
        bgfx::dbgTextPrintf(0, 7, 0x0f, "bgfx: stats unavailable");
    }

    const std::string cameraLine = fmt::format(
        "Camera: ({:.1f}, {:.1f}, {:.1f})",
        frameDebugData.cameraPosition.x,
        frameDebugData.cameraPosition.y,
        frameDebugData.cameraPosition.z);
    bgfx::dbgTextPrintf(0, 8, 0x0f, "%s", cameraLine.c_str());

    if (frameDebugData.hasTarget)
    {
        const std::string targetLine = fmt::format(
            "Target block: ({}, {}, {})",
            frameDebugData.targetBlock.x,
            frameDebugData.targetBlock.y,
            frameDebugData.targetBlock.z);
        bgfx::dbgTextPrintf(0, 9, 0x0f, "%s", targetLine.c_str());
    }
    else
    {
        bgfx::dbgTextPrintf(0, 9, 0x0f, "Target block: none");
    }

    drawHotbarHud(hotbarRow, frameDebugData);
    drawHotbarKeyHintsRow(hotbarKeyRow);
    drawHealthHud(healthRow, frameDebugData);
    if (textHeight >= 10)
    {
        drawBagHud(
            bagTitleRow,
            bagSepRow,
            bagRow0,
            bagRow1,
            bagRow2,
            frameDebugData);
    }
    bgfx::dbgTextPrintf(
        0,
        controlsRow0,
        0x0e,
        "Controls: WASD move, Shift sneak, Ctrl sprint, Space jump, mouse look");
    bgfx::dbgTextPrintf(
        0,
        controlsRow1,
        0x0e,
        "LMB mine, RMB place, 1-9 select hotbar, Tab capture, Esc pause menu");

    bgfx::frame();
}

void Renderer::destroySceneMeshes()
{
    std::vector<std::uint64_t> sceneMeshIds;
    sceneMeshIds.reserve(sceneMeshes_.size());

    for (const auto& [sceneMeshId, sceneMesh] : sceneMeshes_)
    {
        static_cast<void>(sceneMeshId);
        static_cast<void>(sceneMesh);
        sceneMeshIds.push_back(sceneMeshId);
    }

    for (const std::uint64_t sceneMeshId : sceneMeshIds)
    {
        destroySceneMesh(sceneMeshId);
    }
}

void Renderer::destroySceneMesh(const std::uint64_t sceneMeshId)
{
    const auto sceneMeshIt = sceneMeshes_.find(sceneMeshId);
    if (sceneMeshIt == sceneMeshes_.end())
    {
        return;
    }

    bgfx::destroy(toVertexBufferHandle(sceneMeshIt->second.vertexBufferHandle));
    bgfx::destroy(toIndexBufferHandle(sceneMeshIt->second.indexBufferHandle));
    sceneMeshes_.erase(sceneMeshIt);
}

void Renderer::drawMainMenuLogo()
{
    if (logoProgramHandle_ == UINT16_MAX || logoTextureHandle_ == UINT16_MAX || logoSamplerHandle_ == UINT16_MAX)
    {
        return;
    }
    if (logoWidthPx_ == 0 || logoHeightPx_ == 0 || width_ == 0 || height_ == 0)
    {
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(4, ChunkVertex::layout()) < 4)
    {
        return;
    }
    if (bgfx::getAvailTransientIndexBuffer(6) < 6)
    {
        return;
    }

    const float aspect =
        static_cast<float>(logoWidthPx_) / static_cast<float>(logoHeightPx_);
    constexpr float kMarginTop = 42.0f;
    const float maxWidth = std::min(620.0f, static_cast<float>(width_) * 0.78f);
    float drawW = maxWidth;
    float drawH = drawW / aspect;
    const float maxHeight = std::min(static_cast<float>(height_) * 0.15f, 176.0f);
    if (drawH > maxHeight)
    {
        drawH = maxHeight;
        drawW = drawH * aspect;
    }

    const float x0 = (static_cast<float>(width_) - drawW) * 0.5f;
    const float y0 = kMarginTop;
    const float x1 = x0 + drawW;
    const float y1 = y0 + drawH;

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
    bgfx::setViewTransform(kMainView, view, proj);

    const float identity[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    bgfx::setTransform(identity);

    ChunkVertex vertices[4] = {
        ChunkVertex{.x = x0, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 0.0f, .abgr = 0xffffffff},
        ChunkVertex{.x = x1, .y = y0, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 0.0f, .abgr = 0xffffffff},
        ChunkVertex{.x = x1, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 1.0f, .v = 1.0f, .abgr = 0xffffffff},
        ChunkVertex{.x = x0, .y = y1, .z = 0.0f, .nx = 0.0f, .ny = 0.0f, .nz = 1.0f, .u = 0.0f, .v = 1.0f, .abgr = 0xffffffff},
    };

    bgfx::TransientVertexBuffer tvb{};
    bgfx::allocTransientVertexBuffer(&tvb, 4, ChunkVertex::layout());
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

    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    bgfx::setTexture(0, toUniformHandle(logoSamplerHandle_), toTextureHandle(logoTextureHandle_));
    bgfx::setState(
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_ALWAYS);
    bgfx::submit(kMainView, toProgramHandle(logoProgramHandle_));
}
}  // namespace vibecraft::render
