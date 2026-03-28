#pragma once

#include <cstdint>

namespace vibecraft::game
{
enum class MobKind : std::uint8_t
{
    HostileStalker = 0,
};

struct EnemyInstance
{
    std::uint32_t id = 0;
    MobKind kind = MobKind::HostileStalker;
    float feetX = 0.0f;
    float feetY = 0.0f;
    float feetZ = 0.0f;
    float yawRadians = 0.0f;
    float attackCooldownSeconds = 0.0f;
};
}  // namespace vibecraft::game
