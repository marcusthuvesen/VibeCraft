#pragma once

#include <cstdint>

namespace vibecraft::game
{
enum class MobKind : std::uint8_t
{
    HostileStalker = 0,
    Player,
    Cow,
    Pig,
    Sheep,
    Chicken,
};

[[nodiscard]] constexpr bool isHostileMob(const MobKind kind)
{
    return kind == MobKind::HostileStalker;
}

[[nodiscard]] constexpr bool isPassiveMob(const MobKind kind)
{
    return !isHostileMob(kind);
}

struct MobInstance
{
    std::uint32_t id = 0;
    MobKind kind = MobKind::HostileStalker;
    float feetX = 0.0f;
    float feetY = 0.0f;
    float feetZ = 0.0f;
    float yawRadians = 0.0f;
    float attackCooldownSeconds = 0.0f;
    /// Passive: countdown until a new wander heading is chosen.
    float wanderTimerSeconds = 0.0f;
    /// Passive: current wander heading on the XZ plane.
    float wanderYawRadians = 0.0f;
    float health = 1.0f;
    float halfWidth = 0.28f;
    float height = 1.75f;
};
}  // namespace vibecraft::game
