#include "vibecraft/render/Renderer.hpp"

#include <SDL3/SDL_filesystem.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "debugdraw.h"

namespace vibecraft::render
{
namespace
{
constexpr bgfx::ViewId kMainView = 0;
constexpr std::uint32_t kDefaultResetFlags = BGFX_RESET_VSYNC;
constexpr std::uint64_t kChunkRenderState =
    BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

struct ChunkVertex
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    std::uint32_t abgr = 0xffffffff;

    static bgfx::VertexLayout layout()
    {
        bgfx::VertexLayout vertexLayout;
        vertexLayout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
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

    const char* const basePathCStr = SDL_GetBasePath();
    const std::filesystem::path basePath = basePathCStr != nullptr ? basePathCStr : "";

    const std::filesystem::path shaderPath =
        basePath / "shaders" / profileDirectory / (shaderName + ".bin");

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

    chunkProgramHandle_ = chunkProgram.idx;
    return true;
}

void Renderer::shutdown()
{
    if (!initialized_)
    {
        return;
    }

    destroySceneMeshes();
    if (chunkProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(toProgramHandle(chunkProgramHandle_));
        chunkProgramHandle_ = UINT16_MAX;
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
            bgfx::setState(kChunkRenderState);
            bgfx::submit(kMainView, toProgramHandle(chunkProgramHandle_));
        }
    }

    DebugDrawEncoder debugDrawEncoder;
    debugDrawEncoder.begin(kMainView);
    debugDrawEncoder.setColor(0xff455a64);
    debugDrawEncoder.drawGrid(Axis::Y, bx::Vec3(0.0f, 0.0f, 0.0f), 48, 1.0f);
    debugDrawEncoder.drawAxis(0.0f, 0.0f, 0.0f, 2.0f);

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
        "Chunks: %u  Dirty: %u  Resident: %u",
        frameDebugData.chunkCount,
        frameDebugData.dirtyChunkCount,
        frameDebugData.residentChunkCount);
    bgfx::dbgTextPrintf(0, 6, 0x0f, "Faces: %u  Visible chunks: %u", frameDebugData.totalFaces, visibleChunkCount);

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

    bgfx::dbgTextPrintf(0, 11, 0x0e, "Controls: WASD move, Space/Shift fly, mouse look");
    bgfx::dbgTextPrintf(0, 12, 0x0e, "LMB remove, RMB place, Tab capture mouse, Esc release mouse");

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
}  // namespace vibecraft::render
