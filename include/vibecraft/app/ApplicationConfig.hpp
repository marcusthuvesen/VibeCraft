#pragma once

#include <cstddef>
#include <cstdint>

namespace vibecraft::app
{
struct WindowSettings
{
    std::uint32_t width = 1600;
    std::uint32_t height = 900;
};

struct StreamingSettings
{
    int bootstrapChunkRadius = 2;
    int residentChunkRadius = 3;
    int generationChunkRadius = 5;
    std::size_t generationChunkBudgetPerFrame = 12;
    std::size_t prefetchGenerationBudgetPerFrame = 6;
    std::size_t meshBuildBudgetPerFrame = 5;
    std::size_t offResidentDirtyRebuildBudget = 8;
    int forwardPrefetchChunks = 2;
};

struct InputTuning
{
    float moveSpeed = 4.317f;
    float sneakSpeedMultiplier = 0.3f;
    float sprintSpeedMultiplier = 1.58f;
    float mouseSensitivity = 0.09f;
    float reachDistance = 6.0f;
};

struct PlayerMovementSettings
{
    float colliderHalfWidth = 0.3f;
    float standingColliderHeight = 1.8f;
    float sneakingColliderHeight = 1.5f;
    float standingEyeHeight = 1.62f;
    float sneakingEyeHeight = 1.27f;
    float gravity = 32.0f;
    float jumpVelocity = 9.0f;
    float terminalFallVelocity = 45.0f;
    float waterMoveSpeedMultiplier = 0.30f;
    float waterGravity = 10.0f;
    float waterBuoyancyAcceleration = 10.0f;
    float waterTerminalFallVelocity = 4.2f;
    float waterTerminalRiseVelocity = 4.0f;
    float waterSwimUpAcceleration = 15.0f;
    float waterSinkAcceleration = 6.0f;
    float waterVerticalDrag = 4.0f;
    float maxStepHeight = 1.0f;
    float collisionSweepStep = 0.2f;
    float groundProbeDistance = 0.05f;
    float vineClimbSpeed = 2.45f;
    float vineDescendSpeed = 3.1f;
    float vineIdleFallGravityMultiplier = 0.14f;
};

inline constexpr WindowSettings kWindowSettings{};
inline constexpr StreamingSettings kStreamingSettings{};
inline constexpr InputTuning kInputTuning{};
inline constexpr PlayerMovementSettings kPlayerMovementSettings{};
inline constexpr float kFloatEpsilon = 0.0001f;
inline constexpr float kNetworkTickSeconds = 1.0f / 20.0f;
}  // namespace vibecraft::app
