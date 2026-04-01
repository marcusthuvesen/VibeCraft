#pragma once

#include <cstdint>

namespace vibecraft::game
{
class PlayerVitals;

enum class OxygenTankTier : std::uint8_t
{
    None = 0,
    Starter,
    Field,
    Expedition,
};

struct OxygenTankDefinition
{
    float capacity = 0.0f;
    float safeZoneRefillPerSecond = 0.0f;
};

[[nodiscard]] constexpr OxygenTankDefinition oxygenTankDefinition(const OxygenTankTier tankTier)
{
    switch (tankTier)
    {
    case OxygenTankTier::None:
        return {.capacity = 0.0f, .safeZoneRefillPerSecond = 0.0f};
    case OxygenTankTier::Starter:
        return {.capacity = 180.0f, .safeZoneRefillPerSecond = 10.0f};
    case OxygenTankTier::Field:
        return {.capacity = 300.0f, .safeZoneRefillPerSecond = 12.0f};
    case OxygenTankTier::Expedition:
        return {.capacity = 420.0f, .safeZoneRefillPerSecond = 14.0f};
    }

    return {.capacity = 0.0f, .safeZoneRefillPerSecond = 0.0f};
}

[[nodiscard]] const char* oxygenTankTierName(OxygenTankTier tankTier);

struct OxygenState
{
    OxygenTankTier tankTier = OxygenTankTier::Starter;
    float oxygen = oxygenTankDefinition(OxygenTankTier::Starter).capacity;
    float capacity = oxygenTankDefinition(OxygenTankTier::Starter).capacity;
};

struct OxygenSystemSettings
{
    float baseDrainPerSecond = 1.0f;
    float depletedHealthDamagePerSecond = 1.0f;
};

struct OxygenEnvironment
{
    bool insideSafeZone = false;
    float drainMultiplier = 1.0f;
    float safeZoneRefillPerSecond = 0.0f;
};

struct OxygenTickResult
{
    float oxygenBefore = 0.0f;
    float oxygenAfter = 0.0f;
    float healthDamageApplied = 0.0f;
    bool insideSafeZone = false;
    bool depleted = false;
};

class OxygenSystem
{
  public:
    explicit OxygenSystem(const OxygenSystemSettings& settings = {});

    void resetForNewGame(OxygenTankTier startingTankTier = OxygenTankTier::Starter, float fillRatio = 1.0f);
    void setState(const OxygenState& state);
    [[nodiscard]] const OxygenState& state() const;

    void setTankTier(OxygenTankTier tankTier, bool refillToCapacity);
    [[nodiscard]] float refill(float amount);
    [[nodiscard]] float consume(float amount);
    [[nodiscard]] OxygenTickResult tick(
        float deltaTimeSeconds,
        const OxygenEnvironment& environment,
        PlayerVitals* vitals = nullptr);

  private:
    [[nodiscard]] float effectiveSafeZoneRefillPerSecond(const OxygenEnvironment& environment) const;

    OxygenSystemSettings settings_{};
    OxygenState state_{};
};
}  // namespace vibecraft::game
