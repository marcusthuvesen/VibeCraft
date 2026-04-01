#include "vibecraft/game/OxygenSystem.hpp"

#include <algorithm>

#include "vibecraft/game/PlayerVitals.hpp"

namespace vibecraft::game
{
const char* oxygenTankTierName(const OxygenTankTier tankTier)
{
    switch (tankTier)
    {
    case OxygenTankTier::None:
        return "none";
    case OxygenTankTier::Starter:
        return "starter";
    case OxygenTankTier::Field:
        return "field";
    case OxygenTankTier::Expedition:
        return "expedition";
    }

    return "unknown";
}

OxygenSystem::OxygenSystem(const OxygenSystemSettings& settings) : settings_(settings)
{
    resetForNewGame();
}

void OxygenSystem::resetForNewGame(const OxygenTankTier startingTankTier, const float fillRatio)
{
    state_.tankTier = startingTankTier;
    state_.capacity = oxygenTankDefinition(startingTankTier).capacity;
    state_.oxygen = std::clamp(state_.capacity * fillRatio, 0.0f, state_.capacity);
}

void OxygenSystem::setState(const OxygenState& state)
{
    state_.tankTier = state.tankTier;
    state_.capacity = std::max(0.0f, state.capacity);
    state_.oxygen = std::clamp(state.oxygen, 0.0f, state_.capacity);
}

const OxygenState& OxygenSystem::state() const
{
    return state_;
}

void OxygenSystem::setTankTier(const OxygenTankTier tankTier, const bool refillToCapacity)
{
    state_.tankTier = tankTier;
    state_.capacity = oxygenTankDefinition(tankTier).capacity;
    state_.oxygen = refillToCapacity ? state_.capacity : std::min(state_.oxygen, state_.capacity);
}

float OxygenSystem::refill(const float amount)
{
    if (amount <= 0.0f || state_.capacity <= 0.0f)
    {
        return 0.0f;
    }

    const float oxygenBefore = state_.oxygen;
    state_.oxygen = std::clamp(state_.oxygen + amount, 0.0f, state_.capacity);
    return state_.oxygen - oxygenBefore;
}

float OxygenSystem::consume(const float amount)
{
    if (amount <= 0.0f || state_.oxygen <= 0.0f)
    {
        return 0.0f;
    }

    const float oxygenBefore = state_.oxygen;
    state_.oxygen = std::max(0.0f, state_.oxygen - amount);
    return oxygenBefore - state_.oxygen;
}

OxygenTickResult OxygenSystem::tick(
    const float deltaTimeSeconds,
    const OxygenEnvironment& environment,
    PlayerVitals* vitals)
{
    OxygenTickResult result{
        .oxygenBefore = state_.oxygen,
        .oxygenAfter = state_.oxygen,
        .healthDamageApplied = 0.0f,
        .insideSafeZone = environment.insideSafeZone,
        .depleted = state_.oxygen <= 0.0f,
    };

    if (deltaTimeSeconds <= 0.0f)
    {
        return result;
    }

    if (environment.insideSafeZone)
    {
        static_cast<void>(refill(effectiveSafeZoneRefillPerSecond(environment) * deltaTimeSeconds));
    }
    else
    {
        const float drainMultiplier = std::max(0.0f, environment.drainMultiplier);
        static_cast<void>(consume(settings_.baseDrainPerSecond * drainMultiplier * deltaTimeSeconds));
    }

    result.oxygenAfter = state_.oxygen;
    result.depleted = state_.oxygen <= 0.0f;

    if (result.depleted && vitals != nullptr && settings_.depletedHealthDamagePerSecond > 0.0f)
    {
        result.healthDamageApplied = vitals->applyDamage({
            .cause = DamageCause::OxygenDepletion,
            .amount = settings_.depletedHealthDamagePerSecond * deltaTimeSeconds,
        });
    }

    return result;
}

float OxygenSystem::effectiveSafeZoneRefillPerSecond(const OxygenEnvironment& environment) const
{
    const float tankDefaultRefill = oxygenTankDefinition(state_.tankTier).safeZoneRefillPerSecond;
    return environment.safeZoneRefillPerSecond > 0.0f ? environment.safeZoneRefillPerSecond : tankDefaultRefill;
}
}  // namespace vibecraft::game
