#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <SDL3/SDL_filesystem.h>
#include <bgfx/bgfx.h>
#include <bx/allocator.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vibecraft/ChunkAtlasLayout.hpp"

namespace vibecraft::render::detail
{
namespace
{
struct RgbaColor
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

[[nodiscard]] constexpr RgbaColor lerpColor(const RgbaColor a, const RgbaColor b, const float t)
{
    const auto mix = [t](const std::uint8_t lhs, const std::uint8_t rhs)
    {
        return static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(static_cast<float>(lhs) + (static_cast<float>(rhs) - static_cast<float>(lhs)) * t)),
            0,
            255));
    };
    return RgbaColor{
        .r = mix(a.r, b.r),
        .g = mix(a.g, b.g),
        .b = mix(a.b, b.b),
        .a = mix(a.a, b.a),
    };
}

template <std::size_t N>
void fillTexturePixels(
    std::array<std::uint8_t, N>& pixels,
    const int width,
    const int x,
    const int y,
    const RgbaColor color)
{
    const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
    pixels[offset + 0] = color.b;
    pixels[offset + 1] = color.g;
    pixels[offset + 2] = color.r;
    pixels[offset + 3] = color.a;
}

void stripWhiteEdgeMatteAlpha(
    std::uint8_t* const rgbaPixels,
    const int width,
    const int height)
{
    if (rgbaPixels == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    constexpr std::uint8_t kWhiteThreshold = 245;
    constexpr std::uint8_t kMinAlpha = 1;
    const auto isEdgeMattePixel = [&](const int x, const int y)
    {
        const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x))
            * 4U;
        const std::uint8_t r = rgbaPixels[idx + 0];
        const std::uint8_t g = rgbaPixels[idx + 1];
        const std::uint8_t b = rgbaPixels[idx + 2];
        const std::uint8_t a = rgbaPixels[idx + 3];
        return a >= kMinAlpha && r >= kWhiteThreshold && g >= kWhiteThreshold && b >= kWhiteThreshold;
    };

    const auto pixelIndex = [width](const int x, const int y)
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    };

    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> visited(count, 0);
    std::vector<std::size_t> stack;
    stack.reserve(count / 8U + 16U);

    const auto pushIfMatte = [&](const int x, const int y)
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return;
        }
        const std::size_t p = pixelIndex(x, y);
        if (visited[p] != 0 || !isEdgeMattePixel(x, y))
        {
            return;
        }
        stack.push_back(p);
    };

    for (int x = 0; x < width; ++x)
    {
        pushIfMatte(x, 0);
        pushIfMatte(x, height - 1);
    }
    for (int y = 1; y < height - 1; ++y)
    {
        pushIfMatte(0, y);
        pushIfMatte(width - 1, y);
    }

    while (!stack.empty())
    {
        const std::size_t p = stack.back();
        stack.pop_back();
        if (visited[p] != 0)
        {
            continue;
        }
        visited[p] = 1;

        const int x = static_cast<int>(p % static_cast<std::size_t>(width));
        const int y = static_cast<int>(p / static_cast<std::size_t>(width));
        if (!isEdgeMattePixel(x, y))
        {
            continue;
        }

        const std::size_t idx = p * 4U;
        rgbaPixels[idx + 3] = 0;

        pushIfMatte(x - 1, y);
        pushIfMatte(x + 1, y);
        pushIfMatte(x, y - 1);
        pushIfMatte(x, y + 1);
    }
}
}  // namespace


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

