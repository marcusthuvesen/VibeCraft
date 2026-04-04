#pragma once

#include <glm/vec3.hpp>

namespace vibecraft::app
{
struct PlayerMovementSettings;

/// Camera-relative swim wish from WASD, already scaled by move speed and delta time (local Z = forward, X = strafe).
struct WaterSwimInput
{
    glm::vec3 cameraForward{0.0f};
    glm::vec3 cameraRight{0.0f};
    glm::vec3 localMotion{0.0f};
    bool headSubmergedInWater = false;
};

struct WaterSwimOutput
{
    glm::vec3 horizontalDisplacement{0.0f};
    float swimVerticalFromLook = 0.0f;
};

/// Minecraft-style: at the surface you mostly move on XZ; look-based vertical swim applies fully when submerged,
/// and on the surface only when looking down (dive) or with reduced coupling so you do not constantly sink.
void computeWaterSwimDisplacement(const WaterSwimInput& input, WaterSwimOutput& output);

struct WaterVerticalInput
{
    const PlayerMovementSettings* settings = nullptr;
    bool headSubmergedInWater = false;
    bool jumpHeld = false;
    bool sneaking = false;
    float deltaTimeSeconds = 0.0f;
};

/// Integrates vertical velocity while standing in water (buoyancy, swim up, sink, drag).
void integrateWaterVerticalVelocity(float& verticalVelocity, const WaterVerticalInput& input);
}  // namespace vibecraft::app
