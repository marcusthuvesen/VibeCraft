#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/allocator.h>
#include <bx/math.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_set>
#include <vector>

#include "stb_image.h"
#include "debugdraw.h"

namespace vibecraft::render
{
namespace
{
[[nodiscard]] TextureUvRect computePrimaryOpaqueUvRect(const std::filesystem::path& texturePath)
{
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
        return {};
    }

    std::vector<std::uint8_t> visited(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0U);
    auto pixelIndex = [width](const int x, const int y)
    {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
    };
    auto alphaAt = [rgbaPixels, width](const int x, const int y)
    {
        const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x))
            * 4U;
        return rgbaPixels[idx + 3U];
    };

    int bestCount = 0;
    int bestMinX = 0;
    int bestMinY = 0;
    int bestMaxX = width - 1;
    int bestMaxY = height - 1;

    std::queue<std::pair<int, int>> floodQueue;
    constexpr int kDirs[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    };

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const std::size_t startIdx = pixelIndex(x, y);
            if (visited[startIdx] != 0U || alphaAt(x, y) == 0U)
            {
                continue;
            }

            visited[startIdx] = 1U;
            floodQueue.push({x, y});
            int count = 0;
            int minX = x;
            int minY = y;
            int maxX = x;
            int maxY = y;

            while (!floodQueue.empty())
            {
                const auto [cx, cy] = floodQueue.front();
                floodQueue.pop();
                ++count;
                minX = std::min(minX, cx);
                minY = std::min(minY, cy);
                maxX = std::max(maxX, cx);
                maxY = std::max(maxY, cy);

                for (const auto& dir : kDirs)
                {
                    const int nx = cx + dir[0];
                    const int ny = cy + dir[1];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                    {
                        continue;
                    }
                    const std::size_t nIdx = pixelIndex(nx, ny);
                    if (visited[nIdx] != 0U || alphaAt(nx, ny) == 0U)
                    {
                        continue;
                    }
                    visited[nIdx] = 1U;
                    floodQueue.push({nx, ny});
                }
            }

            if (count > bestCount)
            {
                bestCount = count;
                bestMinX = minX;
                bestMinY = minY;
                bestMaxX = maxX;
                bestMaxY = maxY;
            }
        }
    }

    stbi_image_free(rgbaPixels);
    if (bestCount <= 0)
    {
        return {};
    }

    const float invW = 1.0f / static_cast<float>(width);
    const float invH = 1.0f / static_cast<float>(height);
    const float texelInsetU = 0.5f * invW;
    const float texelInsetV = 0.5f * invH;
    TextureUvRect uv{
        .minU = std::clamp(static_cast<float>(bestMinX) * invW + texelInsetU, 0.0f, 1.0f),
        .maxU = std::clamp(static_cast<float>(bestMaxX + 1) * invW - texelInsetU, 0.0f, 1.0f),
        .minV = std::clamp(static_cast<float>(bestMinY) * invH + texelInsetV, 0.0f, 1.0f),
        .maxV = std::clamp(static_cast<float>(bestMaxY + 1) * invH - texelInsetV, 0.0f, 1.0f),
    };
    if (uv.maxU <= uv.minU || uv.maxV <= uv.minV)
    {
        return {};
    }
    return uv;
}
}  // namespace


