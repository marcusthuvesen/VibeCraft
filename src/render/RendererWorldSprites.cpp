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

void Renderer::drawWorldPickupSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    for (const FrameDebugData::WorldPickupHud& pickup : frameDebugData.worldPickups)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            break;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            break;
        }

        std::uint16_t textureHandle = UINT16_MAX;
        float minU = 0.0f;
        float maxU = 1.0f;
        float minV = 0.0f;
        float maxV = 1.0f;

        if (pickup.itemKind != HudItemKind::None)
        {
            textureHandle = hudItemKindTextureHandle(pickup.itemKind);
            const TextureUvRect uvRect = hudItemKindTextureUv(pickup.itemKind);
            minU = uvRect.minU;
            maxU = uvRect.maxU;
            minV = uvRect.minV;
            maxV = uvRect.maxV;
        }
        if (textureHandle == UINT16_MAX && pickup.blockType != vibecraft::world::BlockType::Air)
        {
            if (chunkAtlasTextureHandle_ == UINT16_MAX)
            {
                continue;
            }
            textureHandle = chunkAtlasTextureHandle_;
            const std::uint8_t tileIndex = vibecraft::world::textureTileIndex(
                pickup.blockType,
                vibecraft::world::BlockFace::Side);
            const float tileWidth = 1.0f / static_cast<float>(kChunkAtlasTileColumns);
            const float tileHeight = 1.0f / static_cast<float>(kChunkAtlasTileRows);
            const float tileX = static_cast<float>(tileIndex % kChunkAtlasTileColumns);
            const float tileY = static_cast<float>(tileIndex / kChunkAtlasTileColumns);
            minU = tileX * tileWidth;
            maxU = minU + tileWidth;
            minV = tileY * tileHeight;
            maxV = minV + tileHeight;
        }
        if (textureHandle == UINT16_MAX)
        {
            continue;
        }

        const glm::vec3 center = pickup.worldPosition;
        glm::vec3 toCamera = cameraFrameData.position - center;
        const float distance = std::max(0.001f, glm::length(toCamera));
        if (distance > 0.001f)
        {
            toCamera /= distance;
        }
        else
        {
            toCamera = -cameraFrameData.forward;
        }

        glm::vec3 right = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), toCamera);
        if (glm::dot(right, right) < 1.0e-6f)
        {
            right = glm::cross(cameraFrameData.up, toCamera);
        }
        if (glm::dot(right, right) < 1.0e-6f)
        {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(toCamera, right);
        if (glm::dot(up, up) < 1.0e-6f)
        {
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        up = glm::normalize(up);

        const float cosA = std::cos(pickup.spinRadians);
        const float sinA = std::sin(pickup.spinRadians);
        const glm::vec3 rolledRight = glm::normalize(right * cosA + up * sinA);
        const glm::vec3 rolledUp = glm::normalize(-right * sinA + up * cosA);
        const float scale = glm::clamp(0.19f + 2.1f / std::max(distance, 3.2f), 0.19f, 0.28f);
        right = rolledRight * scale;
        up = rolledUp * (scale * 1.06f);
        const std::uint32_t pickupAbgr = detail::packAbgr8(glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);

        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = center.x - right.x - up.x,
                .y = center.y - right.y - up.y,
                .z = center.z - right.z - up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = minU, .v = maxV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x + right.x - up.x,
                .y = center.y + right.y - up.y,
                .z = center.z + right.z - up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = maxU, .v = maxV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x + right.x + up.x,
                .y = center.y + right.y + up.y,
                .z = center.z + right.z + up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = maxU, .v = minV,
                .abgr = pickupAbgr},
            detail::ChunkVertex{
                .x = center.x - right.x + up.x,
                .y = center.y - right.y + up.y,
                .z = center.z - right.z + up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = minU, .v = minV,
                .abgr = pickupAbgr},
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
    }
}

void Renderer::drawWorldProjectileSprites(
    const FrameDebugData& frameDebugData,
    const CameraFrameData& cameraFrameData)
{
    if (inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    for (const FrameDebugData::WorldProjectileHud& projectile : frameDebugData.worldProjectiles)
    {
        const std::uint16_t textureHandle = hudItemKindTextureHandle(projectile.itemKind);
        if (textureHandle == UINT16_MAX)
        {
            continue;
        }
        const TextureUvRect uv = hudItemKindTextureUv(projectile.itemKind);
        const glm::vec3 toProjectile = projectile.worldPosition - cameraFrameData.position;
        const float distanceSq = glm::dot(toProjectile, toProjectile);
        if (distanceSq < 1.2f * 1.2f || distanceSq > 96.0f * 96.0f)
        {
            continue;
        }
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4
            || bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            break;
        }

        glm::vec3 velocityDir = projectile.velocity;
        if (glm::dot(velocityDir, velocityDir) > 1.0e-8f)
        {
            velocityDir = glm::normalize(velocityDir);
        }
        else
        {
            velocityDir = glm::normalize(cameraFrameData.forward);
        }

        glm::vec3 toCamera = cameraFrameData.position - projectile.worldPosition;
        if (glm::dot(toCamera, toCamera) <= 1.0e-8f)
        {
            toCamera = -cameraFrameData.forward;
        }
        toCamera = glm::normalize(toCamera);

        glm::vec3 right = glm::cross(toCamera, velocityDir);
        if (glm::dot(right, right) <= 1.0e-8f)
        {
            right = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), velocityDir);
        }
        if (glm::dot(right, right) <= 1.0e-8f)
        {
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(velocityDir, right);
        if (glm::dot(up, up) <= 1.0e-8f)
        {
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        up = glm::normalize(up);

        const float distance = std::sqrt(distanceSq);
        const float halfWidth = glm::clamp(0.03f + 0.2f / std::max(distance, 2.0f), 0.03f, 0.07f);
        const float halfHeight = halfWidth * 2.6f;
        right *= halfWidth;
        up *= halfHeight;

        const std::uint32_t arrowAbgr = detail::packAbgr8(glm::vec3(1.0f), 1.0f);
        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{
                .x = projectile.worldPosition.x - right.x - up.x,
                .y = projectile.worldPosition.y - right.y - up.y,
                .z = projectile.worldPosition.z - right.z - up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.minU, .v = uv.maxV,
                .abgr = arrowAbgr},
            detail::ChunkVertex{
                .x = projectile.worldPosition.x + right.x - up.x,
                .y = projectile.worldPosition.y + right.y - up.y,
                .z = projectile.worldPosition.z + right.z - up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.maxU, .v = uv.maxV,
                .abgr = arrowAbgr},
            detail::ChunkVertex{
                .x = projectile.worldPosition.x + right.x + up.x,
                .y = projectile.worldPosition.y + right.y + up.y,
                .z = projectile.worldPosition.z + right.z + up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.maxU, .v = uv.minV,
                .abgr = arrowAbgr},
            detail::ChunkVertex{
                .x = projectile.worldPosition.x - right.x + up.x,
                .y = projectile.worldPosition.y - right.y + up.y,
                .z = projectile.worldPosition.z - right.z + up.z,
                .nx = 0.0f, .ny = 1.0f, .nz = 0.0f,
                .u = uv.minU, .v = uv.minV,
                .abgr = arrowAbgr},
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
    }
}

