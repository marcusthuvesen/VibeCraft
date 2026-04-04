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
constexpr int kPreferredButtonLineCount = 11;
constexpr int kMinButtonLineCount = 7;
constexpr int kMinOuterWidth = 104;
constexpr int kMaxOuterWidth = 156;
constexpr int kSubtitleRuleAndGapRows = 3;
}  // namespace MainMenuLayout

/// Rows match `drawMainMenuMultiplayerOverlay` / multiplayer hit tests (dbg text grid).
namespace MultiplayerMenuLayout
{
constexpr int kWide = 84;
/// Same vertical pitch as main menu buttons (`MainMenuLayout::kPreferredButtonLineCount`).
constexpr int kMainButtonLineCount = 9;
/// Smaller framed fields (IP / port) — still solid-filled like main menu.
constexpr int kFieldButtonLineCount = 5;
constexpr int kHubHostRow = 8;
constexpr int kHubJoinRow = kHubHostRow + kMainButtonLineCount;
constexpr int kHubBackRow = kHubJoinRow + kMainButtonLineCount;
constexpr int kHostStartRow = 15;
constexpr int kHostBackRow = kHostStartRow + kMainButtonLineCount;
constexpr int kJoinAddrFieldRow = 8;
/// Vertical distance between join preset / primary buttons (matches main menu pitch).
constexpr int kJoinPresetPitchRows = kMainButtonLineCount;
constexpr int kJoinPresetSlotMax = 3;

[[nodiscard]] inline int joinPresetSlotCountForLayout(const std::size_t presetLabelCount)
{
    return static_cast<int>(std::min(presetLabelCount, std::size_t{kJoinPresetSlotMax}));
}

[[nodiscard]] inline int joinManualSectionOffsetRows(const int presetSlotCount)
{
    return kJoinPresetPitchRows * std::clamp(presetSlotCount, 0, kJoinPresetSlotMax);
}

[[nodiscard]] inline int joinPresetButtonStartRow(const int presetIndex)
{
    return kJoinAddrFieldRow + presetIndex * kJoinPresetPitchRows;
}

inline void joinManualSectionRows(
    const int presetSlotCount,
    int& addrFieldRow,
    int& portLabelRow,
    int& portFieldRow,
    int& connectRow,
    int& backRow)
{
    const int off = joinManualSectionOffsetRows(presetSlotCount);
    addrFieldRow = kJoinAddrFieldRow + off;
    // Spacing for kFieldButtonLineCount address row, label, port field, then kMainButtonLineCount connect/back.
    portLabelRow = addrFieldRow + 6;
    portFieldRow = addrFieldRow + 9;
    connectRow = addrFieldRow + 16;
    backRow = addrFieldRow + 25;
}

/// Title row in the legacy (fixed) layout; used with `multiplayerMenuRowShift` to center panels vertically.
constexpr int kMultiplayerTitleAnchorRow = 5;
/// Extra dbg rows below the logo reservation so Hub / Host / Join sit clearly under the title logo (not crowding it).
constexpr int kMultiplayerExtraRowsBelowLogo = 6;

[[nodiscard]] inline int multiplayerJoinBottomRow(const int presetSlotCount)
{
    int addrFieldRow = 0;
    [[maybe_unused]] int portLabelUnused = 0;
    int portFieldRow = 0;
    int connectRow = 0;
    int backRow = 0;
    joinManualSectionRows(presetSlotCount, addrFieldRow, portLabelUnused, portFieldRow, connectRow, backRow);
    return backRow + kMainButtonLineCount - 1;
}

[[nodiscard]] inline int multiplayerPanelContentRows(
    const FrameDebugData::MainMenuMultiplayerPanel panel,
    const int joinPresetSlotCount)
{
    switch (panel)
    {
    case FrameDebugData::MainMenuMultiplayerPanel::Hub:
        return (kHubBackRow + kMainButtonLineCount - 1) - kMultiplayerTitleAnchorRow + 1;
    case FrameDebugData::MainMenuMultiplayerPanel::Host:
        return (kHostBackRow + kMainButtonLineCount - 1) - kMultiplayerTitleAnchorRow + 1;
    case FrameDebugData::MainMenuMultiplayerPanel::Join:
        return multiplayerJoinBottomRow(joinPresetSlotCount) - kMultiplayerTitleAnchorRow + 1;
    case FrameDebugData::MainMenuMultiplayerPanel::None:
    default:
        return 1;
    }
}

/// Shifts legacy row indices so the multiplayer block is vertically centered (matches `computeMainMenuLayout` + logo bias).
[[nodiscard]] inline int multiplayerMenuRowShift(
    const int textHeight,
    const FrameDebugData::MainMenuMultiplayerPanel panel,
    const int joinPresetSlotCount,
    const int mainMenuContentTopBias)
{
    if (textHeight <= 0 || panel == FrameDebugData::MainMenuMultiplayerPanel::None)
    {
        return 0;
    }
    const int contentRows = multiplayerPanelContentRows(panel, joinPresetSlotCount);
    if (contentRows <= 0)
    {
        return 0;
    }
    const int centeredBase = (textHeight - contentRows) / 2;
    const int minFirstRow = mainMenuContentTopBias + kMultiplayerExtraRowsBelowLogo;
    const int firstRow = std::clamp(
        std::max(centeredBase + mainMenuContentTopBias + kMultiplayerExtraRowsBelowLogo, minFirstRow),
        1,
        std::max(1, textHeight - contentRows));
    return firstRow - kMultiplayerTitleAnchorRow;
}
}  // namespace MultiplayerMenuLayout

