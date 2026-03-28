#pragma once

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/EnemyTypes.hpp"
#include "vibecraft/game/PlayerVitals.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
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
    std::size_t maxMobsNearPlayer = 16;
    float spawnMinHorizontalDistance = 18.0f;
    float spawnMaxHorizontalDistance = 48.0f;
    float despawnHorizontalDistance = 72.0f;
    float spawnAttemptIntervalSeconds = 2.5f;
    float mobHalfWidth = 0.28f;
    float mobHeight = 1.75f;
    float mobMoveSpeed = 2.8f;
    float meleeReach = 1.15f;
    float meleeDamage = 2.0f;
    float attackCooldownSeconds = 1.1f;
    float collisionSweepStep = 0.15f;
    float minSeparationFromPlayer = 2.5f;
    float minSeparationFromMob = 1.2f;
};

class MobSpawnSystem
{
  public:
    explicit MobSpawnSystem(const MobSpawnSettings& settings = {});

    [[nodiscard]] const std::vector<EnemyInstance>& enemies() const;
    [[nodiscard]] const MobSpawnSettings& settings() const;

    void clearAllMobs();

    /// Deterministic tests / repro.
    void setRngSeedForTests(std::uint32_t seed);

    /// Hostile spawn + chase + melee. Skips spawning when `spawningEnabled` is false; still runs AI
    /// on existing mobs unless you call `clearAllMobs` when disabling.
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
    std::vector<EnemyInstance> enemies_{};
    std::uint32_t nextId_ = 1;
    float spawnAccumulatorSeconds_ = 0.0f;
    std::mt19937 rng_{};

    [[nodiscard]] bool trySpawnOne(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        TimeOfDayPeriod timePeriod);

    void despawnDistant(const glm::vec3& playerFeet);
    void moveTowardPlayerAxis(
        const world::World& world,
        EnemyInstance& enemy,
        int axisIndex,
        float displacement);
    void applyMelee(
        EnemyInstance& enemy,
        const glm::vec3& playerFeet,
        PlayerVitals& playerVitals);
};
}  // namespace vibecraft::game
