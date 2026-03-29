#pragma once

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/MobTypes.hpp"
#include "vibecraft/game/PlayerVitals.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace vibecraft::world
{
class TerrainGenerator;
class World;
}

namespace vibecraft::game
{
struct MobSpawnSettings
{
    std::size_t maxHostileMobsNearPlayer = 10;
    std::size_t maxPassiveMobsNearPlayer = 14;
    float spawnMinHorizontalDistance = 18.0f;
    float spawnMaxHorizontalDistance = 48.0f;
    float despawnHorizontalDistance = 72.0f;
    float spawnAttemptIntervalSeconds = 2.5f;
    float mobHalfWidth = 0.28f;
    float mobHeight = 1.75f;
    float mobMoveSpeed = 2.8f;
    float passiveWanderSpeed = 1.2f;
    float passiveFleeSpeed = 3.0f;
    float passiveFleePlayerDistance = 7.0f;
    float passiveWanderTimerMinSeconds = 1.4f;
    float passiveWanderTimerMaxSeconds = 3.8f;
    float meleeReach = 1.15f;
    float meleeDamage = 2.0f;
    float attackCooldownSeconds = 1.1f;
    float collisionSweepStep = 0.15f;
    float mobStepHeight = 1.05f;
    float minSeparationFromPlayer = 2.5f;
    float minSeparationFromMob = 1.2f;
};

struct MobDamageResult
{
    std::uint32_t mobId = 0;
    MobKind mobKind = MobKind::HostileStalker;
    glm::vec3 feetPosition{0.0f};
    bool killed = false;
};

class MobSpawnSystem
{
  public:
    explicit MobSpawnSystem(const MobSpawnSettings& settings = {});

    [[nodiscard]] const std::vector<MobInstance>& mobs() const;
    [[nodiscard]] const MobSpawnSettings& settings() const;
    [[nodiscard]] std::optional<MobDamageResult> damageClosestAlongRay(
        const world::World& world,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        float maxDistance,
        float damageAmount,
        const glm::vec3& attackerFeet,
        float knockbackDistance);

    void clearAllMobs();

    /// Deterministic tests / repro.
    void setRngSeedForTests(std::uint32_t seed);

    /// Hostile spawn (night) + passive animals (day on grass), movement, and hostile melee. Skips
    /// spawning when `spawningEnabled` is false; still runs AI on existing mobs unless you call
    /// `clearAllMobs` when disabling.
    void tick(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        float deltaSeconds,
        TimeOfDayPeriod timePeriod,
        bool spawningEnabled,
        PlayerVitals& playerVitals);

  private:
    MobSpawnSettings settings_{};
    std::vector<MobInstance> mobs_{};
    std::uint32_t nextId_ = 1;
    float hostileSpawnAccumulatorSeconds_ = 0.0f;
    float passiveSpawnAccumulatorSeconds_ = 0.0f;
    std::mt19937 rng_{};

    [[nodiscard]] bool trySpawnOneHostile(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        TimeOfDayPeriod timePeriod);

    [[nodiscard]] bool trySpawnOnePassive(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        TimeOfDayPeriod timePeriod);

    void despawnDistant(const glm::vec3& playerFeet);
    void moveMobAxis(
        const world::World& world,
        MobInstance& mob,
        int axisIndex,
        float displacement);
    void applyMelee(
        MobInstance& mob,
        const glm::vec3& playerFeet,
        PlayerVitals& playerVitals);
};
}  // namespace vibecraft::game