bool Renderer::initialize(void* const nativeWindowHandle, const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    blockBreakStageTextureHandles_.fill(UINT16_MAX);

    bgfx::PlatformData platformData{};
    platformData.nwh = nativeWindowHandle;
    bgfx::setPlatformData(platformData);

    bgfx::Init init;
    init.type = bgfx::RendererType::Count;
    init.platformData = platformData;
    init.resolution.width = width_;
    init.resolution.height = height_;
    init.resolution.reset = detail::kDefaultResetFlags;

    if (!bgfx::init(init))
    {
        return false;
    }

    initialized_ = true;
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(detail::kMainView, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x263238ff, 1.0f, 0);
    bgfx::setViewClear(detail::kUiView, BGFX_CLEAR_NONE, 0, 1.0f, 0);
    // UI overlays rely on painter's-order submission so slot fills do not cover item icons.
    bgfx::setViewMode(detail::kUiView, bgfx::ViewMode::Sequential);
    ddInit();

    const bgfx::ProgramHandle chunkProgram = detail::loadProgram("vs_chunk", "fs_chunk");
    if (!bgfx::isValid(chunkProgram))
    {
        ddShutdown();
        bgfx::shutdown();
        initialized_ = false;
        return false;
    }

    const bgfx::TextureHandle chunkAtlasTexture = detail::createChunkAtlasTexture();
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

    const bgfx::TextureHandle crosshairTexture = detail::createMinecraftStyleCrosshairTexture();
    if (bgfx::isValid(crosshairTexture))
    {
        crosshairTextureHandle_ = crosshairTexture.idx;
        const bgfx::UniformHandle crosshairSampler = bgfx::createUniform("s_logo", bgfx::UniformType::Sampler);
        if (bgfx::isValid(crosshairSampler))
        {
            crosshairSamplerHandle_ = crosshairSampler.idx;
            const bgfx::ProgramHandle crosshairProgram = detail::loadProgram("vs_chunk", "fs_logo");
            if (bgfx::isValid(crosshairProgram))
            {
                crosshairProgramHandle_ = crosshairProgram.idx;
            }
            else
            {
                bgfx::destroy(crosshairSampler);
                bgfx::destroy(crosshairTexture);
                crosshairSamplerHandle_ = UINT16_MAX;
                crosshairTextureHandle_ = UINT16_MAX;
            }
        }
        else
        {
            bgfx::destroy(crosshairTexture);
            crosshairTextureHandle_ = UINT16_MAX;
        }
    }

    constexpr std::uint16_t kUiItemTextureFlags =
        BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    const bgfx::TextureHandle diamondSwordTexture = detail::createTextureFromPng(
        "textures/item/diamond_sword.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(diamondSwordTexture))
    {
        diamondSwordTextureHandle_ = diamondSwordTexture.idx;
    }
    const bgfx::TextureHandle stickTexture = detail::createTextureFromPng(
        "textures/item/stick.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(stickTexture))
    {
        stickTextureHandle_ = stickTexture.idx;
    }
    const bgfx::TextureHandle rottenFleshTexture =
        detail::createTextureFromPng(
            "textures/item/mob_drops/rotten_flesh.png",
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
    if (bgfx::isValid(rottenFleshTexture))
    {
        rottenFleshTextureHandle_ = rottenFleshTexture.idx;
    }
    const bgfx::TextureHandle leatherTexture =
        detail::createTextureFromPng(
            "textures/item/mob_drops/leather.png",
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
    if (bgfx::isValid(leatherTexture))
    {
        leatherTextureHandle_ = leatherTexture.idx;
    }
    const bgfx::TextureHandle rawPorkchopTexture =
        detail::createTextureFromPng(
            "textures/item/mob_drops/porkchop.png",
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
    if (bgfx::isValid(rawPorkchopTexture))
    {
        rawPorkchopTextureHandle_ = rawPorkchopTexture.idx;
    }
    const bgfx::TextureHandle muttonTexture =
        detail::createTextureFromPng(
            "textures/item/mob_drops/mutton.png",
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
    if (bgfx::isValid(muttonTexture))
    {
        muttonTextureHandle_ = muttonTexture.idx;
    }
    const bgfx::TextureHandle featherTexture =
        detail::createTextureFromPng(
            "textures/item/mob_drops/feather.png",
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
    if (bgfx::isValid(featherTexture))
    {
        featherTextureHandle_ = featherTexture.idx;
    }
    const bgfx::TextureHandle coalTexture = detail::createTextureFromPng(
        "textures/item/coal.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(coalTexture))
    {
        coalTextureHandle_ = coalTexture.idx;
    }
    const bgfx::TextureHandle oxygenCanisterTexture = detail::createTextureFromPng(
        "textures/item/oxygen_canister.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(oxygenCanisterTexture))
    {
        oxygenCanisterTextureHandle_ = oxygenCanisterTexture.idx;
        oxygenCanisterTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath("textures/item/oxygen_canister.png"));
    }
    const bgfx::TextureHandle starterTankTexture = detail::createTextureFromPng(
        "textures/item/starter_tank.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(starterTankTexture))
    {
        starterTankTextureHandle_ = starterTankTexture.idx;
        starterTankTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath("textures/item/starter_tank.png"));
    }
    const bgfx::TextureHandle fieldTankTexture = detail::createTextureFromPng(
        "textures/item/field_tank.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(fieldTankTexture))
    {
        fieldTankTextureHandle_ = fieldTankTexture.idx;
        fieldTankTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath("textures/item/field_tank.png"));
    }
    const bgfx::TextureHandle expeditionTankTexture = detail::createTextureFromPng(
        "textures/item/expedition_tank.png",
        kUiItemTextureFlags,
        nullptr,
        nullptr,
        true);
    if (bgfx::isValid(expeditionTankTexture))
    {
        expeditionTankTextureHandle_ = expeditionTankTexture.idx;
        expeditionTankTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath("textures/item/expedition_tank.png"));
    }

    extendedToolTextureHandles_.fill(UINT16_MAX);
    static constexpr const char* const kExtendedToolTexturePaths[14] = {
        "textures/item/wooden_sword.png",
        "textures/item/stone_sword.png",
        "textures/item/iron_sword.png",
        "textures/item/golden_sword.png",
        "textures/item/wooden_pickaxe.png",
        "textures/item/stone_pickaxe.png",
        "textures/item/iron_pickaxe.png",
        "textures/item/golden_pickaxe.png",
        "textures/item/diamond_pickaxe.png",
        "textures/item/wooden_axe.png",
        "textures/item/stone_axe.png",
        "textures/item/iron_axe.png",
        "textures/item/golden_axe.png",
        "textures/item/diamond_axe.png",
    };
    for (std::size_t i = 0; i < extendedToolTextureHandles_.size(); ++i)
    {
        const bgfx::TextureHandle toolTexture = detail::createTextureFromPng(
            kExtendedToolTexturePaths[i],
            kUiItemTextureFlags,
            nullptr,
            nullptr,
            true);
        if (bgfx::isValid(toolTexture))
        {
            extendedToolTextureHandles_[i] = toolTexture.idx;
        }
    }

    const bgfx::TextureHandle fullHeartTexture = detail::createHeartTexture(2);
    if (bgfx::isValid(fullHeartTexture))
    {
        fullHeartTextureHandle_ = fullHeartTexture.idx;
    }
    const bgfx::TextureHandle halfHeartTexture = detail::createHeartTexture(1);
    if (bgfx::isValid(halfHeartTexture))
    {
        halfHeartTextureHandle_ = halfHeartTexture.idx;
    }
    const bgfx::TextureHandle emptyHeartTexture = detail::createHeartTexture(0);
    if (bgfx::isValid(emptyHeartTexture))
    {
        emptyHeartTextureHandle_ = emptyHeartTexture.idx;
    }

    for (int stage = 0; stage < static_cast<int>(blockBreakStageTextureHandles_.size()); ++stage)
    {
        const bgfx::TextureHandle blockBreakTexture = detail::createBlockBreakOverlayTexture(stage);
        if (bgfx::isValid(blockBreakTexture))
        {
            blockBreakStageTextureHandles_[static_cast<std::size_t>(stage)] = blockBreakTexture.idx;
        }
    }

    const bgfx::TextureHandle voidStriderTexture = detail::createProceduralMobTexture(vibecraft::game::MobKind::VoidStrider);
    if (bgfx::isValid(voidStriderTexture))
    {
        voidStriderTextureHandle_ = voidStriderTexture.idx;
        voidStriderTextureUv_ = TextureUvRect{};
    }
    const std::filesystem::path playerPath = "textures/entity/player_mars.png";
    const bgfx::TextureHandle playerMobTexture = detail::createTextureFromPng(playerPath);
    if (bgfx::isValid(playerMobTexture))
    {
        playerMobTextureHandle_ = playerMobTexture.idx;
        playerMobTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath(playerPath));
    }
    const bgfx::TextureHandle sporegrazerTexture = detail::createProceduralMobTexture(vibecraft::game::MobKind::Sporegrazer);
    if (bgfx::isValid(sporegrazerTexture))
    {
        sporegrazerTextureHandle_ = sporegrazerTexture.idx;
        sporegrazerTextureUv_ = TextureUvRect{};
    }
    const bgfx::TextureHandle burrowerTexture = detail::createProceduralMobTexture(vibecraft::game::MobKind::Burrower);
    if (bgfx::isValid(burrowerTexture))
    {
        burrowerTextureHandle_ = burrowerTexture.idx;
        burrowerTextureUv_ = TextureUvRect{};
    }
    const bgfx::TextureHandle shardbackTexture = detail::createProceduralMobTexture(vibecraft::game::MobKind::Shardback);
    if (bgfx::isValid(shardbackTexture))
    {
        shardbackTextureHandle_ = shardbackTexture.idx;
        shardbackTextureUv_ = TextureUvRect{};
    }
    const bgfx::TextureHandle skitterwingTexture = detail::createProceduralMobTexture(vibecraft::game::MobKind::Skitterwing);
    if (bgfx::isValid(skitterwingTexture))
    {
        skitterwingTextureHandle_ = skitterwingTexture.idx;
        skitterwingTextureUv_ = TextureUvRect{};
    }
    const std::filesystem::path ambientBirdPath = "textures/entity/alien_bird.png";
    const bgfx::TextureHandle ambientBirdTexture = detail::createTextureFromPng(ambientBirdPath);
    if (bgfx::isValid(ambientBirdTexture))
    {
        ambientBirdTextureHandle_ = ambientBirdTexture.idx;
        ambientBirdTextureUv_ = computePrimaryOpaqueUvRect(detail::runtimeAssetPath(ambientBirdPath));
    }

    const bgfx::UniformHandle inventoryUiSampler = bgfx::createUniform("s_uiAtlas", bgfx::UniformType::Sampler);
    if (bgfx::isValid(inventoryUiSampler))
    {
        inventoryUiSamplerHandle_ = inventoryUiSampler.idx;
        const bgfx::ProgramHandle inventoryUiProgram = detail::loadProgram("vs_chunk", "fs_ui");
        if (bgfx::isValid(inventoryUiProgram))
        {
            inventoryUiProgramHandle_ = inventoryUiProgram.idx;
            const bgfx::ProgramHandle inventoryUiSolidProgram = detail::loadProgram("vs_chunk", "fs_ui_solid");
            if (bgfx::isValid(inventoryUiSolidProgram))
            {
                inventoryUiSolidProgramHandle_ = inventoryUiSolidProgram.idx;
            }
        }
        else
        {
            bgfx::destroy(inventoryUiSampler);
            inventoryUiSamplerHandle_ = UINT16_MAX;
        }
    }

    if (inventoryUiProgramHandle_ != UINT16_MAX)
    {
        std::uint16_t menuBgW = 0;
        std::uint16_t menuBgH = 0;
        const bgfx::TextureHandle menuBgTexture = detail::createTextureFromPng(
            "textures/ui/main_menu_background.png",
            BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            &menuBgW,
            &menuBgH);
        if (bgfx::isValid(menuBgTexture))
        {
            mainMenuBackgroundTextureHandle_ = menuBgTexture.idx;
            mainMenuBackgroundWidthPx_ = menuBgW;
            mainMenuBackgroundHeightPx_ = menuBgH;
        }
    }

    bx::DefaultAllocator logoAllocator;
    const bgfx::TextureHandle logoTexture =
        detail::createLogoTextureFromPng(logoAllocator, logoWidthPx_, logoHeightPx_);
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
            const bgfx::ProgramHandle logoProgram = detail::loadProgram("vs_chunk", "fs_logo");
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
        bgfx::destroy(detail::toProgramHandle(logoProgramHandle_));
        logoProgramHandle_ = UINT16_MAX;
    }
    if (logoTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(logoTextureHandle_));
        logoTextureHandle_ = UINT16_MAX;
    }
    if (logoSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(logoSamplerHandle_));
        logoSamplerHandle_ = UINT16_MAX;
    }
    if (crosshairProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toProgramHandle(crosshairProgramHandle_));
        crosshairProgramHandle_ = UINT16_MAX;
    }
    if (crosshairTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(crosshairTextureHandle_));
        crosshairTextureHandle_ = UINT16_MAX;
    }
    if (crosshairSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(crosshairSamplerHandle_));
        crosshairSamplerHandle_ = UINT16_MAX;
    }
    if (diamondSwordTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(diamondSwordTextureHandle_));
        diamondSwordTextureHandle_ = UINT16_MAX;
    }
    if (stickTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(stickTextureHandle_));
        stickTextureHandle_ = UINT16_MAX;
    }
    if (rottenFleshTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(rottenFleshTextureHandle_));
        rottenFleshTextureHandle_ = UINT16_MAX;
    }
    if (leatherTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(leatherTextureHandle_));
        leatherTextureHandle_ = UINT16_MAX;
    }
    if (rawPorkchopTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(rawPorkchopTextureHandle_));
        rawPorkchopTextureHandle_ = UINT16_MAX;
    }
    if (muttonTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(muttonTextureHandle_));
        muttonTextureHandle_ = UINT16_MAX;
    }
    if (featherTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(featherTextureHandle_));
        featherTextureHandle_ = UINT16_MAX;
    }
    if (coalTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(coalTextureHandle_));
        coalTextureHandle_ = UINT16_MAX;
    }
    if (oxygenCanisterTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(oxygenCanisterTextureHandle_));
        oxygenCanisterTextureHandle_ = UINT16_MAX;
    }
    if (starterTankTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(starterTankTextureHandle_));
        starterTankTextureHandle_ = UINT16_MAX;
    }
    if (fieldTankTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(fieldTankTextureHandle_));
        fieldTankTextureHandle_ = UINT16_MAX;
    }
    if (expeditionTankTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(expeditionTankTextureHandle_));
        expeditionTankTextureHandle_ = UINT16_MAX;
    }
    {
        std::unordered_set<std::uint16_t> destroyedExtended;
        for (const std::uint16_t handle : extendedToolTextureHandles_)
        {
            if (handle == UINT16_MAX || !destroyedExtended.insert(handle).second)
            {
                continue;
            }
            bgfx::destroy(detail::toTextureHandle(handle));
        }
        extendedToolTextureHandles_.fill(UINT16_MAX);
    }
    if (fullHeartTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(fullHeartTextureHandle_));
        fullHeartTextureHandle_ = UINT16_MAX;
    }
    if (halfHeartTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(halfHeartTextureHandle_));
        halfHeartTextureHandle_ = UINT16_MAX;
    }
    if (emptyHeartTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(emptyHeartTextureHandle_));
        emptyHeartTextureHandle_ = UINT16_MAX;
    }
    for (std::uint16_t& textureHandle : blockBreakStageTextureHandles_)
    {
        if (textureHandle != UINT16_MAX)
        {
            bgfx::destroy(detail::toTextureHandle(textureHandle));
            textureHandle = UINT16_MAX;
        }
    }
    if (inventoryUiSolidProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toProgramHandle(inventoryUiSolidProgramHandle_));
        inventoryUiSolidProgramHandle_ = UINT16_MAX;
    }
    if (inventoryUiProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toProgramHandle(inventoryUiProgramHandle_));
        inventoryUiProgramHandle_ = UINT16_MAX;
    }
    if (inventoryUiSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(inventoryUiSamplerHandle_));
        inventoryUiSamplerHandle_ = UINT16_MAX;
    }
    if (voidStriderTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(voidStriderTextureHandle_));
        voidStriderTextureHandle_ = UINT16_MAX;
    }
    if (playerMobTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(playerMobTextureHandle_));
        playerMobTextureHandle_ = UINT16_MAX;
    }
    if (sporegrazerTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(sporegrazerTextureHandle_));
        sporegrazerTextureHandle_ = UINT16_MAX;
    }
    if (burrowerTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(burrowerTextureHandle_));
        burrowerTextureHandle_ = UINT16_MAX;
    }
    if (shardbackTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(shardbackTextureHandle_));
        shardbackTextureHandle_ = UINT16_MAX;
    }
    if (skitterwingTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(skitterwingTextureHandle_));
        skitterwingTextureHandle_ = UINT16_MAX;
    }
    if (ambientBirdTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(ambientBirdTextureHandle_));
        ambientBirdTextureHandle_ = UINT16_MAX;
    }
    if (mainMenuBackgroundTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(mainMenuBackgroundTextureHandle_));
        mainMenuBackgroundTextureHandle_ = UINT16_MAX;
    }
    mainMenuBackgroundWidthPx_ = 0;
    mainMenuBackgroundHeightPx_ = 0;
    logoWidthPx_ = 0;
    logoHeightPx_ = 0;
    if (chunkProgramHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toProgramHandle(chunkProgramHandle_));
        chunkProgramHandle_ = UINT16_MAX;
    }
    if (chunkAtlasTextureHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toTextureHandle(chunkAtlasTextureHandle_));
        chunkAtlasTextureHandle_ = UINT16_MAX;
    }
    if (chunkAtlasSamplerHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkAtlasSamplerHandle_));
        chunkAtlasSamplerHandle_ = UINT16_MAX;
    }
    if (chunkSunDirectionUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkSunDirectionUniformHandle_));
        chunkSunDirectionUniformHandle_ = UINT16_MAX;
    }
    if (chunkSunLightColorUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkSunLightColorUniformHandle_));
        chunkSunLightColorUniformHandle_ = UINT16_MAX;
    }
    if (chunkMoonDirectionUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkMoonDirectionUniformHandle_));
        chunkMoonDirectionUniformHandle_ = UINT16_MAX;
    }
    if (chunkMoonLightColorUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkMoonLightColorUniformHandle_));
        chunkMoonLightColorUniformHandle_ = UINT16_MAX;
    }
    if (chunkAmbientLightUniformHandle_ != UINT16_MAX)
    {
        bgfx::destroy(detail::toUniformHandle(chunkAmbientLightUniformHandle_));
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
    bgfx::reset(width_, height_, detail::kDefaultResetFlags);
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

        std::vector<detail::ChunkVertex> chunkVertices;
        chunkVertices.reserve(sceneMesh.vertices.size());

        for (const SceneMeshData::Vertex& vertex : sceneMesh.vertices)
        {
            chunkVertices.push_back(detail::ChunkVertex{
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
            bgfx::copy(chunkVertices.data(), static_cast<std::uint32_t>(chunkVertices.size() * sizeof(detail::ChunkVertex))),
            detail::ChunkVertex::layout());
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

    bgfx::destroy(detail::toVertexBufferHandle(sceneMeshIt->second.vertexBufferHandle));
    bgfx::destroy(detail::toIndexBufferHandle(sceneMeshIt->second.indexBufferHandle));
    sceneMeshes_.erase(sceneMeshIt);
}

std::uint16_t Renderer::mobTextureHandleForKind(const vibecraft::game::MobKind kind) const
{
    using MK = vibecraft::game::MobKind;
    switch (kind)
    {
    case MK::VoidStrider:
        return voidStriderTextureHandle_;
    case MK::Player:
        return playerMobTextureHandle_;
    case MK::Sporegrazer:
        return sporegrazerTextureHandle_;
    case MK::Burrower:
        return burrowerTextureHandle_;
    case MK::Shardback:
        return shardbackTextureHandle_;
    case MK::Skitterwing:
        return skitterwingTextureHandle_;
    }
    return UINT16_MAX;
}

TextureUvRect Renderer::mobTextureUvForKind(const vibecraft::game::MobKind kind) const
{
    using MK = vibecraft::game::MobKind;
    switch (kind)
    {
    case MK::VoidStrider:
        return voidStriderTextureUv_;
    case MK::Player:
        return playerMobTextureUv_;
    case MK::Sporegrazer:
        return sporegrazerTextureUv_;
    case MK::Burrower:
        return burrowerTextureUv_;
    case MK::Shardback:
        return shardbackTextureUv_;
    case MK::Skitterwing:
        return skitterwingTextureUv_;
    }
    return {};
}

std::uint16_t Renderer::hudItemKindTextureHandle(const HudItemKind kind) const
{
    switch (kind)
    {
    case HudItemKind::None:
        return UINT16_MAX;
    case HudItemKind::DiamondSword:
        return diamondSwordTextureHandle_;
    case HudItemKind::Stick:
        return stickTextureHandle_;
    case HudItemKind::RottenFlesh:
        return rottenFleshTextureHandle_;
    case HudItemKind::Leather:
        return leatherTextureHandle_;
    case HudItemKind::RawPorkchop:
        return rawPorkchopTextureHandle_;
    case HudItemKind::Mutton:
        return muttonTextureHandle_;
    case HudItemKind::Feather:
        return featherTextureHandle_;
    case HudItemKind::OxygenCanister:
        return oxygenCanisterTextureHandle_ != UINT16_MAX
            ? oxygenCanisterTextureHandle_
            : (coalTextureHandle_ != UINT16_MAX ? coalTextureHandle_ : stickTextureHandle_);
    case HudItemKind::FieldTank:
        return fieldTankTextureHandle_ != UINT16_MAX
            ? fieldTankTextureHandle_
            : (coalTextureHandle_ != UINT16_MAX ? coalTextureHandle_ : stickTextureHandle_);
    case HudItemKind::StarterTank:
        return starterTankTextureHandle_ != UINT16_MAX
            ? starterTankTextureHandle_
            : (coalTextureHandle_ != UINT16_MAX ? coalTextureHandle_ : stickTextureHandle_);
    case HudItemKind::ScoutHelmet:
    case HudItemKind::ScoutChestRig:
    case HudItemKind::ScoutGreaves:
    case HudItemKind::ScoutBoots:
        return leatherTextureHandle_ != UINT16_MAX ? leatherTextureHandle_ : stickTextureHandle_;
    case HudItemKind::ExpeditionTank:
        return expeditionTankTextureHandle_ != UINT16_MAX
            ? expeditionTankTextureHandle_
            : (coalTextureHandle_ != UINT16_MAX ? coalTextureHandle_ : stickTextureHandle_);
    case HudItemKind::Coal:
        return coalTextureHandle_ != UINT16_MAX ? coalTextureHandle_ : stickTextureHandle_;
    case HudItemKind::WoodSword:
    case HudItemKind::StoneSword:
    case HudItemKind::IronSword:
    case HudItemKind::GoldSword:
    case HudItemKind::WoodPickaxe:
    case HudItemKind::StonePickaxe:
    case HudItemKind::IronPickaxe:
    case HudItemKind::GoldPickaxe:
    case HudItemKind::DiamondPickaxe:
    case HudItemKind::WoodAxe:
    case HudItemKind::StoneAxe:
    case HudItemKind::IronAxe:
    case HudItemKind::GoldAxe:
    case HudItemKind::DiamondAxe:
        break;
    }

    const auto kindByte = static_cast<std::uint8_t>(kind);
    constexpr std::uint8_t kFirstExtended = static_cast<std::uint8_t>(HudItemKind::WoodSword);
    constexpr std::uint8_t kLastExtended = static_cast<std::uint8_t>(HudItemKind::DiamondAxe);
    if (kindByte >= kFirstExtended && kindByte <= kLastExtended)
    {
        const std::size_t index = static_cast<std::size_t>(kindByte - kFirstExtended);
        if (extendedToolTextureHandles_[index] != UINT16_MAX)
        {
            return extendedToolTextureHandles_[index];
        }
        if (kindByte <= static_cast<std::uint8_t>(HudItemKind::GoldSword))
        {
            return diamondSwordTextureHandle_;
        }
        return stickTextureHandle_;
    }

    return UINT16_MAX;
}

TextureUvRect Renderer::hudItemKindTextureUv(const HudItemKind kind) const
{
    switch (kind)
    {
    case HudItemKind::OxygenCanister:
        return oxygenCanisterTextureUv_;
    case HudItemKind::FieldTank:
        return fieldTankTextureUv_;
    case HudItemKind::StarterTank:
        return starterTankTextureUv_;
    case HudItemKind::ExpeditionTank:
        return expeditionTankTextureUv_;
    case HudItemKind::None:
    case HudItemKind::DiamondSword:
    case HudItemKind::Stick:
    case HudItemKind::RottenFlesh:
    case HudItemKind::Leather:
    case HudItemKind::RawPorkchop:
    case HudItemKind::Mutton:
    case HudItemKind::Feather:
    case HudItemKind::WoodSword:
    case HudItemKind::StoneSword:
    case HudItemKind::IronSword:
    case HudItemKind::GoldSword:
    case HudItemKind::WoodPickaxe:
    case HudItemKind::StonePickaxe:
    case HudItemKind::IronPickaxe:
    case HudItemKind::GoldPickaxe:
    case HudItemKind::DiamondPickaxe:
    case HudItemKind::WoodAxe:
    case HudItemKind::StoneAxe:
    case HudItemKind::IronAxe:
    case HudItemKind::GoldAxe:
    case HudItemKind::DiamondAxe:
    case HudItemKind::Coal:
    case HudItemKind::ScoutHelmet:
    case HudItemKind::ScoutChestRig:
    case HudItemKind::ScoutGreaves:
    case HudItemKind::ScoutBoots:
        return {};
    }
    return {};
}

} // namespace vibecraft::render
