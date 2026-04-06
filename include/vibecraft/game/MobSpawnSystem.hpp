#pragma once

#include "vibecraft/game/DayNightCycle.hpp"
#include "vibecraft/game/MobTypes.hpp"
#include "vibecraft/game/PlayerVitals.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
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
    float passiveBreedRange = 3.0f;
    float passiveBreedChancePerSecond = 0.012f;
    float passiveBreedCooldownSeconds = 300.0f;
    float passiveBabyGrowSeconds = 1200.0f;
    float passiveBabyScale = 0.58f;
    float passiveBreedCrowdRadius = 10.0f;
    std::size_t maxPassiveMobsAfterBreeding = 24;
    std::size_t maxNearbySameKindForBreeding = 6;
    float meleeReach = 1.15f;
    float meleeDamage = 2.0f;
    float attackCooldownSeconds = 1.1f;
    float collisionSweepStep = 0.15f;
    float mobStepHeight = 1.05f;
    float minSeparationFromPlayer = 2.5f;
    float minSeparationFromMob = 1.2f;
    /// Hostiles cannot spawn within this horizontal radius of any torch block.
    float hostileTorchExclusionRadius = 10.0f;
};

struct MobDamageResult
{
    std::uint32_t mobId = 0;
    MobKind mobKind = MobKind::Zombie;
    glm::vec3 feetPosition{0.0f};
    bool killed = false;
};

enum class HostileProjectileKind : std::uint8_t
{
    Arrow = 0,
};

struct HostileProjectile
{
    std::uint32_t id = 0;
    std::uint32_t ownerMobId = 0;
    MobKind ownerMobKind = MobKind::Skeleton;
    HostileProjectileKind kind = HostileProjectileKind::Arrow;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float radius = 0.14f;
    float gravity = 12.0f;
    float damage = 4.0f;
    float remainingLifeSeconds = 2.0f;
};

enum class MobCombatEventType : std::uint8_t
{
    MeleeAttack = 0,
    ProjectileFired,
    ProjectileHitBlock,
    ProjectileHitPlayer,
    /// Player arrow hit a mob (`actorKind` is victim). `projectileMobHitLethal` if it killed.
    ProjectileHitMob,
    /// Creeper fuse hiss (throttled while charging).
    CreeperFuseSound,
    /// Creeper detonation at `worldPosition` with `blastRadiusBlocks`.
    CreeperExplosion,
    /// Throttled cue while undead mobs take daylight fire damage.
    DaylightBurnDamage,
    /// Hostile mob was removed after lethal daylight burn (play defeat cue).
    HostileMobBurnDeath,
};

struct MobCombatEvent
{
    MobCombatEventType type = MobCombatEventType::MeleeAttack;
    MobKind actorKind = MobKind::Zombie;
    glm::vec3 worldPosition{0.0f};
    HostileProjectileKind projectileKind = HostileProjectileKind::Arrow;
    /// Valid for `CreeperExplosion` (matches `Application::explodeTntAt` radius clamp).
    float blastRadiusBlocks = 3.0f;
    bool projectileMobHitLethal = false;
};

/// Ray vs mob AABBs (same rules as `MobSpawnSystem::damageClosestAlongRay`).
[[nodiscard]] std::optional<std::size_t> findClosestMobIndexAlongRay(
    const std::vector<MobInstance>& mobs,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirection,
    float maxDistance);

class MobSpawnSystem
{
  public:
    explicit MobSpawnSystem(const MobSpawnSettings& settings = {});

    [[nodiscard]] const std::vector<MobInstance>& mobs() const;
    [[nodiscard]] const std::vector<HostileProjectile>& projectiles() const;
    [[nodiscard]] const MobSpawnSettings& settings() const;
    [[nodiscard]] std::vector<MobCombatEvent> takeCombatEvents();
    void setSettings(const MobSpawnSettings& settings);
    [[nodiscard]] std::optional<MobDamageResult> damageClosestAlongRay(
        const world::World& world,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        float maxDistance,
        float damageAmount,
        const glm::vec3& attackerFeet,
        float knockbackDistance);

