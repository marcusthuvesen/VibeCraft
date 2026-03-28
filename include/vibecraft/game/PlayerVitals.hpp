#pragma once

#include <string_view>

namespace vibecraft::game
{
enum class DamageCause
{
    Fall,
    Lava,
    Drowning,
    EnemyAttack,
};

[[nodiscard]] std::string_view damageCauseName(DamageCause cause);

struct DamageEvent
{
    DamageCause cause = DamageCause::EnemyAttack;
    float amount = 0.0f;
};

struct EnvironmentalHazards
{
    bool bodyInWater = false;
    bool bodyInLava = false;
    bool headSubmergedInWater = false;
};

struct PlayerVitalsSettings
{
    float maxHealth = 20.0f;
    float maxAir = 10.0f;
    float fallDamageSafeDistance = 3.0f;
    float airDepletionPerSecond = 1.0f;
    float airRecoveryPerSecond = 4.0f;
    float drowningDamage = 2.0f;
    float drowningIntervalSeconds = 1.0f;
    float lavaDamage = 4.0f;
    float lavaIntervalSeconds = 0.5f;
};

class PlayerVitals
{
  public:
    explicit PlayerVitals(const PlayerVitalsSettings& settings = {});

    void reset();
    void tickEnvironment(float deltaTimeSeconds, const EnvironmentalHazards& hazards);

    [[nodiscard]] float applyDamage(const DamageEvent& event);
    [[nodiscard]] float applyLandingImpact(float fallDistance, bool softenWithWater);

    [[nodiscard]] float health() const;
    [[nodiscard]] float maxHealth() const;
    [[nodiscard]] float air() const;
    [[nodiscard]] float maxAir() const;
    [[nodiscard]] bool isDead() const;
    [[nodiscard]] DamageCause lastDamageCause() const;

  private:
    PlayerVitalsSettings settings_{};
    float health_ = 0.0f;
    float air_ = 0.0f;
    float lavaTickAccumulator_ = 0.0f;
    float drowningTickAccumulator_ = 0.0f;
    bool isDead_ = false;
    DamageCause lastDamageCause_ = DamageCause::EnemyAttack;
};
}  // namespace vibecraft::game
