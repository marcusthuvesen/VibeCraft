#pragma once

#include <algorithm>
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
constexpr int kPreferredButtonLineCount = 9;
constexpr int kMinButtonLineCount = 5;
constexpr int kMinOuterWidth = 92;
constexpr int kMaxOuterWidth = 140;
constexpr int kSubtitleRuleAndGapRows = 3;
}  // namespace MainMenuLayout

/// Rows match `drawMainMenuMultiplayerOverlay` / multiplayer hit tests (dbg text grid).
namespace MultiplayerMenuLayout
{
constexpr int kWide = 84;
constexpr int kHubHostRow = 8;
constexpr int kHubJoinRow = 15;
constexpr int kHubBackRow = 22;
constexpr int kHostStartRow = 15;
constexpr int kHostBackRow = 24;
constexpr int kJoinAddrFieldRow = 8;
constexpr int kJoinPortFieldRow = 15;
constexpr int kJoinConnectRow = 22;
constexpr int kJoinBackRow = 29;
}  // namespace MultiplayerMenuLayout

/// Pause menu dbg-text grid; must match `drawPauseMenuOverlay` and pause hit tests.
namespace PauseMenuLayout
{
constexpr int kWideChars = 96;
/// Framed control height: top rule, label, bottom rule (one text line per button).
constexpr int kButtonRowSpan = 3;
constexpr int kButtonGapRows = 2;
constexpr int kButtonPitch = kButtonRowSpan + kButtonGapRows;
constexpr int kMainButtonCount = 5;
constexpr int kSubmenuButtonCount = 3;

[[nodiscard]] inline int mainPauseMenuTotalRows()
{
    return kMainButtonCount * kButtonRowSpan + (kMainButtonCount - 1) * kButtonGapRows;
}

[[nodiscard]] inline int mainPauseMenuFirstButtonRow(const int textHeight)
{
    const int total = mainPauseMenuTotalRows();
    const int maxFirst = std::max(1, textHeight - total - 1);
    return std::clamp((textHeight - total) / 2, 1, maxFirst);
}

[[nodiscard]] inline int submenuTotalRows()
{
    return kSubmenuButtonCount * kButtonRowSpan + (kSubmenuButtonCount - 1) * kButtonGapRows;
}

[[nodiscard]] inline int submenuFirstButtonRow(const int textHeight)
{
    const int total = submenuTotalRows();
    const int maxFirst = std::max(2, textHeight - total - 1);
    return std::clamp((textHeight - total) / 2, 2, maxFirst);
}

[[nodiscard]] inline int pauseSoundTitleRow(const int textHeight)
{
    return std::max(1, submenuFirstButtonRow(textHeight) - 3);
}

[[nodiscard]] inline int pauseSoundMusicButtonRow(const int textHeight)
{
    return submenuFirstButtonRow(textHeight);
}

[[nodiscard]] inline int pauseSoundSfxButtonRow(const int textHeight)
{
    return pauseSoundMusicButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseSoundBackButtonRow(const int textHeight)
{
    return pauseSoundSfxButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameTitleRow(const int textHeight)
{
    return std::max(1, submenuFirstButtonRow(textHeight) - 3);
}

[[nodiscard]] inline int pauseGameMobButtonRow(const int textHeight)
{
    return submenuFirstButtonRow(textHeight);
}

[[nodiscard]] inline int pauseGameBiomeButtonRow(const int textHeight)
{
    return pauseGameMobButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameBackButtonRow(const int textHeight)
{
    return pauseGameBiomeButtonRow(textHeight) + kButtonPitch;
}

constexpr int kSoundTitleRow = 3;
constexpr int kSoundMusicButtonRow = 6;
constexpr int kSoundSfxButtonRow = kSoundMusicButtonRow + kButtonPitch;
constexpr int kSoundBackButtonRow = kSoundSfxButtonRow + kButtonPitch;
/// Slider fill region inside the framed button's inner text area.
constexpr int kSoundSliderFillStartInner = 38;
constexpr int kSoundSliderFillChars = 18;

constexpr int kGameTitleRow = 3;
constexpr int kGameMobButtonRow = 6;
constexpr int kGameBiomeButtonRow = kGameMobButtonRow + kButtonPitch;
constexpr int kGameBackButtonRow = kGameBiomeButtonRow + kButtonPitch;
}  // namespace PauseMenuLayout

struct MainMenuComputedLayout
{
    int centerCol = 0;
    int outerWidth = MainMenuLayout::kMinOuterWidth;
    int firstContentRow = 1;
    int subtitleRow = 1;
    int ruleRow = 2;
    int buttonLineCount = MainMenuLayout::kMinButtonLineCount;
    std::array<int, 5> buttonTopRows{};
    int iconHintsRow = 1;
};

/// Dbg-text rows consumed by `drawMainMenuLogo` (plus one gap); 0 if no logo.
[[nodiscard]] int mainMenuLogoReservedDbgRows(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint16_t textHeight,
    std::uint16_t logoWidthPx,
    std::uint16_t logoHeightPx);

struct HotbarLayoutPx
{
    float originX = 0.0f;
    float slotTopY = 0.0f;
    float slotSize = 0.0f;
    float gap = 0.0f;
    float totalWidth = 0.0f;
};

struct CraftingOverlayLayoutPx
{
    float panelLeft = 0.0f;
    float panelTop = 0.0f;
    float panelRight = 0.0f;
    float panelBottom = 0.0f;
    float slotSize = 0.0f;
    float slotGap = 0.0f;
    float craftingOriginX = 0.0f;
    float craftingOriginY = 0.0f;
    float resultSlotX = 0.0f;
    float resultSlotY = 0.0f;
    float inventoryOriginX = 0.0f;
    float inventoryOriginY = 0.0f;
};

[[nodiscard]] MainMenuComputedLayout computeMainMenuLayout(
    int textWidth, int textHeight, int contentTopRowOffset = 0);

[[nodiscard]] HotbarLayoutPx computeHotbarLayoutPx(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    std::uint16_t textHeight,
    std::uint16_t hotbarRow);
[[nodiscard]] CraftingOverlayLayoutPx computeCraftingOverlayLayoutPx(
    std::uint32_t windowWidth,
    std::uint32_t windowHeight,
    bool useWorkbench);

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
[[nodiscard]] bgfx::TextureHandle createTextureFromPng(
    const std::filesystem::path& relativePath,
    std::uint16_t textureFlags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
    std::uint16_t* outWidthPx = nullptr,
    std::uint16_t* outHeightPx = nullptr,
    bool stripWhiteEdgeMatte = false);
[[nodiscard]] bgfx::TextureHandle createLogoTextureFromPng(
    bx::AllocatorI& allocator,
    std::uint16_t& outWidth,
    std::uint16_t& outHeight);
[[nodiscard]] bgfx::TextureHandle createMinecraftStyleCrosshairTexture();
[[nodiscard]] bgfx::TextureHandle createBlockBreakOverlayTexture(int stage);
[[nodiscard]] bgfx::TextureHandle createHeartTexture(int fillStage);

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

void drawMainMenuOverlay(
    const FrameDebugData& frameDebugData,
    std::uint16_t textWidth,
    std::uint16_t textHeight,
    int mainMenuTitleContentRowOffset = 0);
void drawMainMenuMultiplayerOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);
void drawPauseMenuOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);

}  // namespace vibecraft::render::detail