    [[nodiscard]] std::optional<MobDamageResult> damageMobAtIndex(
        const world::World& world,
        std::size_t mobIndex,
        float damageAmount,
        const glm::vec3& attackerFeet,
        const glm::vec3& attackRayDirection,
        float knockbackDistance);

    void clearAllMobs();

    /// Host/single-player: player-fired arrow (same simulation as skeleton arrows; hits mobs, not shooter).
    void spawnPlayerArrow(
        const glm::vec3& origin,
        const glm::vec3& directionUnit,
        float speed,
        float gravity,
        float damage,
        float radius,
        float lifeSeconds);

    /// Deterministic tests / repro.
    void setRngSeedForTests(std::uint32_t seed);
    void addMobForTests(MobInstance mob);

    /// Hostile spawn (night) + passive animals (day on grass), movement, and hostile melee. Skips
    /// spawning when `spawningEnabled` is false; still runs AI on existing mobs unless you call
    /// `clearAllMobs` when disabling.
    /// When `remotePlayerFeetForMultiTarget` is non-empty (multiplayer host), mobs chase/flee/damage
    /// the nearest **living** player among the host and remotes; `remotePlayerHealthForMelee` must
    /// match that span and is updated in place when a remote is hit. Spawn/despawn still anchor on
    /// the host feet plus remotes for separation and despawn distance.
    void tick(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        float deltaSeconds,
        TimeOfDayPeriod timePeriod,
        float sunVisibility01,
        bool spawningEnabled,
        PlayerVitals& playerVitals,
        float playerDamageMultiplier = 1.0f,
        std::span<const glm::vec3> remotePlayerFeetForMultiTarget = {},
        std::span<float> remotePlayerHealthForMelee = {},
        float remotePlayerMaxHealth = 20.0f,
        float remotePlayerDamageMultiplier = 1.0f);

  private:
    MobSpawnSettings settings_{};
    std::vector<MobInstance> mobs_{};
    std::vector<HostileProjectile> projectiles_{};
    std::vector<MobCombatEvent> combatEvents_{};
    std::uint32_t nextId_ = 1;
    std::uint32_t nextProjectileId_ = 1;
    float hostileSpawnAccumulatorSeconds_ = 0.0f;
    float passiveSpawnAccumulatorSeconds_ = 0.0f;
    std::mt19937 rng_{};

    [[nodiscard]] bool trySpawnOneHostile(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        TimeOfDayPeriod timePeriod,
        std::span<const glm::vec3> remotePlayerFeet = {});

    [[nodiscard]] bool trySpawnOnePassive(
        const world::World& world,
        const world::TerrainGenerator& terrain,
        const glm::vec3& playerFeet,
        float playerHalfWidth,
        TimeOfDayPeriod timePeriod,
        std::span<const glm::vec3> remotePlayerFeet = {});

    void despawnDistant(const glm::vec3& hostFeet, std::span<const glm::vec3> remotePlayerFeet);
    void moveMobAxis(
        const world::World& world,
        MobInstance& mob,
        int axisIndex,
        float displacement);
    void applyMelee(
        MobInstance& mob,
        const glm::vec3& mobAttackOrigin,
        const glm::vec3& hostPlayerFeet,
        PlayerVitals& playerVitals,
        float hostDamageMultiplier,
        std::span<const glm::vec3> remotePlayerFeet,
        std::span<float> remotePlayerHealth,
        float remotePlayerMaxHealth,
        float remotePlayerDamageMultiplier);
    void tickProjectiles(
        const world::World& world,
        float playerHalfWidth,
        float deltaSeconds,
        const glm::vec3& hostPlayerFeet,
        PlayerVitals& playerVitals,
        float hostDamageMultiplier,
        std::span<const glm::vec3> remotePlayerFeet,
        std::span<float> remotePlayerHealth,
        float remotePlayerMaxHealth,
        float remotePlayerDamageMultiplier);

    void applyDaylightBurn(
        const world::World& world,
        float sunVisibility01,
        float deltaSeconds);

    float daylightBurnSoundCooldownSeconds_ = 0.0f;
};
}  // namespace vibecraft::game
