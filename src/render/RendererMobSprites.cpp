#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/render/RendererInternal.hpp"

#include <bgfx/bgfx.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "vibecraft/ChunkAtlasLayout.hpp"
#include "vibecraft/world/BlockMetadata.hpp"

namespace vibecraft::render
{
namespace
{
struct CuboidUvSet
{
    TextureUvRect front{};
    TextureUvRect back{};
    TextureUvRect left{};
    TextureUvRect right{};
    TextureUvRect top{};
    TextureUvRect bottom{};
};

[[nodiscard]] TextureUvRect uvRectFromPixels(
    const float textureWidth,
    const float textureHeight,
    const float x,
    const float y,
    const float width,
    const float height)
{
    const float insetU = 0.5f / textureWidth;
    const float insetV = 0.5f / textureHeight;
    return TextureUvRect{
        .minU = std::clamp(x / textureWidth + insetU, 0.0f, 1.0f),
        .maxU = std::clamp((x + width) / textureWidth - insetU, 0.0f, 1.0f),
        .minV = std::clamp(y / textureHeight + insetV, 0.0f, 1.0f),
        .maxV = std::clamp((y + height) / textureHeight - insetV, 0.0f, 1.0f),
    };
}

[[nodiscard]] CuboidUvSet makeCuboidUvSet(
    const float textureWidth,
    const float textureHeight,
    const float u,
    const float v,
    const float width,
    const float height,
    const float depth)
{
    return CuboidUvSet{
        .front = uvRectFromPixels(textureWidth, textureHeight, u + depth, v + depth, width, height),
        .back = uvRectFromPixels(textureWidth, textureHeight, u + depth + width + depth, v + depth, width, height),
        .left = uvRectFromPixels(textureWidth, textureHeight, u, v + depth, depth, height),
        .right = uvRectFromPixels(textureWidth, textureHeight, u + depth + width, v + depth, depth, height),
        .top = uvRectFromPixels(textureWidth, textureHeight, u + depth, v, width, depth),
        .bottom = uvRectFromPixels(textureWidth, textureHeight, u + depth + width, v, width, depth),
    };
}

struct MobUvLayout
{
    CuboidUvSet body{};
    CuboidUvSet head{};
    CuboidUvSet leg{};
    CuboidUvSet arm{};
    CuboidUvSet wing{};
    CuboidUvSet beak{};
    CuboidUvSet snout{};
    CuboidUvSet horn{};
    CuboidUvSet wattle{};
};

[[nodiscard]] MobUvLayout uvLayoutForMobKind(const vibecraft::game::MobKind mobKind)
{
    constexpr float kTexW = 64.0f;
    constexpr float kTexH = 32.0f;
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 8.0f, 12.0f, 4.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .arm = makeCuboidUvSet(kTexW, kTexH, 40.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .horn = makeCuboidUvSet(kTexW, kTexH, 24.0f, 0.0f, 2.0f, 8.0f, 2.0f),
        };
    case MK::Skeleton:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 8.0f, 12.0f, 4.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 2.0f, 12.0f, 2.0f),
            .arm = makeCuboidUvSet(kTexW, kTexH, 40.0f, 16.0f, 2.0f, 12.0f, 2.0f),
        };
    case MK::Creeper:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 8.0f, 12.0f, 4.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 6.0f, 4.0f),
        };
    case MK::Spider:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 0.0f, 12.0f, 10.0f, 8.0f, 12.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 32.0f, 4.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 18.0f, 0.0f, 16.0f, 2.0f, 2.0f),
        };
    case MK::Cow:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 18.0f, 4.0f, 12.0f, 18.0f, 10.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 6.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 12.0f, 4.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 0.0f, 22.0f, 4.0f, 3.0f, 2.0f),
            .horn = makeCuboidUvSet(kTexW, kTexH, 22.0f, 0.0f, 1.0f, 3.0f, 1.0f),
        };
    case MK::Pig:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 28.0f, 8.0f, 8.0f, 14.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 6.0f, 4.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 16.0f, 16.0f, 4.0f, 3.0f, 2.0f),
        };
    case MK::Sheep:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 28.0f, 8.0f, 12.0f, 16.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 6.0f, 6.0f, 8.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 6.0f, 4.0f),
        };
    case MK::Chicken:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 0.0f, 9.0f, 6.0f, 8.0f, 6.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 4.0f, 6.0f, 3.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 26.0f, 0.0f, 3.0f, 5.0f, 3.0f),
            .wing = makeCuboidUvSet(kTexW, kTexH, 24.0f, 13.0f, 1.0f, 4.0f, 6.0f),
            .beak = makeCuboidUvSet(kTexW, kTexH, 14.0f, 0.0f, 4.0f, 2.0f, 2.0f),
            .wattle = makeCuboidUvSet(kTexW, kTexH, 14.0f, 4.0f, 2.0f, 2.0f, 2.0f),
        };
    case MK::Wolf:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 18.0f, 4.0f, 10.0f, 14.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 6.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 3.0f, 10.0f, 3.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 13.0f, 26.0f, 4.0f, 3.0f, 2.0f),
        };
    case MK::Bear:
        return {
            .body = makeCuboidUvSet(kTexW, kTexH, 18.0f, 4.0f, 12.0f, 14.0f, 8.0f),
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 8.0f, 6.0f),
            .leg = makeCuboidUvSet(kTexW, kTexH, 0.0f, 16.0f, 4.0f, 10.0f, 4.0f),
            .snout = makeCuboidUvSet(kTexW, kTexH, 13.0f, 26.0f, 4.0f, 3.0f, 2.0f),
        };
    case MK::SandScorpion:
        return {
            // Cephalothorax (front body segment)
            .body = makeCuboidUvSet(kTexW, kTexH, 0.0f, 12.0f, 12.0f, 4.0f, 8.0f),
            // Head
            .head = makeCuboidUvSet(kTexW, kTexH, 0.0f, 0.0f, 8.0f, 4.0f, 6.0f),
            // Walking legs (thin)
            .leg = makeCuboidUvSet(kTexW, kTexH, 40.0f, 0.0f, 8.0f, 2.0f, 1.0f),
            // Pincers/claws
            .arm = makeCuboidUvSet(kTexW, kTexH, 0.0f, 25.0f, 6.0f, 3.0f, 3.0f),
            // Stinger tail
            .horn = makeCuboidUvSet(kTexW, kTexH, 22.0f, 26.0f, 2.0f, 4.0f, 2.0f),
        };
    }
    return {};
}