[[nodiscard]] bgfx::TextureHandle createTextureFromPng(
    const std::filesystem::path& relativePath,
    const std::uint16_t textureFlags,
    std::uint16_t* const outWidthPx,
    std::uint16_t* const outHeightPx,
    const bool stripWhiteEdgeMatte)
{
    if (outWidthPx != nullptr)
    {
        *outWidthPx = 0;
    }
    if (outHeightPx != nullptr)
    {
        *outHeightPx = 0;
    }

    const std::filesystem::path texturePath = runtimeAssetPath(relativePath);
    if (!std::filesystem::exists(texturePath))
    {
        return BGFX_INVALID_HANDLE;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* const rgbaPixels = stbi_load(texturePath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (rgbaPixels == nullptr || width <= 0 || height <= 0)
    {
        if (rgbaPixels != nullptr)
        {
            stbi_image_free(rgbaPixels);
        }
        return BGFX_INVALID_HANDLE;
    }

    if (stripWhiteEdgeMatte)
    {
        stripWhiteEdgeMatteAlpha(rgbaPixels, width, height);
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> bgraPixels(pixelCount * 4U);
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const std::size_t src = i * 4U;
        const std::size_t dst = src;
        bgraPixels[dst + 0] = rgbaPixels[src + 2];
        bgraPixels[dst + 1] = rgbaPixels[src + 1];
        bgraPixels[dst + 2] = rgbaPixels[src + 0];
        bgraPixels[dst + 3] = rgbaPixels[src + 3];
    }
    stbi_image_free(rgbaPixels);

    const bgfx::Memory* const textureMemory =
        bgfx::copy(bgraPixels.data(), static_cast<std::uint32_t>(bgraPixels.size()));
    const bgfx::TextureHandle texture = bgfx::createTexture2D(
        static_cast<std::uint16_t>(width),
        static_cast<std::uint16_t>(height),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        textureFlags,
        textureMemory);
    if (bgfx::isValid(texture))
    {
        if (outWidthPx != nullptr)
        {
            *outWidthPx = static_cast<std::uint16_t>(std::min(width, 65535));
        }
        if (outHeightPx != nullptr)
        {
            *outHeightPx = static_cast<std::uint16_t>(std::min(height, 65535));
        }
    }
    return texture;
}

[[nodiscard]] bgfx::TextureHandle createLogoTextureFromPng(
    bx::AllocatorI& allocator,
    std::uint16_t& outWidth,
    std::uint16_t& outHeight)
{
    static_cast<void>(allocator);
    return createTextureFromPng(
        std::filesystem::path("textures/ui/vibecraft_logo.png"),
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        &outWidth,
        &outHeight,
        true);
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
}

[[nodiscard]] bgfx::TextureHandle createBlockBreakOverlayTexture(const int stage)
{
    constexpr int kSize = 32;
    constexpr int kPixelCount = kSize * kSize;
    const int clampedStage = std::clamp(stage, 0, 9);
    std::array<std::uint8_t, static_cast<std::size_t>(kPixelCount * 4)> pixels{};

    const auto indexFor = [](const int x, const int y)
    {
        return y * kSize + x;
    };
    const auto putPixel = [&pixels, &indexFor](const int x, const int y, const std::uint8_t alpha)
    {
        if (x < 0 || y < 0 || x >= kSize || y >= kSize)
        {
            return;
        }
        const std::size_t offset = static_cast<std::size_t>(indexFor(x, y)) * 4U;
        const std::uint8_t brightness = 230;
        pixels[offset + 0] = brightness;
        pixels[offset + 1] = brightness;
        pixels[offset + 2] = brightness;
        pixels[offset + 3] = std::max(pixels[offset + 3], alpha);
    };
    const auto drawLine = [&putPixel](int x0, int y0, int x1, int y1, const std::uint8_t alpha)
    {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
            putPixel(x0, y0, alpha);
            putPixel(x0 + 1, y0, static_cast<std::uint8_t>(alpha * 0.65f));
            putPixel(x0 - 1, y0, static_cast<std::uint8_t>(alpha * 0.65f));
            putPixel(x0, y0 + 1, static_cast<std::uint8_t>(alpha * 0.65f));
            putPixel(x0, y0 - 1, static_cast<std::uint8_t>(alpha * 0.65f));
            if (x0 == x1 && y0 == y1)
            {
                break;
            }
            const int e2 = err * 2;
            if (e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    };

    constexpr std::array<std::array<int, 4>, 14> kCrackSegments = {{
        {{16, 2, 16, 29}},
        {{16, 8, 7, 15}},
        {{16, 10, 24, 17}},
        {{16, 16, 5, 23}},
        {{16, 18, 27, 26}},
        {{16, 22, 10, 30}},
        {{16, 14, 21, 5}},
        {{10, 15, 8, 8}},
        {{23, 17, 26, 9}},
        {{11, 24, 4, 27}},
        {{22, 25, 28, 30}},
        {{20, 7, 27, 4}},
        {{14, 28, 15, 31}},
        {{4, 14, 1, 10}},
    }};

    const int segmentCount = 3 + clampedStage;
    for (int i = 0; i < segmentCount && i < static_cast<int>(kCrackSegments.size()); ++i)
    {
        const auto& seg = kCrackSegments[static_cast<std::size_t>(i)];
        const std::uint8_t alpha = static_cast<std::uint8_t>(70 + clampedStage * 16 + (i % 3) * 8);
        drawLine(seg[0], seg[1], seg[2], seg[3], alpha);
    }

    for (int y = 0; y < kSize; ++y)
    {
        for (int x = 0; x < kSize; ++x)
        {
            const std::size_t offset = static_cast<std::size_t>(indexFor(x, y)) * 4U;
            if (pixels[offset + 3] == 0)
            {
                continue;
            }
            const bool checker = ((x + y) & 1) == 0;
            if (checker)
            {
                pixels[offset + 0] = static_cast<std::uint8_t>(pixels[offset + 0] * 0.87f);
                pixels[offset + 1] = static_cast<std::uint8_t>(pixels[offset + 1] * 0.87f);
                pixels[offset + 2] = static_cast<std::uint8_t>(pixels[offset + 2] * 0.87f);
            }
        }
    }

    const bgfx::Memory* const memory = bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size()));
    return bgfx::createTexture2D(
        static_cast<std::uint16_t>(kSize),
        static_cast<std::uint16_t>(kSize),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        memory);
}

[[nodiscard]] bgfx::TextureHandle createHeartTexture(const int fillStage)
{
    constexpr int kSize = 16;
    std::array<std::uint8_t, static_cast<std::size_t>(kSize * kSize * 4)> pixels{};
    constexpr std::array<const char*, 16> kMask = {
        "................",
        "...XX....XX.....",
        "..XXXX..XXXX....",
        ".XXXXXXXXXXXX...",
        ".XXXXXXXXXXXX...",
        ".XXXXXXXXXXXX...",
        "..XXXXXXXXXX....",
        "...XXXXXXXX.....",
        "....XXXXXX......",
        ".....XXXX.......",
        "......XX........",
        "................",
        "................",
        "................",
        "................",
        "................",
    };

    const int clampedFill = std::clamp(fillStage, 0, 2);
    const int fillLimitX = clampedFill == 2 ? kSize : (clampedFill == 1 ? 8 : 0);
    for (int y = 0; y < kSize; ++y)
    {
        for (int x = 0; x < kSize; ++x)
        {
            if (kMask[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] != 'X')
            {
                continue;
            }

            std::uint8_t r = 54;
            std::uint8_t g = 54;
            std::uint8_t b = 54;
            std::uint8_t a = 220;
            if (x < fillLimitX)
            {
                r = 220;
                g = 40;
                b = 48;
                a = 255;
            }
            const bool highlight = y <= 3 || (y <= 6 && x > 2 && x < 13);
            if (highlight && x < fillLimitX)
            {
                r = static_cast<std::uint8_t>(std::min(255, static_cast<int>(r) + 28));
                g = static_cast<std::uint8_t>(std::min(255, static_cast<int>(g) + 18));
                b = static_cast<std::uint8_t>(std::min(255, static_cast<int>(b) + 18));
            }

            const std::size_t offset = static_cast<std::size_t>((y * kSize + x) * 4);
            pixels[offset + 0] = b;
            pixels[offset + 1] = g;
            pixels[offset + 2] = r;
            pixels[offset + 3] = a;
        }
    }

    const bgfx::Memory* const memory = bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size()));
    return bgfx::createTexture2D(
        static_cast<std::uint16_t>(kSize),
        static_cast<std::uint16_t>(kSize),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        memory);
}

