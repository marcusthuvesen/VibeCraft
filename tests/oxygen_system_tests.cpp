#include <doctest/doctest.h>

#include <filesystem>
#include <optional>

#include "vibecraft/app/ApplicationOxygenRuntime.hpp"
#include "vibecraft/app/SingleplayerSave.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"
#include "vibecraft/game/OxygenSystem.hpp"
#include "vibecraft/game/PlayerVitals.hpp"

TEST_CASE("oxygen system drains outside safe zones and damages health when depleted")
{
    vibecraft::game::PlayerVitals vitals;
    vibecraft::game::OxygenSystem oxygenSystem({
        .baseDrainPerSecond = 10.0f,
        .depletedHealthDamagePerSecond = 2.5f,
    });

    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Starter, 0.1f);
    const auto drainResult = oxygenSystem.tick(1.0f, {.insideSafeZone = false, .drainMultiplier = 1.0f}, &vitals);
    CHECK(drainResult.oxygenAfter < drainResult.oxygenBefore);

    static_cast<void>(oxygenSystem.consume(oxygenSystem.state().oxygen));
    const float healthBefore = vitals.health();
    const auto depletedResult =
        oxygenSystem.tick(2.0f, {.insideSafeZone = false, .drainMultiplier = 1.0f}, &vitals);
    CHECK(depletedResult.depleted);
    CHECK(depletedResult.healthDamageApplied == doctest::Approx(5.0f));
    CHECK(vitals.health() == doctest::Approx(healthBefore - 5.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::OxygenDepletion);
}

TEST_CASE("oxygen system refills inside safe zones using tank defaults")
{
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Field, 0.25f);

    const float oxygenBefore = oxygenSystem.state().oxygen;
    const auto refillResult = oxygenSystem.tick(2.0f, {.insideSafeZone = true}, nullptr);
    CHECK(refillResult.insideSafeZone);
    CHECK(refillResult.oxygenAfter > oxygenBefore);
}

TEST_CASE("oxygen groves count as breathable refill zones")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    std::optional<glm::vec3> jungleFeetPosition;
    for (int worldX = -16384; worldX <= 16384 && !jungleFeetPosition.has_value(); worldX += 96)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 96)
        {
            if (terrainGenerator.surfaceBiomeAt(worldX, worldZ) != vibecraft::world::SurfaceBiome::Jungle)
            {
                continue;
            }

            jungleFeetPosition = glm::vec3(
                static_cast<float>(worldX) + 0.5f,
                static_cast<float>(terrainGenerator.surfaceHeightAt(worldX, worldZ) + 1),
                static_cast<float>(worldZ) + 0.5f);
            break;
        }
    }

    REQUIRE(jungleFeetPosition.has_value());

    const auto environment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        *jungleFeetPosition,
        {},
        false);
    CHECK(environment.insideSafeZone);
    CHECK(environment.drainMultiplier == doctest::Approx(0.0f));
    CHECK(environment.safeZoneRefillPerSecond > 0.0f);
}

TEST_CASE("submerged players lose oxygen safety even inside grove biomes")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    std::optional<glm::vec3> jungleFeetPosition;
    for (int worldX = -16384; worldX <= 16384 && !jungleFeetPosition.has_value(); worldX += 96)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 96)
        {
            if (terrainGenerator.surfaceBiomeAt(worldX, worldZ) != vibecraft::world::SurfaceBiome::Jungle)
            {
                continue;
            }

            jungleFeetPosition = glm::vec3(
                static_cast<float>(worldX) + 0.5f,
                static_cast<float>(terrainGenerator.surfaceHeightAt(worldX, worldZ) + 1),
                static_cast<float>(worldZ) + 0.5f);
            break;
        }
    }

    REQUIRE(jungleFeetPosition.has_value());

    const auto environment = vibecraft::app::sampleOxygenEnvironment(
        world,
        terrainGenerator,
        *jungleFeetPosition,
        {.bodyInWater = true, .headSubmergedInWater = true},
        false);
    CHECK_FALSE(environment.insideSafeZone);
    CHECK(environment.safeZoneRefillPerSecond == doctest::Approx(0.0f));
    CHECK(environment.drainMultiplier >= doctest::Approx(2.35f));
}

TEST_CASE("dust flats drain oxygen faster than regolith plains")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;

    std::optional<glm::vec3> temperateFeetPosition;
    std::optional<glm::vec3> sandyFeetPosition;
    for (int worldX = -16384; worldX <= 16384
            && (!temperateFeetPosition.has_value() || !sandyFeetPosition.has_value());
         worldX += 96)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 96)
        {
            const auto biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome == vibecraft::world::SurfaceBiome::TemperateGrassland && !temperateFeetPosition.has_value())
            {
                temperateFeetPosition = glm::vec3(
                    static_cast<float>(worldX) + 0.5f,
                    static_cast<float>(terrainGenerator.surfaceHeightAt(worldX, worldZ) + 1),
                    static_cast<float>(worldZ) + 0.5f);
            }
            else if (biome == vibecraft::world::SurfaceBiome::Sandy && !sandyFeetPosition.has_value())
            {
                sandyFeetPosition = glm::vec3(
                    static_cast<float>(worldX) + 0.5f,
                    static_cast<float>(terrainGenerator.surfaceHeightAt(worldX, worldZ) + 1),
                    static_cast<float>(worldZ) + 0.5f);
            }
        }
    }

    REQUIRE(temperateFeetPosition.has_value());
    REQUIRE(sandyFeetPosition.has_value());

    const auto temperateEnvironment =
        vibecraft::app::sampleOxygenEnvironment(world, terrainGenerator, *temperateFeetPosition, {}, false);
    const auto sandyEnvironment =
        vibecraft::app::sampleOxygenEnvironment(world, terrainGenerator, *sandyFeetPosition, {}, false);
    CHECK(sandyEnvironment.drainMultiplier > temperateEnvironment.drainMultiplier);
}