[[nodiscard]] float referenceHeightPxForMobKind(const vibecraft::game::MobKind mobKind)
{
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:  return 32.0f;
    case MK::Skeleton: return 32.0f;
    case MK::Creeper:  return 30.0f;
    case MK::Spider:   return 16.0f;
    case MK::Cow:      return 20.0f;
    case MK::Pig:      return 16.0f;
    case MK::Sheep:    return 18.0f;
    case MK::Chicken:  return 12.0f;
    case MK::Wolf:         return 18.0f;
    case MK::Bear:         return 26.0f;
    case MK::SandScorpion: return 12.0f;
    }
    return 16.0f;
}

[[nodiscard]] float referenceHalfWidthPxForMobKind(const vibecraft::game::MobKind mobKind)
{
    using MK = vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MK::Zombie:
    case MK::Player:  return 6.0f;
    case MK::Skeleton: return 6.0f;
    case MK::Creeper:  return 6.0f;
    case MK::Spider:   return 11.0f;
    case MK::Cow:      return 6.0f;
    case MK::Pig:      return 4.4f;
    case MK::Sheep:    return 5.0f;
    case MK::Chicken:  return 4.0f;
    case MK::Wolf:         return 5.0f;
    case MK::Bear:         return 7.5f;
    case MK::SandScorpion: return 8.0f;
    }
    return 4.0f;
}
} // namespace