[[nodiscard]] bgfx::TextureHandle createProceduralMobTexture(const vibecraft::game::MobKind mobKind)
{
    constexpr int kWidth = 64;
    constexpr int kHeight = 32;
    std::array<std::uint8_t, static_cast<std::size_t>(kWidth * kHeight * 4)> pixels{};

    RgbaColor top{};
    RgbaColor bottom{};
    RgbaColor accent{};
    int stripePeriod = 6;
    int seed = 0;

    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::VoidStrider:
        top = {.r = 68, .g = 34, .b = 110, .a = 255};
        bottom = {.r = 15, .g = 12, .b = 34, .a = 255};
        accent = {.r = 81, .g = 228, .b = 206, .a = 255};
        stripePeriod = 7;
        seed = 11;
        break;
    case MK::Player:
        top = {.r = 98, .g = 98, .b = 98, .a = 255};
        bottom = {.r = 54, .g = 54, .b = 54, .a = 255};
        accent = {.r = 215, .g = 215, .b = 215, .a = 255};
        stripePeriod = 8;
        seed = 3;
        break;
    case MK::Sporegrazer:
        top = {.r = 118, .g = 148, .b = 62, .a = 255};
        bottom = {.r = 43, .g = 72, .b = 32, .a = 255};
        accent = {.r = 198, .g = 236, .b = 112, .a = 255};
        stripePeriod = 9;
        seed = 19;
        break;
    case MK::Burrower:
        top = {.r = 147, .g = 91, .b = 46, .a = 255};
        bottom = {.r = 62, .g = 34, .b = 20, .a = 255};
        accent = {.r = 229, .g = 155, .b = 78, .a = 255};
        stripePeriod = 5;
        seed = 27;
        break;
    case MK::Shardback:
        top = {.r = 148, .g = 166, .b = 182, .a = 255};
        bottom = {.r = 69, .g = 82, .b = 102, .a = 255};
        accent = {.r = 102, .g = 255, .b = 250, .a = 255};
        stripePeriod = 10;
        seed = 7;
        break;
    case MK::Skitterwing:
        top = {.r = 66, .g = 82, .b = 148, .a = 255};
        bottom = {.r = 18, .g = 21, .b = 54, .a = 255};
        accent = {.r = 244, .g = 188, .b = 84, .a = 255};
        stripePeriod = 4;
        seed = 31;
        break;
    }

    for (int y = 0; y < kHeight; ++y)
    {
        const float v = static_cast<float>(y) / static_cast<float>(kHeight - 1);
        const RgbaColor gradient = lerpColor(top, bottom, v);
        for (int x = 0; x < kWidth; ++x)
        {
            RgbaColor color = gradient;
            const bool stripe = ((x + seed) / stripePeriod + (y + seed) / std::max(2, stripePeriod - 2)) % 2 == 0;
            const bool speckle = ((x * 13 + y * 7 + seed * 17) % 29) < 4;
            const bool glowBand = ((x + seed * 2) % 17) < 2 && y > 3 && y < kHeight - 3;
            if (stripe)
            {
                color = lerpColor(color, accent, 0.18f);
            }
            if (speckle)
            {
                color = lerpColor(color, accent, 0.36f);
            }
            if (glowBand)
            {
                color = lerpColor(color, accent, 0.55f);
            }
            fillTexturePixels(pixels, kWidth, x, y, color);
        }
    }

    const bgfx::Memory* const memory = bgfx::copy(pixels.data(), static_cast<std::uint32_t>(pixels.size()));
    return bgfx::createTexture2D(
        static_cast<std::uint16_t>(kWidth),
        static_cast<std::uint16_t>(kHeight),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        memory);
}

} // namespace vibecraft::render::detail
