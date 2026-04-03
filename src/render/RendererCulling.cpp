#include "vibecraft/render/RendererDetail.hpp"

#include <algorithm>
#include <glm/geometric.hpp>

namespace vibecraft::render::detail
{
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

[[nodiscard]] float distanceSqCameraToAabb(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax)
{
    const glm::vec3 closest{
        std::clamp(cameraPosition.x, aabbMin.x, aabbMax.x),
        std::clamp(cameraPosition.y, aabbMin.y, aabbMax.y),
        std::clamp(cameraPosition.z, aabbMin.z, aabbMax.z)};
    const glm::vec3 offset = cameraPosition - closest;
    return glm::dot(offset, offset);
}

[[nodiscard]] float distanceSqCameraToAabbXZ(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax)
{
    const float closestX = std::clamp(cameraPosition.x, aabbMin.x, aabbMax.x);
    const float closestZ = std::clamp(cameraPosition.z, aabbMin.z, aabbMax.z);
    const float dx = cameraPosition.x - closestX;
    const float dz = cameraPosition.z - closestZ;
    return dx * dx + dz * dz;
}

[[nodiscard]] float distanceSqCameraToAabbDownWeighted(
    const glm::vec3& cameraPosition,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    const float belowCameraWeight,
    const float aboveCameraWeight)
{
    const glm::vec3 closest{
        std::clamp(cameraPosition.x, aabbMin.x, aabbMax.x),
        std::clamp(cameraPosition.y, aabbMin.y, aabbMax.y),
        std::clamp(cameraPosition.z, aabbMin.z, aabbMax.z)};
    const glm::vec3 offset = cameraPosition - closest;
    const float verticalWeight = offset.y > 0.0f ? belowCameraWeight : aboveCameraWeight;
    const float weightedDy = offset.y * verticalWeight;
    return offset.x * offset.x + weightedDy * weightedDy + offset.z * offset.z;
}
}  // namespace vibecraft::render::detail
