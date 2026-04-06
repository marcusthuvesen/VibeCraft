#include <doctest/doctest.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>

#include "vibecraft/game/MobSpawnSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/game/mobs/MobSpecies.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace
{
vibecraft::game::MobInstance makeTestMob(
    const vibecraft::game::MobKind kind,
    const std::uint32_t id,
    const glm::vec3& feetPosition)
{
    const vibecraft::game::MobDimensions dims =
        vibecraft::game::adultDimensionsForMobKind(kind);
    return vibecraft::game::MobInstance{
        .id = id,
        .kind = kind,
        .feetX = feetPosition.x,
        .feetY = feetPosition.y,
        .feetZ = feetPosition.z,
        .yawRadians = 0.0f,
        .pitchRadians = 0.0f,
        .attackCooldownSeconds = 0.0f,
        .wanderTimerSeconds = 0.0f,
        .wanderYawRadians = 0.0f,
        .breedCooldownSeconds = 0.0f,
        .growthSecondsRemaining = 0.0f,
        .health = vibecraft::game::mobKindDefaultMaxHealth(kind),
        .halfWidth = dims.halfWidth,
        .height = dims.height,
    };
}

void clearAirCorridor(
    vibecraft::world::World& world,
    const int minX,
    const int maxX,
    const int minY,
    const int maxY,
    const int minZ,
    const int maxZ)
{
    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                static_cast<void>(world.applyEditCommand(vibecraft::world::WorldEditCommand{
                    .action = vibecraft::world::WorldEditAction::Remove,
                    .position = glm::ivec3{x, y, z},
                    .blockType = vibecraft::world::BlockType::Air,
                }));
            }
        }
    }
}
}  // namespace

