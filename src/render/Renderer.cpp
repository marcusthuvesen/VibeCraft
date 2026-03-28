#include "vibecraft/render/Renderer.hpp"

#include <SDL3/SDL_filesystem.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/bounds.h>
#include <bx/math.h>
#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cmath>
#include <string>
#include <utility>
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
constexpr std::uint16_t kChunkAtlasWidth = 64;
constexpr std::uint16_t kChunkAtlasHeight = 32;

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

[[nodiscard]] int computeCenteredHotbarStartColumn()
{
    constexpr int kCellChars = 5;
    constexpr int kGap = 1;
    constexpr int kSlotCount = 9;
    const int totalChars = kSlotCount * kCellChars + (kSlotCount - 1) * kGap;

    const bgfx::Stats* stats = bgfx::getStats();
    const std::uint16_t textWidth = stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
    const int startCol = (static_cast<int>(textWidth) - totalChars) / 2;
    return std::max(0, startCol);
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

    chunkProgramHandle_ = chunkProgram.idx;
    chunkAtlasTextureHandle_ = chunkAtlasTexture.idx;
    chunkAtlasSamplerHandle_ = chunkAtlasSampler.idx;
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
            if (chunkAtlasTextureHandle_ != UINT16_MAX && chunkAtlasSamplerHandle_ != UINT16_MAX)
            {
                bgfx::setTexture(
                    0,
                    toUniformHandle(chunkAtlasSamplerHandle_),
                    toTextureHandle(chunkAtlasTextureHandle_));
            }
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
    const bgfx::Stats* const bgfxStats = bgfx::getStats();
    const std::uint16_t textHeight = bgfxStats != nullptr ? bgfxStats->textHeight : 30;
    const std::uint16_t hotbarRow = textHeight > 0 ? static_cast<std::uint16_t>(textHeight - 1) : 0;
    const std::uint16_t hotbarKeyRow = textHeight > 1 ? static_cast<std::uint16_t>(textHeight - 2) : 0;
    const std::uint16_t bagHudRow = textHeight > 2 ? static_cast<std::uint16_t>(textHeight - 3) : 0;
    const std::uint16_t controlsRow0 = textHeight > 3 ? static_cast<std::uint16_t>(textHeight - 4) : 0;
    const std::uint16_t controlsRow1 = textHeight > 4 ? static_cast<std::uint16_t>(textHeight - 5) : 0;

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
    dbgTextPrintfCenteredRow(bagHudRow, 0x0a, frameDebugData.bagLine);
    bgfx::dbgTextPrintf(
        0,
        controlsRow0,
        0x0e,
        "Controls: WASD move, Shift sneak, Ctrl sprint, Space jump, mouse look");
    bgfx::dbgTextPrintf(
        0,
        controlsRow1,
        0x0e,
        "LMB mine, RMB place, 1-9 select hotbar, Tab capture, Esc release");

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
