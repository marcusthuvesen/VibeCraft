#include "vibecraft/app/Application.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_set>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] float passiveMobStepDistance(const game::MobKind kind)
{
    using MK = game::MobKind;
    switch (kind)
    {
    case MK::Cow:
        return 0.95f;
    case MK::Pig:
        return 0.72f;
    case MK::Sheep:
        return 0.82f;
    case MK::Chicken:
        return 0.42f;
    case MK::Zombie:
        return 0.88f;
    case MK::Skeleton:
        return 0.90f;
    case MK::Creeper:
        return 0.84f;
    case MK::Spider:
        return 0.64f;
    case MK::Player:
        return 0.0f;
    }
    return 0.8f;
}

[[nodiscard]] float passiveMobAmbientBaseSeconds(const game::MobKind kind)
{
    using MK = game::MobKind;
    switch (kind)
    {
    case MK::Cow:
        return 8.0f;
    case MK::Pig:
        return 7.0f;
    case MK::Sheep:
        return 8.5f;
    case MK::Chicken:
        return 5.5f;
    case MK::Zombie:
        return 10.0f;
    case MK::Skeleton:
        return 9.5f;
    case MK::Creeper:
        return 11.0f;
    case MK::Spider:
        return 8.0f;
    case MK::Player:
        return 1000.0f;
    }
    return 8.0f;
}

[[nodiscard]] float passiveMobAmbientRangeSeconds(const game::MobKind kind)
{
    using MK = game::MobKind;
    switch (kind)
    {
    case MK::Cow:
        return 6.0f;
    case MK::Pig:
        return 5.0f;
    case MK::Sheep:
        return 5.5f;
    case MK::Chicken:
        return 4.0f;
    case MK::Zombie:
        return 6.0f;
    case MK::Skeleton:
        return 5.5f;
    case MK::Creeper:
        return 6.5f;
    case MK::Spider:
        return 5.0f;
    case MK::Player:
        return 0.0f;
    }
    return 5.0f;
}

[[nodiscard]] float nextPassiveMobAmbientDelay(std::uint32_t& rngState, const game::MobKind kind)
{
    rngState = rngState * 1664525u + 1013904223u;
    const float jitter01 = static_cast<float>((rngState >> 8U) & 0x00ffffffU) / static_cast<float>(0x01000000U);
    return passiveMobAmbientBaseSeconds(kind) + passiveMobAmbientRangeSeconds(kind) * jitter01;
}
}  // namespace

void Application::updateMobSoundEffects(
    const float deltaTimeSeconds,
    const std::vector<vibecraft::game::MobInstance>& mobs)
{
    if (gameScreen_ != GameScreen::Playing)
    {
        return;
    }

    std::unordered_set<std::uint32_t> activeMobIds;
    activeMobIds.reserve(mobs.size());
    const glm::vec3 listenerFeet = playerFeetPosition_;

    for (const game::MobInstance& mob : mobs)
    {
        if (mob.kind == game::MobKind::Player)
        {
            continue;
        }

        activeMobIds.insert(mob.id);
        MobAudioState& state = mobAudioStateById_[mob.id];
        const glm::vec3 feetPosition(mob.feetX, mob.feetY, mob.feetZ);
        if (!state.initialized)
        {
            state.lastFeetPosition = feetPosition;
            state.rngState = mob.id * 747796405u + 2891336453u;
            state.ambientCooldownSeconds = nextPassiveMobAmbientDelay(state.rngState, mob.kind);
            state.initialized = true;
            continue;
        }

        const glm::vec2 horizontalDelta(feetPosition.x - state.lastFeetPosition.x, feetPosition.z - state.lastFeetPosition.z);
        state.stepDistanceAccumulator += glm::length(horizontalDelta);
        const float stepDistance = passiveMobStepDistance(mob.kind);
        const float listenerDistance = glm::distance(listenerFeet, feetPosition);
        if (stepDistance > 0.0f && listenerDistance <= 18.0f)
        {
            while (state.stepDistanceAccumulator >= stepDistance)
            {
                state.stepDistanceAccumulator -= stepDistance;
                soundEffects_.playMobStep(mob.kind);
            }
        }
        else if (stepDistance > 0.0f)
        {
            state.stepDistanceAccumulator = std::fmod(state.stepDistanceAccumulator, stepDistance);
        }

        state.ambientCooldownSeconds -= deltaTimeSeconds;
        if (state.ambientCooldownSeconds <= 0.0f && listenerDistance <= 24.0f)
        {
            soundEffects_.playMobAmbient(mob.kind);
            state.ambientCooldownSeconds = nextPassiveMobAmbientDelay(state.rngState, mob.kind);
        }

        state.lastFeetPosition = feetPosition;
    }

    for (auto it = mobAudioStateById_.begin(); it != mobAudioStateById_.end();)
    {
        if (!activeMobIds.contains(it->first))
        {
            it = mobAudioStateById_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
}  // namespace vibecraft::app
