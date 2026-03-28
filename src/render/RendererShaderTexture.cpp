#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <SDL3/SDL_filesystem.h>
#include <bgfx/bgfx.h>
#include <bx/allocator.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::render::detail
{


[[nodiscard]] std::filesystem::path runtimeAssetPath(const std::filesystem::path& relativePath)
{
    const char* const basePathCStr = SDL_GetBasePath();
    const std::filesystem::path basePath = basePathCStr != nullptr ? basePathCStr : "";
    return basePath / relativePath;
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
        static_cast<std::size_t>(detail::kChunkAtlasWidth) * static_cast<std::size_t>(detail::kChunkAtlasHeight) * 4U;
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
        detail::kChunkAtlasWidth,
        detail::kChunkAtlasHeight,
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
    static_cast<void>(allocator);
    outWidth = 0;
    outHeight = 0;
    return BGFX_INVALID_HANDLE;
}

[[nodiscard]] bgfx::TextureHandle createMinecraftStyleCrosshairTexture()
{
    constexpr int kCrosshairSize = 15;
    constexpr int kCenter = kCrosshairSize / 2;
    constexpr int kArmLength = 5;
    constexpr int kPixelCount = kCrosshairSize * kCrosshairSize;

    std::array<bool, kPixelCount> whiteMask{};
    std::array<bool, kPixelCount> blackMask{};

    const auto inBounds = [](const int x, const int y)
    {
        return x >= 0 && x < kCrosshairSize && y >= 0 && y < kCrosshairSize;
    };
    const auto indexFor = [](const int x, const int y)
    {
        return y * kCrosshairSize + x;
    };

    for (int d = -kArmLength; d <= kArmLength; ++d)
    {
        if (d == 0)
        {
            continue;
        }

        const int x = kCenter + d;
        const int y = kCenter;
        if (inBounds(x, y))
        {
            whiteMask[static_cast<std::size_t>(indexFor(x, y))] = true;
        }
    }

    for (int d = -kArmLength; d <= kArmLength; ++d)
    {
        if (d == 0)
        {
            continue;
        }

        const int x = kCenter;
        const int y = kCenter + d;
        if (inBounds(x, y))
        {
            whiteMask[static_cast<std::size_t>(indexFor(x, y))] = true;
        }
    }

    for (int y = 0; y < kCrosshairSize; ++y)
    {
        for (int x = 0; x < kCrosshairSize; ++x)
        {
            const std::size_t sourceIndex = static_cast<std::size_t>(indexFor(x, y));
            if (!whiteMask[sourceIndex])
            {
                continue;
            }

            for (int offsetY = -1; offsetY <= 1; ++offsetY)
            {
                for (int offsetX = -1; offsetX <= 1; ++offsetX)
                {
                    const int outlineX = x + offsetX;
                    const int outlineY = y + offsetY;
                    if (!inBounds(outlineX, outlineY))
                    {
                        continue;
                    }

                    const std::size_t outlineIndex = static_cast<std::size_t>(indexFor(outlineX, outlineY));
                    if (!whiteMask[outlineIndex])
                    {
                        blackMask[outlineIndex] = true;
                    }
                }
            }
        }
    }

    std::array<std::uint8_t, static_cast<std::size_t>(kPixelCount * 4)> pixels{};
    for (int y = 0; y < kCrosshairSize; ++y)
    {
        for (int x = 0; x < kCrosshairSize; ++x)
        {
            const std::size_t maskIndex = static_cast<std::size_t>(indexFor(x, y));
            const std::size_t pixelOffset = static_cast<std::size_t>((y * kCrosshairSize + x) * 4);

            if (whiteMask[maskIndex])
            {
                pixels[pixelOffset + 0] = 0xff;  // B
                pixels[pixelOffset + 1] = 0xff;  // G
                pixels[pixelOffset + 2] = 0xff;  // R
                pixels[pixelOffset + 3] = 0xff;  // A
                continue;
            }

            if (blackMask[maskIndex])
            {
                pixels[pixelOffset + 0] = 0x00;  // B
                pixels[pixelOffset + 1] = 0x00;  // G
                pixels[pixelOffset + 2] = 0x00;  // R
                pixels[pixelOffset + 3] = 0xff;  // A
            }
        }
    }

    const bgfx::Memory* const crosshairMemory =
        bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size()));
    return bgfx::createTexture2D(
        static_cast<std::uint16_t>(kCrosshairSize),
        static_cast<std::uint16_t>(kCrosshairSize),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        crosshairMemory);

} // namespace vibecraft::render::detail
