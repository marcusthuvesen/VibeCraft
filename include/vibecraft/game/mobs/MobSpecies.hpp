#pragma once

#include "vibecraft/game/MobTypes.hpp"

#include <algorithm>

namespace vibecraft::game
{
struct MobDimensions
{
    float halfWidth = 0.28f;
    float height = 1.75f;
};

[[nodiscard]] constexpr MobDimensions adultDimensionsForMobKind(const MobKind kind)
{
    switch (kind)
    {
    case MobKind::Zombie:
        // Match classic Minecraft hostile silhouette (roughly player-scale height).
        return {.halfWidth = 0.30f, .height = 1.95f};
    case MobKind::Player:
        return {.halfWidth = 0.30f, .height = 2.0f};
    case MobKind::Skeleton:
        return {.halfWidth = 0.28f, .height = 1.99f};
    case MobKind::Creeper:
        return {.halfWidth = 0.35f, .height = 1.70f};
    case MobKind::Spider:
        return {.halfWidth = 0.70f, .height = 0.90f};
    case MobKind::Cow:
        return {.halfWidth = 0.45f, .height = 1.40f};
    case MobKind::Pig:
        return {.halfWidth = 0.35f, .height = 0.92f};
    case MobKind::Sheep:
        return {.halfWidth = 0.43f, .height = 1.24f};
    case MobKind::Chicken:
        return {.halfWidth = 0.20f, .height = 0.78f};
    }
    return {.halfWidth = 0.28f, .height = 1.75f};
}

[[nodiscard]] constexpr bool isBreedablePassiveMobKind(const MobKind kind)
{
    return kind == MobKind::Cow || kind == MobKind::Pig || kind == MobKind::Sheep || kind == MobKind::Chicken;
}

[[nodiscard]] inline MobDimensions scaledMobDimensions(
    const MobKind kind,
    const float scale)
{
    const MobDimensions adult = adultDimensionsForMobKind(kind);
    const float clampedScale = std::max(scale, 0.05f);
    return {
        .halfWidth = adult.halfWidth * clampedScale,
        .height = adult.height * clampedScale,
    };
}
}  // namespace vibecraft::game
