#pragma once

#include "vibecraft/game/MobSpawnSystem.hpp"

#include <random>

namespace vibecraft::game
{
void tickPassiveMobLifecycle(std::vector<MobInstance>& mobs, const MobSpawnSettings& settings, float deltaSeconds);

void tickPassiveBreeding(
    std::vector<MobInstance>& mobs,
    const MobSpawnSettings& settings,
    std::mt19937& rng,
    float deltaSeconds,
    std::uint32_t& nextId);
}  // namespace vibecraft::game
