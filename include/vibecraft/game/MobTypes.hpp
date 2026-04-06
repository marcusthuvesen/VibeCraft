#pragma once

#include <cstdint>

namespace vibecraft::game
{
enum class MobKind : std::uint8_t
{
    Zombie = 0,
    Player,
    Skeleton,
    Creeper,
    Spider,
    Cow,
    Pig,
    Sheep,
    Chicken,
};

[[nodiscard]] constexpr bool isHostileMob(const MobKind kind)
{
    return kind == MobKind::Zombie
        || kind == MobKind::Skeleton
        || kind == MobKind::Creeper
        || kind == MobKind::Spider;
}

[[nodiscard]] constexpr bool isPassiveMob(const MobKind kind)
{
    return !isHostileMob(kind);
}

/// Minecraft-like: undead hostiles burn in direct sunlight (not creepers/spiders).
[[nodiscard]] constexpr bool burnsInDaylight(const MobKind kind)
{
    return kind == MobKind::Zombie || kind == MobKind::Skeleton;
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
    case MobKind::Skeleton:
        return 20.0f;
    case MobKind::Creeper:
        return 20.0f;
    case MobKind::Spider:
        return 16.0f;
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
    float pitchRadians = 0.0f;
    float attackCooldownSeconds = 0.0f;
    /// Creeper: seconds spent in blast range with line of sight (fuse). Other kinds ignore.
    float creeperFuseSeconds = 0.0f;
    /// Creeper: throttle for fuse hiss cues.
    float creeperFuseSoundCooldownSeconds = 0.0f;
    /// Passive: countdown until a new wander heading is chosen.
    float wanderTimerSeconds = 0.0f;
    /// Passive: current wander heading on the XZ plane.
    float wanderYawRadians = 0.0f;
    /// Passive breeding cooldown. Adults cannot breed while this is positive.
    float breedCooldownSeconds = 0.0f;
    /// Passive baby growth timer. Positive values mean the mob is still a juvenile.
    float growthSecondsRemaining = 0.0f;
    float health = 1.0f;
    float halfWidth = 0.30f;
    /// Default kind is Zombie; keep in sync with `adultDimensionsForMobKind(MobKind::Zombie)`.
    float height = 2.0f * (1.95f / 1.8f);
};
}  // namespace vibecraft::game