TEST_CASE("singleplayer saves preserve oxygen state")
{
    vibecraft::app::SingleplayerPlayerState state;
    state.oxygenState.tankTier = vibecraft::game::OxygenTankTier::Expedition;
    state.oxygenState.capacity = 140.0f;
    state.oxygenState.oxygen = 77.5f;

    const std::filesystem::path outputPath =
        std::filesystem::temp_directory_path() / "vibecraft_oxygen_state_test.bin";
    REQUIRE(vibecraft::app::SingleplayerSaveSerializer::savePlayerState(state, outputPath));

    const auto loadedState = vibecraft::app::SingleplayerSaveSerializer::loadPlayerState(outputPath);
    REQUIRE(loadedState.has_value());
    CHECK(loadedState->oxygenState.tankTier == vibecraft::game::OxygenTankTier::Expedition);
    CHECK(loadedState->oxygenState.capacity == doctest::Approx(140.0f));
    CHECK(loadedState->oxygenState.oxygen == doctest::Approx(77.5f));

    std::error_code errorCode;
    std::filesystem::remove(outputPath, errorCode);
}

TEST_CASE("legacy multiplayer air field can carry oxygen tank tier and fill ratio")
{
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Field, 0.5f);

    const float encodedAir = vibecraft::app::encodeLegacyNetworkAir(oxygenSystem.state());
    CHECK(encodedAir == doctest::Approx(205.0f));

    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Starter, 1.0f);
    vibecraft::app::applyLegacyNetworkAirToOxygenSystem(oxygenSystem, encodedAir);
    CHECK(oxygenSystem.state().tankTier == vibecraft::game::OxygenTankTier::Field);
    CHECK(oxygenSystem.state().oxygen == doctest::Approx(oxygenSystem.state().capacity * 0.5f));
}

TEST_CASE("legacy multiplayer air decoder preserves tank tier for old fill-only values")
{
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Expedition, 1.0f);

    vibecraft::app::applyLegacyNetworkAirToOxygenSystem(oxygenSystem, 5.0f);
    CHECK(oxygenSystem.state().tankTier == vibecraft::game::OxygenTankTier::Expedition);
    CHECK(oxygenSystem.state().oxygen == doctest::Approx(oxygenSystem.state().capacity * 0.5f));
}

TEST_CASE("player survival oxygen tick reports damage in hostile conditions")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::game::PlayerVitals vitals;
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Starter, 0.0f);

    const auto result = vibecraft::app::tickPlayerSurvivalOxygen(
        1.0f,
        world,
        terrainGenerator,
        glm::vec3(32.0f, 64.0f, 32.0f),
        {.bodyInLava = true},
        false,
        vitals,
        oxygenSystem);

    CHECK(result.playerTookDamage);
    CHECK_FALSE(result.oxygenEnvironment.insideSafeZone);
    CHECK(vitals.health() < vitals.maxHealth());
}

TEST_CASE("player survival oxygen tick keeps creative mode fully topped off")
{
    vibecraft::world::World world;
    vibecraft::world::TerrainGenerator terrainGenerator;
    vibecraft::game::PlayerVitals vitals;
    static_cast<void>(vitals.applyDamage({
        .cause = vibecraft::game::DamageCause::EnemyAttack,
        .amount = 4.0f,
    }));
    vibecraft::game::OxygenSystem oxygenSystem;
    oxygenSystem.resetForNewGame(vibecraft::game::OxygenTankTier::Field, 0.2f);

    const auto result = vibecraft::app::tickPlayerSurvivalOxygen(
        1.0f,
        world,
        terrainGenerator,
        glm::vec3(0.0f, 64.0f, 0.0f),
        {},
        true,
        vitals,
        oxygenSystem);

    CHECK_FALSE(result.playerTookDamage);
    CHECK(result.oxygenEnvironment.insideSafeZone);
    CHECK(vitals.health() == doctest::Approx(vitals.maxHealth()));
    CHECK(oxygenSystem.state().oxygen == doctest::Approx(oxygenSystem.state().capacity));
}

TEST_CASE("early survival tips are present then expire")
{
    CHECK_FALSE(vibecraft::app::survivalTipLine(0.0f).empty());
    CHECK(vibecraft::app::survivalTipLine(400.0f).empty());
}