TEST_CASE("skeletons fire arrows when a player is visible in range")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrain;
    world.generateRadius(terrain, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.maxHostileMobsNearPlayer = 0;
    settings.maxPassiveMobsNearPlayer = 0;
    vibecraft::game::MobSpawnSystem sys(settings);
    vibecraft::game::PlayerVitals vitals;

    const int surfaceY = terrain.surfaceHeightAt(0, 0);
    const glm::vec3 skeletonFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    const glm::vec3 playerFeet{8.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    clearAirCorridor(world, -1, 10, surfaceY + 1, surfaceY + 4, -1, 1);
    sys.addMobForTests(makeTestMob(vibecraft::game::MobKind::Skeleton, 1, skeletonFeet));

    bool firedArrow = false;
    bool hitPlayer = false;
    const float healthBefore = vitals.health();
    for (int tick = 0; tick < 180; ++tick)
    {
        sys.tick(
            world,
            terrain,
            playerFeet,
            0.30f,
            0.05f,
            vibecraft::game::TimeOfDayPeriod::Night,
            0.0f,
            false,
            vitals);
        for (const vibecraft::game::MobCombatEvent& event : sys.takeCombatEvents())
        {
            if (event.actorKind == vibecraft::game::MobKind::Skeleton
                && event.type == vibecraft::game::MobCombatEventType::ProjectileFired)
            {
                firedArrow = true;
            }
            if (event.actorKind == vibecraft::game::MobKind::Skeleton
                && event.type == vibecraft::game::MobCombatEventType::ProjectileHitPlayer)
            {
                hitPlayer = true;
            }
        }
        if (firedArrow && hitPlayer)
        {
            break;
        }
    }

    CHECK(firedArrow);
    CHECK(hitPlayer);
    CHECK(vitals.health() < healthBefore);
}

TEST_CASE("spiders can land melee attacks when close enough to the player")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrain;
    world.generateRadius(terrain, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.maxHostileMobsNearPlayer = 0;
    settings.maxPassiveMobsNearPlayer = 0;
    vibecraft::game::MobSpawnSystem sys(settings);
    vibecraft::game::PlayerVitals vitals;

    const int surfaceY = terrain.surfaceHeightAt(0, 0);
    const glm::vec3 spiderFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    const glm::vec3 playerFeet{1.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    clearAirCorridor(world, -1, 3, surfaceY + 1, surfaceY + 3, -1, 1);
    sys.addMobForTests(makeTestMob(vibecraft::game::MobKind::Spider, 1, spiderFeet));

    bool spiderAttacked = false;
    const float healthBefore = vitals.health();
    for (int tick = 0; tick < 40; ++tick)
    {
        sys.tick(
            world,
            terrain,
            playerFeet,
            0.30f,
            0.05f,
            vibecraft::game::TimeOfDayPeriod::Night,
            0.0f,
            false,
            vitals);
        const std::vector<vibecraft::game::MobCombatEvent> events = sys.takeCombatEvents();
        spiderAttacked = std::any_of(
            events.begin(),
            events.end(),
            [](const vibecraft::game::MobCombatEvent& event)
            {
                return event.actorKind == vibecraft::game::MobKind::Spider
                    && event.type == vibecraft::game::MobCombatEventType::MeleeAttack;
            });
        if (spiderAttacked)
        {
            break;
        }
    }

    CHECK(spiderAttacked);
    CHECK(vitals.health() < healthBefore);
}

TEST_CASE("creepers arm fuse next to the player then explode and despawn")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrain;
    world.generateRadius(terrain, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.maxHostileMobsNearPlayer = 0;
    settings.maxPassiveMobsNearPlayer = 0;
    vibecraft::game::MobSpawnSystem sys(settings);
    vibecraft::game::PlayerVitals vitals;

    const int surfaceY = terrain.surfaceHeightAt(0, 0);
    const glm::vec3 creeperFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    const glm::vec3 playerFeet{2.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    clearAirCorridor(world, -1, 4, surfaceY + 1, surfaceY + 4, -1, 1);
    sys.addMobForTests(makeTestMob(vibecraft::game::MobKind::Creeper, 1, creeperFeet));

    bool sawExplosionEvent = false;
    for (int tick = 0; tick < 120; ++tick)
    {
        sys.tick(
            world,
            terrain,
            playerFeet,
            0.30f,
            0.05f,
            vibecraft::game::TimeOfDayPeriod::Night,
            0.0f,
            false,
            vitals);
        for (const vibecraft::game::MobCombatEvent& event : sys.takeCombatEvents())
        {
            if (event.type == vibecraft::game::MobCombatEventType::CreeperExplosion)
            {
                sawExplosionEvent = true;
            }
        }
        if (sawExplosionEvent && sys.mobs().empty())
        {
            break;
        }
    }

    CHECK(sawExplosionEvent);
    CHECK(sys.mobs().empty());
}

TEST_CASE("undead mobs burn in direct sunlight; creepers do not")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrain;
    world.generateRadius(terrain, 4);

    vibecraft::game::MobSpawnSettings settings;
    settings.maxHostileMobsNearPlayer = 0;
    settings.maxPassiveMobsNearPlayer = 0;
    vibecraft::game::MobSpawnSystem sys(settings);
    vibecraft::game::PlayerVitals vitals;
    // No living target: keeps hostiles from pathing into water/terrain while we test daylight burn.
    vitals.setHealthAndAir(0.0f, 0.0f);

    const int surfaceY = terrain.surfaceHeightAt(0, 0);
    const glm::vec3 zombieFeet{0.5f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    const glm::vec3 creeperFeet = zombieFeet + glm::vec3(4.0f, 0.0f, 0.0f);
    clearAirCorridor(world, -3, 6, surfaceY + 1, surfaceY + 10, -3, 3);

    sys.addMobForTests(makeTestMob(vibecraft::game::MobKind::Zombie, 1, zombieFeet));
    sys.addMobForTests(makeTestMob(vibecraft::game::MobKind::Creeper, 2, creeperFeet));

    const glm::vec3 farPlayerFeet{40.0f, static_cast<float>(surfaceY) + 1.0f, 0.5f};
    for (int i = 0; i < 24; ++i)
    {
        sys.tick(
            world,
            terrain,
            farPlayerFeet,
            0.30f,
            1.0f,
            vibecraft::game::TimeOfDayPeriod::Day,
            1.0f,
            false,
            vitals);
        static_cast<void>(sys.takeCombatEvents());
    }

    REQUIRE(sys.mobs().size() == 1);
    CHECK(sys.mobs().front().kind == vibecraft::game::MobKind::Creeper);
    CHECK(sys.mobs().front().health > 10.0f);
}
