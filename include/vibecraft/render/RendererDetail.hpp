#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include <bgfx/bgfx.h>
#include <bx/allocator.h>
#include <bx/bounds.h>

#include <glm/vec3.hpp>

#include "debugdraw.h"

#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererInternal.hpp"

namespace vibecraft::render::detail
{
namespace MainMenuLayout
{
constexpr int kButtonLineCount = 5;
constexpr int kMinOuterWidth = 46;
constexpr int kMaxOuterWidth = 62;
constexpr int kSubtitleRuleAndGapRows = 3;
}  // namespace MainMenuLayout

struct MainMenuComputedLayout
{
    int centerCol = 0;
    int outerWidth = MainMenuLayout::kMinOuterWidth;
    int firstContentRow = 1;
    int subtitleRow = 1;
    int ruleRow = 2;
    std::array<int, 5> buttonTopRows{};
    int iconHintsRow = 1;
};

struct HotbarLayoutPx
{
    float originX = 0.0f;
    float slotTopY = 0.0f;
    float slotSize = 0.0f;
    float gap = 0.0f;
    float totalWidth = 0.0f;
};

[[nodiscard]] MainMenuComputedLayout computeMainMenuLayout(int textWidth, int textHeight);

[[nodiscard]] HotbarLayoutPx computeHotbarLayoutPx(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint16_t textHeight,
    std::uint16_t hotbarRow);

[[nodiscard]] int computeCenteredColumnStart(int totalChars);
[[nodiscard]] int computeHotbarGridWidthChars();
[[nodiscard]] int computeBagGridWidthChars();
[[nodiscard]] int computeCenteredHotbarStartColumn();
[[nodiscard]] int computeHealthHudWidthChars(int heartCount);

[[nodiscard]] bool isAabbInsideFrustum(const bx::Plane* frustumPlanes, const bx::Aabb& aabb);
[[nodiscard]] float distanceSqCameraToAabb(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax);

[[nodiscard]] std::filesystem::path runtimeAssetPath(const std::filesystem::path& relativePath);

[[nodiscard]] bgfx::ProgramHandle loadProgram(const std::string& vertexShaderName, const std::string& fragmentShaderName);

[[nodiscard]] bgfx::TextureHandle createChunkAtlasTexture();
[[nodiscard]] bgfx::TextureHandle createLogoTextureFromPng(
    bx::AllocatorI& allocator,
    std::uint16_t& outWidth,
    std::uint16_t& outHeight);
[[nodiscard]] bgfx::TextureHandle createMinecraftStyleCrosshairTexture();

void drawWeatherClouds(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawWeatherRain(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);

void drawHealthHud(std::uint16_t row, const FrameDebugData& frameDebugData);
void drawHotbarHud(std::uint16_t row, const FrameDebugData& frameDebugData);
void drawHotbarKeyHintsRow(
    std::uint16_t row,
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint16_t textWidth,
    std::uint16_t textHeight,
    std::uint16_t hotbarRow);
void drawHotbarStackCounts(
    std::uint16_t row,
    const FrameDebugData& frameDebugData,
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint16_t textWidth,
    std::uint16_t textHeight,
    std::uint16_t hotbarRow);
void drawBagHud(
    std::uint16_t titleRow,
    std::uint16_t separatorRow,
    std::uint16_t row0,
    std::uint16_t row1,
    std::uint16_t row2,
    const FrameDebugData& frameDebugData);

void drawMainMenuOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);
void drawPauseMenuOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);

}  // namespace vibecraft::render::detail
