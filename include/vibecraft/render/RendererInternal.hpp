#pragma once

#include <algorithm>
#include <cstdint>

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::render::detail
{
inline constexpr bgfx::ViewId kMainView = 0;
inline constexpr bgfx::ViewId kUiView = 1;
// Do not add BGFX_RESET_MSAA_* here. With MSAA enabled, bgfx debug text (dbgText) on the Metal
// backend has broken the main menu: full-screen magenta/pink and no proper overlay. Keep MSAA
// off for the default swapchain until a separate non-dbgText UI path can use it safely.
inline constexpr std::uint32_t kDefaultResetFlags = BGFX_RESET_VSYNC;
/// `bgfx::setViewClear` packs color as **0xRRGGBBAA** (see bgfx `Clear::set`). Not ABGR vertex color.
inline constexpr std::uint32_t kMainMenuClearColor = 0x1d2a40ff;
inline constexpr std::uint64_t kChunkRenderState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
    | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS;
inline constexpr std::uint16_t kChunkAtlasWidth = vibecraft::kChunkAtlasWidthPx;
inline constexpr std::uint16_t kChunkAtlasHeight = vibecraft::kChunkAtlasHeightPx;

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

[[nodiscard]] inline bgfx::VertexBufferHandle toVertexBufferHandle(const std::uint16_t handleIndex)
{
    return bgfx::VertexBufferHandle{handleIndex};
}

[[nodiscard]] inline bgfx::IndexBufferHandle toIndexBufferHandle(const std::uint16_t handleIndex)
{
    return bgfx::IndexBufferHandle{handleIndex};
}

[[nodiscard]] inline bgfx::ProgramHandle toProgramHandle(const std::uint16_t handleIndex)
{
    return bgfx::ProgramHandle{handleIndex};
}

[[nodiscard]] inline bgfx::TextureHandle toTextureHandle(const std::uint16_t handleIndex)
{
    return bgfx::TextureHandle{handleIndex};
}

[[nodiscard]] inline bgfx::UniformHandle toUniformHandle(const std::uint16_t handleIndex)
{
    return bgfx::UniformHandle{handleIndex};
}

/// Packed **0xRRGGBBAA** for `bgfx::setViewClear` / APIs that follow the same channel order.
[[nodiscard]] inline std::uint32_t packRgba8(const glm::vec3& color, const float alpha = 1.0f)
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

/// Packed **0xAABBGGRR** for vertex `abgr` attributes and debug draw colors — not for `setViewClear`.
[[nodiscard]] inline std::uint32_t packAbgr8(const glm::vec3& color, const float alpha = 1.0f)
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

inline void setVec4Uniform(
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

inline void setVec4Uniform(const std::uint16_t handleIndex, const glm::vec4& xyzw)
{
    if (handleIndex == UINT16_MAX)
    {
        return;
    }

    const float values[4] = {
        xyzw.x,
        xyzw.y,
        xyzw.z,
        xyzw.w,
    };
    bgfx::setUniform(toUniformHandle(handleIndex), values);
}

}  // namespace vibecraft::render::detail