void Renderer::drawWorldMobSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    const auto toAbgrShade = [](const float shade)
    {
        const float clamped = std::clamp(shade, 0.0f, 1.0f);
        const std::uint8_t g = static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
        return (0xffu << 24u) | (static_cast<std::uint32_t>(g) << 16u)
            | (static_cast<std::uint32_t>(g) << 8u) | static_cast<std::uint32_t>(g);
    };

    const auto submitFace = [&](const glm::vec3& a,
                                const glm::vec3& b,
                                const glm::vec3& c,
                                const glm::vec3& d,
                                const TextureUvRect& uv,
                                const std::uint32_t abgr,
                                const std::uint16_t textureHandle)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return false;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return false;
        }

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = a.x, .y = a.y, .z = a.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.minU, .v = uv.maxV, .abgr = abgr},
            detail::ChunkVertex{
                .x = b.x, .y = b.y, .z = b.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.maxU, .v = uv.maxV, .abgr = abgr},
            detail::ChunkVertex{
                .x = c.x, .y = c.y, .z = c.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.maxU, .v = uv.minV, .abgr = abgr},
            detail::ChunkVertex{
                .x = d.x, .y = d.y, .z = d.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.minU, .v = uv.minV, .abgr = abgr},
        };

        bgfx::TransientVertexBuffer tvb{};
        bgfx::allocTransientVertexBuffer(&tvb, 4, detail::ChunkVertex::layout());
        std::memcpy(tvb.data, vertices, sizeof(vertices));

        bgfx::TransientIndexBuffer tib{};
        bgfx::allocTransientIndexBuffer(&tib, 6);
        auto* indices = reinterpret_cast<std::uint16_t*>(tib.data);
        indices[0] = 0; indices[1] = 1; indices[2] = 2;
        indices[3] = 0; indices[4] = 2; indices[5] = 3;

        const float identity[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        bgfx::setTransform(identity);
        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setIndexBuffer(&tib);
        bgfx::setTexture(
            0,
            detail::toUniformHandle(inventoryUiSamplerHandle_),
            detail::toTextureHandle(textureHandle));
        bgfx::setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z
            | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LESS);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };
    const auto resolveHeldSpriteTexture = [&](const FrameDebugData::WorldMobHud& mob,
                                              std::uint16_t& textureHandle,
                                              float& minU,
                                              float& maxU,
                                              float& minV,
                                              float& maxV)
    {
        textureHandle = UINT16_MAX;
        minU = 0.0f; maxU = 1.0f; minV = 0.0f; maxV = 1.0f;

        if (mob.heldItemKind != HudItemKind::None)
        {
            textureHandle = hudItemKindTextureHandle(mob.heldItemKind);
            const TextureUvRect uvRect = hudItemKindTextureUv(mob.heldItemKind);
            minU = uvRect.minU; maxU = uvRect.maxU;
            minV = uvRect.minV; maxV = uvRect.maxV;
        }
        if (textureHandle == UINT16_MAX && mob.heldBlockType != vibecraft::world::BlockType::Air)
        {
            if (chunkAtlasTextureHandle_ == UINT16_MAX)
            {
                return false;
            }
            textureHandle = chunkAtlasTextureHandle_;
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                mob.heldBlockType, vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
            minU = tileX * tileWidth;
            maxU = minU + tileWidth;
            minV = tileY * tileHeight;
            maxV = minV + tileHeight;
        }
        return textureHandle != UINT16_MAX;
    };

    for (const FrameDebugData::WorldMobHud& mob : frameDebugData.worldMobs)
    {
        const std::uint16_t mobTextureHandle = mobTextureHandleForKind(mob.mobKind);
        if (mobTextureHandle == UINT16_MAX)
        {
            continue;
        }
        const MobUvLayout uv = uvLayoutForMobKind(mob.mobKind);
        const float halfWidth = std::max(0.14f, mob.halfWidth);
        const float height = std::max(0.24f, mob.height);

        const float yawSin = std::sin(mob.yawRadians);
        const float yawCos = std::cos(mob.yawRadians);
        const glm::vec3 forward(yawSin, 0.0f, yawCos);
        const glm::vec3 right(yawCos, 0.0f, -yawSin);
        const float pitchSin = std::sin(mob.pitchRadians);
        const float pitchCos = std::cos(mob.pitchRadians);
        const glm::vec3 headForward = glm::normalize(forward * pitchCos + glm::vec3(0.0f, pitchSin, 0.0f));
        const glm::vec3 headUp = glm::normalize(glm::cross(headForward, right));
        const float sx = halfWidth / referenceHalfWidthPxForMobKind(mob.mobKind);
        const float sy = height / referenceHeightPxForMobKind(mob.mobKind);
        const float sz = sx;

        const auto submitHeldItemSprite = [&](const glm::vec3& centerOffsetPx,
                                              const float halfWidthPx,
                                              const float halfHeightPx)
        {
            std::uint16_t textureHandle = UINT16_MAX;
            float minU = 0.0f, maxU = 1.0f, minV = 0.0f, maxV = 1.0f;
            if (!resolveHeldSpriteTexture(mob, textureHandle, minU, maxU, minV, maxV))
            {
                return true;
            }
            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 spriteRight = right * (halfWidthPx * sx);
            const glm::vec3 spriteUp(0.0f, halfHeightPx * sy, 0.0f);
            return submitFace(
                center - spriteRight - spriteUp,
                center + spriteRight - spriteUp,
                center + spriteRight + spriteUp,
                center - spriteRight + spriteUp,
                TextureUvRect{.minU = minU, .maxU = maxU, .minV = minV, .maxV = maxV},
                0xffffffff,
                textureHandle);
        };

        const auto submitOrientedCuboid = [&](const glm::vec3& centerOffsetPx,
                                              const glm::vec3& halfExtentsWorld,
                                              const glm::vec3& axisX,
                                              const glm::vec3& axisY,
                                              const glm::vec3& axisZ,
                                              const CuboidUvSet& uvSet)
        {
            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 dx = axisX * halfExtentsWorld.x;
            const glm::vec3 dy = axisY * halfExtentsWorld.y;
            const glm::vec3 dz = axisZ * halfExtentsWorld.z;
            const glm::vec3 lbf = center - dx - dy + dz, rbf = center + dx - dy + dz;
            const glm::vec3 lbb = center - dx - dy - dz, rbb = center + dx - dy - dz;
            const glm::vec3 ltf = center - dx + dy + dz, rtf = center + dx + dy + dz;
            const glm::vec3 ltb = center - dx + dy - dz, rtb = center + dx + dy - dz;
            if (!submitFace(lbf, rbf, rtf, ltf, uvSet.front,  toAbgrShade(1.00f), mobTextureHandle)) return false;
            if (!submitFace(rbb, lbb, ltb, rtb, uvSet.back,   toAbgrShade(0.82f), mobTextureHandle)) return false;
            if (!submitFace(lbb, lbf, ltf, ltb, uvSet.left,   toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(rbf, rbb, rtb, rtf, uvSet.right,  toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(ltf, rtf, rtb, ltb, uvSet.top,    toAbgrShade(0.92f), mobTextureHandle)) return false;
            if (!submitFace(lbb, rbb, rbf, lbf, uvSet.bottom, toAbgrShade(0.62f), mobTextureHandle)) return false;
            return true;
        };

        const auto submitCuboid = [&](const glm::vec3& centerOffsetPx,
                                      const glm::vec3& halfExtentsPx,
                                      const CuboidUvSet& uvSet)
        {
            return submitOrientedCuboid(
                centerOffsetPx,
                glm::vec3(halfExtentsPx.x * sx, halfExtentsPx.y * sy, halfExtentsPx.z * sz),
                right, glm::vec3(0.0f, 1.0f, 0.0f), forward, uvSet);
        };

        const auto submitHorizontalBody = [&](const glm::vec3& centerOffsetPx,
                                              const glm::vec3& halfExtentsPx,
                                              const CuboidUvSet& uvSet)
        {
            const glm::vec3 center = mob.feetPosition
                + right * (centerOffsetPx.x * sx)
                + glm::vec3(0.0f, centerOffsetPx.y * sy, 0.0f)
                + forward * (centerOffsetPx.z * sz);
            const glm::vec3 dx = right * (halfExtentsPx.x * sx);
            const glm::vec3 dy(0.0f, halfExtentsPx.z * sy, 0.0f);
            const glm::vec3 dz = forward * (halfExtentsPx.y * sz);
            const glm::vec3 lbf = center - dx - dy + dz, rbf = center + dx - dy + dz;
            const glm::vec3 lbb = center - dx - dy - dz, rbb = center + dx - dy - dz;
            const glm::vec3 ltf = center - dx + dy + dz, rtf = center + dx + dy + dz;
            const glm::vec3 ltb = center - dx + dy - dz, rtb = center + dx + dy - dz;
            // Quadruped body: top/bottom faces swapped vs. biped
            if (!submitFace(lbf, rbf, rtf, ltf, uvSet.top,    toAbgrShade(1.00f), mobTextureHandle)) return false;
            if (!submitFace(rbb, lbb, ltb, rtb, uvSet.bottom,  toAbgrShade(0.82f), mobTextureHandle)) return false;
            if (!submitFace(lbb, lbf, ltf, ltb, uvSet.left,   toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(rbf, rbb, rtb, rtf, uvSet.right,  toAbgrShade(0.74f), mobTextureHandle)) return false;
            if (!submitFace(ltf, rtf, rtb, ltb, uvSet.back,   toAbgrShade(0.92f), mobTextureHandle)) return false;
            if (!submitFace(lbb, rbb, rbf, lbf, uvSet.front,  toAbgrShade(0.62f), mobTextureHandle)) return false;
            return true;
        };

        using MK = vibecraft::game::MobKind;
        switch (mob.mobKind)
        {
        case MK::Player:
        {
            // Player silhouette/proportions match classic Minecraft character dimensions.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 3.1f
                + mob.feetPosition.x * 0.19f + mob.feetPosition.z * 0.16f;
            const float armSwing = std::sin(gaitPhase) * 1.25f;
            const float legSwing = std::sin(gaitPhase + kPi) * 1.1f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.2f;
            if (!submitCuboid(glm::vec3(0.0f, 18.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 28.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f + bodyBob, armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f + bodyBob, -armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, -legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (mob.heldItemUsesSwordPose)
            {
                if (!submitHeldItemSprite(glm::vec3(8.7f, 15.5f + bodyBob, -armSwing - 1.0f), 1.35f, 4.1f)) break;
            }
            else if (mob.heldItemKind != HudItemKind::None || mob.heldBlockType != vibecraft::world::BlockType::Air)
            {
                if (!submitHeldItemSprite(glm::vec3(8.1f, 15.8f + bodyBob, -armSwing - 0.3f), 2.1f, 2.1f)) break;
            }
            break;
        }
        case MK::Zombie:
        {
            // Zombie keeps classic humanoid proportions, close to the player silhouette.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 3.2f
                + mob.feetPosition.x * 0.37f + mob.feetPosition.z * 0.29f;
            const float armSwing = std::sin(gaitPhase) * 1.25f;
            const float legSwing = std::sin(gaitPhase + kPi) * 1.05f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.2f;
            if (!submitCuboid(glm::vec3(0.0f, 18.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 28.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f + bodyBob, armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f + bodyBob, -armSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, -legSwing), glm::vec3(2.0f, 6.0f, 2.0f), uv.leg)) break;
            break;
        }
        case MK::Skeleton:
        {
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 3.35f
                + mob.feetPosition.x * 0.34f + mob.feetPosition.z * 0.31f;
            const float armSwing = std::sin(gaitPhase) * 1.15f;
            const float legSwing = std::sin(gaitPhase + kPi) * 0.95f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.17f;
            if (!submitCuboid(glm::vec3(0.0f, 18.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 28.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            if (!submitCuboid(glm::vec3(-6.0f, 18.0f + bodyBob, armSwing), glm::vec3(1.0f, 6.0f, 1.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(6.0f, 18.0f + bodyBob, -armSwing), glm::vec3(1.0f, 6.0f, 1.0f), uv.arm)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 6.0f, legSwing), glm::vec3(1.0f, 6.0f, 1.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.0f, 6.0f, -legSwing), glm::vec3(1.0f, 6.0f, 1.0f), uv.leg)) break;
            break;
        }
        case MK::Creeper:
        {
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 2.95f
                + mob.feetPosition.x * 0.27f + mob.feetPosition.z * 0.23f;
            const float legLift = std::sin(gaitPhase) * 0.65f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 1.9f)) * 0.18f;
            if (!submitCuboid(glm::vec3(0.0f, 17.0f + bodyBob, 0.0f), glm::vec3(4.0f, 6.0f, 2.0f), uv.body)) break;
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 27.2f + bodyBob, 0.0f),
                    glm::vec3(4.0f * sx, 4.0f * sy, 4.0f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            if (!submitCuboid(glm::vec3(-2.6f, 3.2f + legLift, 2.6f), glm::vec3(1.45f, 3.2f, 1.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.6f, 3.2f - legLift, 2.6f), glm::vec3(1.45f, 3.2f, 1.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-2.6f, 3.2f - legLift, -2.6f), glm::vec3(1.45f, 3.2f, 1.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.6f, 3.2f + legLift, -2.6f), glm::vec3(1.45f, 3.2f, 1.45f), uv.leg)) break;
            break;
        }
        case MK::Spider:
        {
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 4.35f
                + mob.feetPosition.x * 0.42f + mob.feetPosition.z * 0.39f;
            const float legSwing = std::sin(gaitPhase) * 1.2f;
            const float legSwingOpp = std::sin(gaitPhase + 3.14159265358979323846f) * 1.2f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 1.6f)) * 0.08f;
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 4.8f + bodyBob, -0.2f),
                    glm::vec3(4.2f, 5.0f, 2.0f), uv.body)) { break; }
            if (!submitCuboid(glm::vec3(0.0f, 5.7f + bodyBob, 5.3f), glm::vec3(3.0f, 2.2f, 3.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(-5.8f, 3.4f, 4.0f + legSwing), glm::vec3(4.8f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(5.8f, 3.4f, 4.0f - legSwing), glm::vec3(4.8f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-6.3f, 3.0f, 1.4f + legSwingOpp), glm::vec3(5.2f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(6.3f, 3.0f, 1.4f - legSwingOpp), glm::vec3(5.2f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-6.3f, 3.0f, -1.4f + legSwing), glm::vec3(5.2f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(6.3f, 3.0f, -1.4f - legSwing), glm::vec3(5.2f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-5.8f, 3.4f, -4.0f + legSwingOpp), glm::vec3(4.8f, 0.45f, 0.45f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(5.8f, 3.4f, -4.0f - legSwingOpp), glm::vec3(4.8f, 0.45f, 0.45f), uv.leg)) break;
            break;
        }
        case MK::Cow:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 13.2f, 0.0f), glm::vec3(6.4f, 8.4f, 4.8f), uv.body)) { break; }
            if (!submitCuboid(glm::vec3(0.0f, 15.0f, 8.4f), glm::vec3(3.0f, 3.0f, 4.0f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 13.5f, 12.7f), glm::vec3(1.5f, 1.4f, 2.0f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-2.0f, 17.3f, 8.9f), glm::vec3(0.5f, 0.8f, 0.7f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(2.0f, 17.3f, 8.9f), glm::vec3(0.5f, 0.8f, 0.7f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, 4.5f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(4.2f, 6.0f, -4.8f), glm::vec3(1.6f, 6.0f, 1.6f), uv.leg)) break;
            break;
        case MK::Pig:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 7.4f, -0.8f), glm::vec3(4.6f, 7.0f, 3.2f), uv.body)) { break; }
            if (!submitCuboid(glm::vec3(0.0f, 8.0f, 6.9f), glm::vec3(3.2f, 2.8f, 3.2f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 7.6f, 10.2f), glm::vec3(1.5f, 1.0f, 1.1f), uv.snout)) break;
            if (!submitCuboid(glm::vec3(-2.2f, 2.7f, 3.9f), glm::vec3(1.05f, 2.7f, 1.05f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.2f, 2.7f, 3.9f), glm::vec3(1.05f, 2.7f, 1.05f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-3.0f, 2.7f, -3.4f), glm::vec3(1.05f, 2.7f, 1.05f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(3.0f, 2.7f, -3.4f), glm::vec3(1.05f, 2.7f, 1.05f), uv.leg)) break;
            break;
        case MK::Sheep:
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(6.0f, 8.0f, 4.0f), uv.body)) { break; }
            if (!submitCuboid(glm::vec3(0.0f, 11.0f, 7.4f), glm::vec3(2.5f, 3.1f, 3.5f), uv.head)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, 2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(2.5f, 3.0f, -2.0f), glm::vec3(2.0f, 3.0f, 2.0f), uv.leg)) break;
            break;
        case MK::Chicken:
            if (!submitCuboid(glm::vec3(0.0f, 7.0f, -0.2f), glm::vec3(2.4f, 3.2f, 3.0f), uv.body)) break;
            if (!submitCuboid(glm::vec3(0.0f, 10.4f, 3.5f), glm::vec3(1.8f, 2.3f, 1.7f), uv.head)) break;
            if (!submitCuboid(glm::vec3(0.0f, 7.8f, -3.7f), glm::vec3(0.7f, 0.7f, 1.6f), uv.wattle)) break;
            if (!submitCuboid(glm::vec3(0.0f, 9.8f, 5.5f), glm::vec3(1.0f, 0.6f, 1.1f), uv.beak)) break;
            if (!submitCuboid(glm::vec3(-2.8f, 7.6f, -0.1f), glm::vec3(0.45f, 1.7f, 2.8f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(2.8f, 7.6f, -0.1f), glm::vec3(0.45f, 1.7f, 2.8f), uv.wing)) break;
            if (!submitCuboid(glm::vec3(-0.9f, 2.1f, 0.9f), glm::vec3(0.28f, 2.1f, 0.28f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(0.9f, 2.1f, 0.9f), glm::vec3(0.28f, 2.1f, 0.28f), uv.leg)) break;
            break;
        case MK::Wolf:
        {
            // Quadruped — low-slung body, four legs, head with snout.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 4.8f
                + mob.feetPosition.x * 0.41f + mob.feetPosition.z * 0.37f;
            const float legSwing = std::sin(gaitPhase) * 1.3f;
            const float legSwingOpp = std::sin(gaitPhase + kPi) * 1.3f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.22f;
            // Body — horizontal, wolf is longer than tall
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 9.0f + bodyBob, 0.0f),
                    glm::vec3(4.0f, 5.6f, 3.2f), uv.body)) { break; }
            // Head — oriented toward yaw, slightly raised
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 10.8f + bodyBob, 6.8f),
                    glm::vec3(3.0f * sx, 2.8f * sy, 3.0f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            // Snout protrudes forward from the head
            if (!submitCuboid(glm::vec3(0.0f, 9.6f + bodyBob, 9.8f), glm::vec3(1.5f, 1.1f, 1.0f), uv.snout)) break;
            // Four legs — front pair swings opposite to rear pair
            if (!submitCuboid(glm::vec3(-2.3f, 4.5f,  3.5f + legSwing),    glm::vec3(1.1f, 4.5f, 1.1f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 2.3f, 4.5f,  3.5f + legSwingOpp), glm::vec3(1.1f, 4.5f, 1.1f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-2.3f, 4.5f, -3.5f + legSwingOpp), glm::vec3(1.1f, 4.5f, 1.1f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 2.3f, 4.5f, -3.5f + legSwing),    glm::vec3(1.1f, 4.5f, 1.1f), uv.leg)) break;
            break;
        }
        case MK::Bear:
        {
            // Heavy quadruped — wide body, thick legs, large head, slow lumbering gait.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 2.8f
                + mob.feetPosition.x * 0.31f + mob.feetPosition.z * 0.27f;
            const float legSwing = std::sin(gaitPhase) * 0.9f;
            const float legSwingOpp = std::sin(gaitPhase + kPi) * 0.9f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.28f;
            // Large horizontal body
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 11.5f + bodyBob, 0.0f),
                    glm::vec3(5.8f, 7.2f, 4.0f), uv.body)) { break; }
            // Big head, oriented toward yaw
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 13.5f + bodyBob, 8.2f),
                    glm::vec3(3.8f * sx, 3.5f * sy, 3.8f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            // Broad muzzle
            if (!submitCuboid(glm::vec3(0.0f, 11.8f + bodyBob, 12.2f), glm::vec3(1.8f, 1.4f, 1.2f), uv.snout)) break;
            // Four thick legs — front opposite to rear
            if (!submitCuboid(glm::vec3(-3.0f, 5.5f,  4.2f + legSwing),    glm::vec3(1.8f, 5.5f, 1.8f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 3.0f, 5.5f,  4.2f + legSwingOpp), glm::vec3(1.8f, 5.5f, 1.8f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-3.0f, 5.5f, -4.2f + legSwingOpp), glm::vec3(1.8f, 5.5f, 1.8f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 3.0f, 5.5f, -4.2f + legSwing),    glm::vec3(1.8f, 5.5f, 1.8f), uv.leg)) break;
            break;
        }
        case MK::SandScorpion:
        {
            // Low, wide arachnid — flat body, 8 walking legs, 2 front pincers, tail stinger.
            constexpr float kPi = 3.14159265358979323846f;
            const float gaitPhase = cameraFrameData.weatherTimeSeconds * 5.5f
                + mob.feetPosition.x * 0.44f + mob.feetPosition.z * 0.41f;
            const float legSwing = std::sin(gaitPhase) * 0.80f;
            const float legSwingOpp = std::sin(gaitPhase + kPi) * 0.80f;
            const float bodyBob = std::abs(std::sin(gaitPhase * 2.0f)) * 0.06f;
            // Main body — very flat and wide
            if (!submitHorizontalBody(
                    glm::vec3(0.0f, 3.8f + bodyBob, -0.5f),
                    glm::vec3(5.2f, 6.4f, 1.6f), uv.body)) { break; }
            // Cephalothorax / head — front segment, slightly raised
            if (!submitOrientedCuboid(
                    glm::vec3(0.0f, 4.2f + bodyBob, 5.6f),
                    glm::vec3(3.2f * sx, 2.4f * sy, 3.2f * sz),
                    right, headUp, headForward, uv.head)) { break; }
            // Two front pincers (claws) angled outward
            if (!submitCuboid(glm::vec3(-4.5f, 3.6f + bodyBob, 7.5f), glm::vec3(2.2f, 0.7f, 0.7f), uv.arm)) break;
            if (!submitCuboid(glm::vec3( 4.5f, 3.6f + bodyBob, 7.5f), glm::vec3(2.2f, 0.7f, 0.7f), uv.arm)) break;
            // Stinger tail — curls up and over the back
            if (!submitCuboid(glm::vec3(0.0f, 6.8f + bodyBob, -5.5f), glm::vec3(0.6f, 2.8f, 0.6f), uv.horn)) break;
            if (!submitCuboid(glm::vec3(0.0f, 9.2f + bodyBob, -3.8f), glm::vec3(0.5f, 0.5f, 1.2f), uv.horn)) break;
            // 8 walking legs — 4 per side, thin and horizontal
            if (!submitCuboid(glm::vec3(-5.0f, 2.5f,  3.8f + legSwing),    glm::vec3(4.2f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 5.0f, 2.5f,  3.8f + legSwingOpp), glm::vec3(4.2f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-5.4f, 2.4f,  1.3f + legSwingOpp), glm::vec3(4.6f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 5.4f, 2.4f,  1.3f + legSwing),    glm::vec3(4.6f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-5.4f, 2.4f, -1.3f + legSwing),    glm::vec3(4.6f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 5.4f, 2.4f, -1.3f + legSwingOpp), glm::vec3(4.6f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3(-5.0f, 2.5f, -3.8f + legSwingOpp), glm::vec3(4.2f, 0.35f, 0.35f), uv.leg)) break;
            if (!submitCuboid(glm::vec3( 5.0f, 2.5f, -3.8f + legSwing),    glm::vec3(4.2f, 0.35f, 0.35f), uv.leg)) break;
            break;
        }
        }  // end switch (mob.mobKind)

        constexpr float kMobHealthBarDamagedEpsilon = 0.08f;
        const bool mobHealthBarVisible = mob.mobKind != MK::Player && mob.mobHealthMax > 1e-3f
            && mob.mobHealthCurrent > 1e-3f
            && mob.mobHealthCurrent < mob.mobHealthMax - kMobHealthBarDamagedEpsilon;
        if (mobHealthBarVisible && chunkAtlasTextureHandle_ != UINT16_MAX)
        {
            glm::vec3 camForward = cameraFrameData.forward;
            if (glm::dot(camForward, camForward) > 1e-8f)
                camForward = glm::normalize(camForward);
            else
                camForward = glm::vec3(0.0f, 0.0f, -1.0f);

            const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 camRight = glm::cross(camForward, worldUp);
            if (glm::dot(camRight, camRight) < 1e-8f)
                camRight = glm::cross(camForward, glm::vec3(1.0f, 0.0f, 0.0f));
            camRight = glm::normalize(camRight);

            const float ratio = std::clamp(mob.mobHealthCurrent / mob.mobHealthMax, 0.0f, 1.0f);
            const glm::vec3 headTop = mob.feetPosition + glm::vec3(0.0f, mob.height + 0.14f, 0.0f);
            const float barHalfW = std::max(0.28f, mob.halfWidth * 1.25f);
            const float barHalfH = 0.045f;
            const glm::vec3 barCenter = headTop + worldUp * 0.05f;

            const std::uint32_t bgAbgr = toAbgrShade(0.14f);
            const std::uint32_t fillAbgr = toAbgrShade(0.3f + 0.62f * ratio);
            const glm::vec3 zBias = camForward * 0.02f;

            const std::uint8_t stoneTileIndex = vibecraft::world::textureTileIndex(
                vibecraft::world::BlockType::Stone, vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(stoneTileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(stoneTileIndex / kChunkAtlasTileColumns);
            TextureUvRect stoneUv{
                .minU = tileX * tileWidth,
                .maxU = tileX * tileWidth + tileWidth,
                .minV = tileY * tileHeight,
                .maxV = tileY * tileHeight + tileHeight,
            };

            const glm::vec3 bgA = barCenter - camRight * barHalfW - worldUp * barHalfH;
            const glm::vec3 bgB = barCenter + camRight * barHalfW - worldUp * barHalfH;
            const glm::vec3 bgC = barCenter + camRight * barHalfW + worldUp * barHalfH;
            const glm::vec3 bgD = barCenter - camRight * barHalfW + worldUp * barHalfH;
            static_cast<void>(submitFace(bgA, bgB, bgC, bgD, stoneUv, bgAbgr, chunkAtlasTextureHandle_));

            const float fillHalfW = barHalfW * ratio;
            if (fillHalfW > 1e-4f)
            {
                const glm::vec3 fillCenter = barCenter - camRight * (barHalfW * (1.0f - ratio));
                const float innerH = barHalfH * 0.88f;
                const glm::vec3 fA = fillCenter - camRight * fillHalfW - worldUp * innerH + zBias;
                const glm::vec3 fB = fillCenter + camRight * fillHalfW - worldUp * innerH + zBias;
                const glm::vec3 fC = fillCenter + camRight * fillHalfW + worldUp * innerH + zBias;
                const glm::vec3 fD = fillCenter - camRight * fillHalfW + worldUp * innerH + zBias;
                static_cast<void>(submitFace(fA, fB, fC, fD, stoneUv, fillAbgr, chunkAtlasTextureHandle_));
            }
        }
    }
}

} // namespace vibecraft::render
