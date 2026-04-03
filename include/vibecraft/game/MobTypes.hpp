#pragma once

#include <cstdint>

namespace vibecraft::game
{
enum class MobKind : std::uint8_t
{
    Zombie = 0,
    Player,
    Cow,
    Pig,
    Sheep,
    Chicken,
};

[[nodiscard]] constexpr bool isHostileMob(const MobKind kind)
{
    return kind == MobKind::Zombie;
}

[[nodiscard]] constexpr bool isPassiveMob(const MobKind kind)
{
    return !isHostileMob(kind);
}

/// Spawn / snapshot defaults when a precise health value is not on the wire.
[[nodiscard]] constexpr float mobKindDefaultMaxHealth(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::Zombie:
        return 20.0f;
    case MobKind::Player:
        return 20.0f;
    case MobKind::Cow:
        return 12.0f;
    case MobKind::Pig:
        return 10.0f;
    case MobKind::Sheep:
        return 8.0f;
    case MobKind::Chicken:
        return 4.0f;
    }
    return 10.0f;
}

struct MobInstance
{
    std::uint32_t id = 0;
    MobKind kind = MobKind::Zombie;
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
