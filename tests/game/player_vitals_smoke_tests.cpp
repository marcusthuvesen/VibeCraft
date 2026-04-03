#include <doctest/doctest.h>

#include "vibecraft/game/PlayerVitals.hpp"

TEST_CASE("player vitals use a Minecraft-style 20 health baseline")
{
    vibecraft::game::PlayerVitals vitals;
    CHECK(vitals.health() == doctest::Approx(20.0f));
    CHECK(vitals.maxHealth() == doctest::Approx(20.0f));
    CHECK(vitals.air() == doctest::Approx(10.0f));
    CHECK(vitals.maxAir() == doctest::Approx(10.0f));
}

TEST_CASE("player vitals apply fall damage only beyond three blocks")
{
    vibecraft::game::PlayerVitals vitals;

    CHECK(vitals.applyLandingImpact(3.0f, false) == doctest::Approx(0.0f));
    CHECK(vitals.applyLandingImpact(4.0f, false) == doctest::Approx(1.0f));
    CHECK(vitals.health() == doctest::Approx(19.0f));
}

TEST_CASE("player vitals ignore fall damage when landing in water")
{
    vibecraft::game::PlayerVitals vitals;

    CHECK(vitals.applyLandingImpact(18.0f, true) == doctest::Approx(0.0f));
    CHECK(vitals.health() == doctest::Approx(vitals.maxHealth()));
}

TEST_CASE("player vitals lose air underwater and start drowning after air is gone")
{
    vibecraft::game::PlayerVitals vitals;
    const vibecraft::game::EnvironmentalHazards underwater{
        .bodyInWater = true,
        .bodyInLava = false,
        .headSubmergedInWater = true,
    };

    vitals.tickEnvironment(10.0f, underwater);
    CHECK(vitals.air() == doctest::Approx(0.0f));
    CHECK(vitals.health() == doctest::Approx(vitals.maxHealth()));

    vitals.tickEnvironment(1.0f, underwater);
    CHECK(vitals.health() == doctest::Approx(18.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::Drowning);
}

TEST_CASE("player vitals take periodic lava damage and support future enemy damage")
{
    vibecraft::game::PlayerVitals vitals;
    const vibecraft::game::EnvironmentalHazards lava{
        .bodyInWater = false,
        .bodyInLava = true,
        .headSubmergedInWater = false,
    };

    vitals.tickEnvironment(0.5f, lava);
    CHECK(vitals.health() == doctest::Approx(16.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::Lava);

    CHECK(vitals.applyDamage({
        .cause = vibecraft::game::DamageCause::EnemyAttack,
        .amount = 3.0f,
    }) == doctest::Approx(3.0f));
    CHECK(vitals.health() == doctest::Approx(13.0f));
    CHECK(vitals.lastDamageCause() == vibecraft::game::DamageCause::EnemyAttack);
}
