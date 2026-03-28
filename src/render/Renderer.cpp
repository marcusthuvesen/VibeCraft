#include "vibecraft/render/Renderer.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <fmt/format.h>

#include "debugdraw.h"

namespace vibecraft::render
{
namespace
{
constexpr bgfx::ViewId kMainView = 0;
constexpr std::uint32_t kDefaultResetFlags = BGFX_RESET_VSYNC;

[[nodiscard]] GeometryHandle toGeometryHandle(const std::uint16_t handleIndex)
{
    return GeometryHandle{handleIndex};
}
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
    return true;
}

void Renderer::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    destroySceneMeshes();
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

void Renderer::replaceSceneMeshes(const std::vector<SceneMeshData>& sceneMeshes)
{
    if (!initialized_)
    {
        return;
    }

    destroySceneMeshes();

    for (const SceneMeshData& sceneMesh : sceneMeshes)
    {
        if (sceneMesh.positions.empty() || sceneMesh.indices.empty())
        {
            continue;
        }

        std::vector<DdVertex> debugVertices;
        debugVertices.reserve(sceneMesh.positions.size());

        for (const glm::vec3& position : sceneMesh.positions)
        {
            debugVertices.push_back(DdVertex{
                .x = position.x,
                .y = position.y,
                .z = position.z,
            });
        }

        const GeometryHandle geometryHandle = ddCreateGeometry(
            static_cast<std::uint32_t>(debugVertices.size()),
            debugVertices.data(),
            static_cast<std::uint32_t>(sceneMesh.indices.size()),
            sceneMesh.indices.data(),
            true);

        sceneMeshHandles_[sceneMesh.id] = geometryHandle.idx;
        sceneMeshColors_[sceneMesh.id] = sceneMesh.abgr;
    }
}

void Renderer::renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData)
{
    if (!initialized_ || width_ == 0 || height_ == 0)
    {
        return;
    }

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

    bgfx::setViewRect(kMainView, 0, 0, static_cast<std::uint16_t>(width_), static_cast<std::uint16_t>(height_));
    bgfx::setViewTransform(kMainView, view, projection);
    bgfx::touch(kMainView);

    DebugDrawEncoder debugDrawEncoder;
    debugDrawEncoder.begin(kMainView);
    debugDrawEncoder.setColor(0xff455a64);
    debugDrawEncoder.drawGrid(Axis::Y, bx::Vec3(0.0f, 0.0f, 0.0f), 48, 1.0f);
    debugDrawEncoder.drawAxis(0.0f, 0.0f, 0.0f, 2.0f);

    for (const auto& [sceneMeshId, handleIndex] : sceneMeshHandles_)
    {
        const auto colorIt = sceneMeshColors_.find(sceneMeshId);
        debugDrawEncoder.setColor(colorIt != sceneMeshColors_.end() ? colorIt->second : 0xff90caf9);
        debugDrawEncoder.draw(toGeometryHandle(handleIndex));
    }

    if (frameDebugData.hasTarget)
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
    bgfx::dbgTextPrintf(0, 1, 0x0f, "VibeCraft foundation slice");
    bgfx::dbgTextPrintf(0, 3, 0x0a, "%s", frameDebugData.statusLine.c_str());
    bgfx::dbgTextPrintf(
        0,
        5,
        0x0f,
        "Chunks: %u  Dirty: %u  Visible faces: %u",
        frameDebugData.chunkCount,
        frameDebugData.dirtyChunkCount,
        frameDebugData.totalFaces);

    const std::string cameraLine = fmt::format(
        "Camera: ({:.1f}, {:.1f}, {:.1f})",
        frameDebugData.cameraPosition.x,
        frameDebugData.cameraPosition.y,
        frameDebugData.cameraPosition.z);
    bgfx::dbgTextPrintf(0, 7, 0x0f, "%s", cameraLine.c_str());

    if (frameDebugData.hasTarget)
    {
        const std::string targetLine = fmt::format(
            "Target block: ({}, {}, {})",
            frameDebugData.targetBlock.x,
            frameDebugData.targetBlock.y,
            frameDebugData.targetBlock.z);
        bgfx::dbgTextPrintf(0, 8, 0x0f, "%s", targetLine.c_str());
    }
    else
    {
        bgfx::dbgTextPrintf(0, 8, 0x0f, "Target block: none");
    }

    bgfx::dbgTextPrintf(0, 10, 0x0e, "Controls: WASD move, Space/Shift fly, mouse look");
    bgfx::dbgTextPrintf(0, 11, 0x0e, "LMB remove, RMB place, Tab capture mouse, Esc release mouse");

    bgfx::frame();
}

void Renderer::destroySceneMeshes()
{
    for (const auto& [sceneMeshId, handleIndex] : sceneMeshHandles_)
    {
        static_cast<void>(sceneMeshId);
        ddDestroy(toGeometryHandle(handleIndex));
    }

    sceneMeshHandles_.clear();
    sceneMeshColors_.clear();
}
}  // namespace vibecraft::render