/// Pause menu dbg-text grid; must match `drawPauseMenuOverlay` and pause hit tests.
namespace PauseMenuLayout
{
constexpr int kWideChars = 116;
/// Framed control height: top/bottom rule with padded interior lines.
constexpr int kButtonRowSpan = 5;
constexpr int kButtonLabelRowOffset = kButtonRowSpan / 2;
constexpr int kButtonGapRows = 2;
constexpr int kButtonPitch = kButtonRowSpan + kButtonGapRows;
constexpr int kMainButtonCount = 5;
constexpr int kPauseSoundButtonCount = 3;
constexpr int kPauseGameButtonCount = 7;

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

[[nodiscard]] inline int pauseSoundTotalRows()
{
    return kPauseSoundButtonCount * kButtonRowSpan + (kPauseSoundButtonCount - 1) * kButtonGapRows;
}

[[nodiscard]] inline int pauseSoundFirstButtonRow(const int textHeight)
{
    const int total = pauseSoundTotalRows();
    const int maxFirst = std::max(2, textHeight - total - 1);
    return std::clamp((textHeight - total) / 2, 2, maxFirst);
}

[[nodiscard]] inline int pauseGameTotalRows()
{
    return kPauseGameButtonCount * kButtonRowSpan + (kPauseGameButtonCount - 1) * kButtonGapRows;
}

[[nodiscard]] inline int pauseGameFirstButtonRow(const int textHeight)
{
    const int total = pauseGameTotalRows();
    const int maxFirst = std::max(2, textHeight - total - 1);
    return std::clamp((textHeight - total) / 2, 2, maxFirst);
}

[[nodiscard]] inline int pauseSoundTitleRow(const int textHeight)
{
    return std::max(1, pauseSoundFirstButtonRow(textHeight) - 3);
}

[[nodiscard]] inline int pauseSoundMusicButtonRow(const int textHeight)
{
    return pauseSoundFirstButtonRow(textHeight);
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
    return std::max(1, pauseGameFirstButtonRow(textHeight) - 3);
}

[[nodiscard]] inline int pauseGameMobButtonRow(const int textHeight)
{
    return pauseGameFirstButtonRow(textHeight);
}

[[nodiscard]] inline int pauseGameCreativeButtonRow(const int textHeight)
{
    return pauseGameMobButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameDifficultyButtonRow(const int textHeight)
{
    return pauseGameCreativeButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameBiomeButtonRow(const int textHeight)
{
    return pauseGameDifficultyButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameTravelButtonRow(const int textHeight)
{
    return pauseGameBiomeButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameWeatherButtonRow(const int textHeight)
{
    return pauseGameTravelButtonRow(textHeight) + kButtonPitch;
}

[[nodiscard]] inline int pauseGameBackButtonRow(const int textHeight)
{
    return pauseGameWeatherButtonRow(textHeight) + kButtonPitch;
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
constexpr int kGameDifficultyButtonRow = kGameMobButtonRow + kButtonPitch;
constexpr int kGameBiomeButtonRow = kGameDifficultyButtonRow + kButtonPitch;
constexpr int kGameTravelButtonRow = kGameBiomeButtonRow + kButtonPitch;
constexpr int kGameWeatherButtonRow = kGameTravelButtonRow + kButtonPitch;
constexpr int kGameBackButtonRow = kGameWeatherButtonRow + kButtonPitch;
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

/// Pixel sizing for the title logo; keep in sync with `Renderer::drawMainMenuLogo`.
namespace MainMenuLogoDraw
{
inline constexpr float kMarginTopPx = 28.0f;
inline constexpr float kMaxWidthFrac = 0.94f;
inline constexpr float kMaxWidthCapPx = 1120.0f;
inline constexpr float kMaxHeightFrac = 0.38f;
inline constexpr float kMaxHeightCapPx = 400.0f;
}  // namespace MainMenuLogoDraw

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
    float equipmentOriginX = 0.0f;
    float equipmentOriginY = 0.0f;
    float craftingOriginX = 0.0f;
    float craftingOriginY = 0.0f;
    float furnaceFuelSlotX = 0.0f;
    float furnaceFuelSlotY = 0.0f;
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
    vibecraft::render::CraftingUiMode mode,
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
[[nodiscard]] float distanceSqCameraToAabbXZ(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax);
[[nodiscard]] float distanceSqCameraToAabbDownWeighted(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    float belowCameraWeight,
    float aboveCameraWeight);

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
[[nodiscard]] bgfx::TextureHandle createProceduralMobTexture(vibecraft::game::MobKind mobKind);

void drawWeatherClouds(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawWeatherRain(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawSkyAtmosphereVeils(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawSkyCirrusBands(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawSkyHorizonBloom(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawSkyNebulaCanopy(DebugDrawEncoder& debugDrawEncoder, const CameraFrameData& cameraFrameData);
void drawCoordinateOverlay(const FrameDebugData& frameDebugData);

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
void drawChatOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);

void drawMainMenuOverlay(
    const FrameDebugData& frameDebugData,
    std::uint16_t textWidth,
    std::uint16_t textHeight,
    int mainMenuTitleContentRowOffset = 0);
void drawMainMenuMultiplayerOverlay(
    const FrameDebugData& frameDebugData,
    std::uint16_t textWidth,
    std::uint16_t textHeight,
    int mainMenuTitleContentRowOffset);
void drawPauseMenuOverlay(const FrameDebugData& frameDebugData, std::uint16_t textWidth, std::uint16_t textHeight);

}  // namespace vibecraft::render::detail
