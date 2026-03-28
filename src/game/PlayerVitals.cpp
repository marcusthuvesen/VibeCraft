#include "vibecraft/game/PlayerVitals.hpp"

#include <algorithm>
#include <cmath>

namespace vibecraft::game
{
std::string_view damageCauseName(const DamageCause cause)
{
    switch (cause)
    {
    case DamageCause::Fall:
        return "fall";
    case DamageCause::Lava:
        return "lava";
    case DamageCause::Drowning:
        return "drowning";
    case DamageCause::EnemyAttack:
        return "enemy";
    }

    return "unknown";
}

PlayerVitals::PlayerVitals(const PlayerVitalsSettings& settings) : settings_(settings)
{
    reset();
}

void PlayerVitals::reset()
{
    health_ = settings_.maxHealth;
    air_ = settings_.maxAir;
    lavaTickAccumulator_ = 0.0f;
    drowningTickAccumulator_ = 0.0f;
    isDead_ = false;
    lastDamageCause_ = DamageCause::EnemyAttack;
}

void PlayerVitals::tickEnvironment(const float deltaTimeSeconds, const EnvironmentalHazards& hazards)
{
    if (isDead_ || deltaTimeSeconds <= 0.0f)
    {
        return;
    }

    if (hazards.bodyInLava && settings_.lavaIntervalSeconds > 0.0f)
    {
        lavaTickAccumulator_ += deltaTimeSeconds;
        while (lavaTickAccumulator_ >= settings_.lavaIntervalSeconds && !isDead_)
        {
            lavaTickAccumulator_ -= settings_.lavaIntervalSeconds;
            static_cast<void>(applyDamage({.cause = DamageCause::Lava, .amount = settings_.lavaDamage}));
        }
    }
    else
    {
        lavaTickAccumulator_ = 0.0f;
    }

    if (!hazards.headSubmergedInWater || hazards.bodyInLava)
    {
        air_ = std::min(settings_.maxAir, air_ + settings_.airRecoveryPerSecond * deltaTimeSeconds);
        drowningTickAccumulator_ = 0.0f;
        return;
    }

    if (settings_.airDepletionPerSecond <= 0.0f || settings_.drowningIntervalSeconds <= 0.0f)
    {
        return;
    }

    float remainingTimeSeconds = deltaTimeSeconds;
    if (air_ > 0.0f && settings_.airDepletionPerSecond > 0.0f)
    {
        const float timeToExhaustAir = air_ / settings_.airDepletionPerSecond;
        if (remainingTimeSeconds < timeToExhaustAir)
        {
            air_ = std::max(0.0f, air_ - settings_.airDepletionPerSecond * remainingTimeSeconds);
            drowningTickAccumulator_ = 0.0f;
            return;
        }

        air_ = 0.0f;
        remainingTimeSeconds -= timeToExhaustAir;
    }
    else
    {
        air_ = 0.0f;
    }

    drowningTickAccumulator_ += remainingTimeSeconds;
    while (drowningTickAccumulator_ >= settings_.drowningIntervalSeconds && !isDead_)
    {
        drowningTickAccumulator_ -= settings_.drowningIntervalSeconds;
        static_cast<void>(applyDamage({.cause = DamageCause::Drowning, .amount = settings_.drowningDamage}));
    }
}

float PlayerVitals::applyDamage(const DamageEvent& event)
{
    if (isDead_ || event.amount <= 0.0f)
    {
        return 0.0f;
    }

    const float previousHealth = health_;
    health_ = std::max(0.0f, health_ - event.amount);
    lastDamageCause_ = event.cause;
    isDead_ = health_ <= 0.0f;
    return previousHealth - health_;
}

float PlayerVitals::applyLandingImpact(const float fallDistance, const bool softenWithWater)
{
    if (softenWithWater || isDead_)
    {
        return 0.0f;
    }

    const float damageAmount = std::ceil(std::max(0.0f, fallDistance - settings_.fallDamageSafeDistance));
    return applyDamage({.cause = DamageCause::Fall, .amount = damageAmount});
}

float PlayerVitals::health() const
{
    return health_;
}

float PlayerVitals::maxHealth() const
{
    return settings_.maxHealth;
}

float PlayerVitals::air() const
{
    return air_;
}

float PlayerVitals::maxAir() const
{
    return settings_.maxAir;
}

bool PlayerVitals::isDead() const
{
    return isDead_;
}

DamageCause PlayerVitals::lastDamageCause() const
{
    return lastDamageCause_;
}
}  // namespace vibecraft::game
