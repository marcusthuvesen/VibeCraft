#include "vibecraft/app/WaterMovement.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    if (glm::dot(v, v) > 1.0e-8f)
    {
        return glm::normalize(v);
    }
    return fallback;
}
}  // namespace

void computeWaterSwimDisplacement(const WaterSwimInput& input, WaterSwimOutput& output)
{
    output.horizontalDisplacement = glm::vec3(0.0f);
    output.swimVerticalFromLook = 0.0f;

    const glm::vec3 swimForward = safeNormalize(input.cameraForward, glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::vec3 swimRight = safeNormalize(input.cameraRight, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 swimWish = swimForward * input.localMotion.z + swimRight * input.localMotion.x;

    output.horizontalDisplacement = glm::vec3(swimWish.x, 0.0f, swimWish.z);

    if (input.headSubmergedInWater)
    {
        output.swimVerticalFromLook = swimWish.y;
        return;
    }

    // Surface (body in water, eyes above): float and strafe like Minecraft — no vertical shove from a level look.
    constexpr float kDivePitchCos = -0.2f;
    if (swimForward.y <= kDivePitchCos)
    {
        constexpr float kSurfaceDiveScale = 0.65f;
        output.swimVerticalFromLook = swimWish.y * kSurfaceDiveScale;
    }
}

void integrateWaterVerticalVelocity(float& verticalVelocity, const WaterVerticalInput& input)
{
    if (input.settings == nullptr || input.deltaTimeSeconds <= 0.0f)
    {
        return;
    }

    const PlayerMovementSettings& s = *input.settings;
    const float dt = input.deltaTimeSeconds;

    if (input.headSubmergedInWater)
    {
        verticalVelocity += s.waterSubmergedBuoyancyAcceleration * dt;
        if (input.jumpHeld)
        {
            verticalVelocity += s.waterSubmergedSwimUpAcceleration * dt;
        }
        if (input.sneaking)
        {
            verticalVelocity -= s.waterSubmergedSinkAcceleration * dt;
        }
        verticalVelocity -= s.waterSubmergedGravity * dt;
        verticalVelocity = std::clamp(
            verticalVelocity,
            -s.waterSubmergedTerminalFallVelocity,
            s.waterSubmergedTerminalRiseVelocity);
        verticalVelocity *= std::exp(-s.waterSubmergedVerticalDrag * dt);
    }
    else
    {
        verticalVelocity += s.waterSurfaceBuoyancyAcceleration * dt;
        if (input.jumpHeld)
        {
            verticalVelocity += s.waterSurfaceSwimUpAcceleration * dt;
        }
        if (input.sneaking)
        {
            verticalVelocity -= s.waterSurfaceSinkAcceleration * dt;
        }
        verticalVelocity -= s.waterSurfaceGravity * dt;
        verticalVelocity = std::clamp(
            verticalVelocity,
            -s.waterSurfaceTerminalFallVelocity,
            s.waterSurfaceTerminalRiseVelocity);
        verticalVelocity *= std::exp(-s.waterSurfaceVerticalDrag * dt);
    }
}
}  // namespace vibecraft::app