void Renderer::drawBlockBreakingOverlay(const FrameDebugData& frameDebugData)
{
    if (!frameDebugData.hasTarget || !frameDebugData.miningTargetActive
        || inventoryUiProgramHandle_ == UINT16_MAX || inventoryUiSamplerHandle_ == UINT16_MAX)
    {
        return;
    }

    const float clampedProgress = std::clamp(frameDebugData.miningTargetProgress, 0.0f, 0.999f);
    const int stage = std::clamp(static_cast<int>(std::floor(clampedProgress * 10.0f)), 0, 9);
    const std::uint16_t textureHandle = blockBreakStageTextureHandles_[static_cast<std::size_t>(stage)];
    if (textureHandle == UINT16_MAX)
    {
        return;
    }

    const auto submitFace = [&](const glm::vec3& a,
                                const glm::vec3& b,
                                const glm::vec3& c,
                                const glm::vec3& d)
    {
        if (bgfx::getAvailTransientVertexBuffer(4, detail::ChunkVertex::layout()) < 4)
        {
            return false;
        }
        if (bgfx::getAvailTransientIndexBuffer(6) < 6)
        {
            return false;
        }

        constexpr std::uint32_t kOverlayAbgr = 0xd8ffffff;
        detail::ChunkVertex vertices[4] = {
            detail::ChunkVertex{.x = a.x, .y = a.y, .z = a.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 0.0f, .v = 1.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = b.x, .y = b.y, .z = b.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 1.0f, .v = 1.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = c.x, .y = c.y, .z = c.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 1.0f, .v = 0.0f, .abgr = kOverlayAbgr},
            detail::ChunkVertex{.x = d.x, .y = d.y, .z = d.z, .nx = 0.0f, .ny = 1.0f, .nz = 0.0f, .u = 0.0f, .v = 0.0f, .abgr = kOverlayAbgr},
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
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_DEPTH_TEST_LEQUAL);
        bgfx::submit(detail::kMainView, detail::toProgramHandle(inventoryUiProgramHandle_));
        return true;
    };

    constexpr float kInset = 0.0015f;
    constexpr float kExpand = 0.0025f;
    const float minX = static_cast<float>(frameDebugData.targetBlock.x) - kInset;
    const float minY = static_cast<float>(frameDebugData.targetBlock.y) - kInset;
    const float minZ = static_cast<float>(frameDebugData.targetBlock.z) - kInset;
    const float maxX = static_cast<float>(frameDebugData.targetBlock.x + 1) + kInset;
    const float maxY = static_cast<float>(frameDebugData.targetBlock.y + 1) + kInset;
    const float maxZ = static_cast<float>(frameDebugData.targetBlock.z + 1) + kInset;

    const glm::vec3 lbf(minX - kExpand, minY - kExpand, maxZ + kExpand);
    const glm::vec3 rbf(maxX + kExpand, minY - kExpand, maxZ + kExpand);
    const glm::vec3 lbb(minX - kExpand, minY - kExpand, minZ - kExpand);
    const glm::vec3 rbb(maxX + kExpand, minY - kExpand, minZ - kExpand);
    const glm::vec3 ltf(minX - kExpand, maxY + kExpand, maxZ + kExpand);
    const glm::vec3 rtf(maxX + kExpand, maxY + kExpand, maxZ + kExpand);
    const glm::vec3 ltb(minX - kExpand, maxY + kExpand, minZ - kExpand);
    const glm::vec3 rtb(maxX + kExpand, maxY + kExpand, minZ - kExpand);

    if (!submitFace(lbf, rbf, rtf, ltf)) return;
    if (!submitFace(rbb, lbb, ltb, rtb)) return;
    if (!submitFace(lbb, lbf, ltf, ltb)) return;
    if (!submitFace(rbf, rbb, rtb, rtf)) return;
    if (!submitFace(ltf, rtf, rtb, ltb)) return;
    static_cast<void>(submitFace(lbb, rbb, rbf, lbf));
}

} // namespace vibecraft::render
