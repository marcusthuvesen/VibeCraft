#include <doctest/doctest.h>

#include "vibecraft/game/mobs/MobBreeding.hpp"
#include "vibecraft/game/mobs/MobSpecies.hpp"

#include <random>
#include <vector>

TEST_CASE("passive breeding creates a baby and applies parent cooldown")
{
    vibecraft::game::MobSpawnSettings settings;
    settings.passiveBreedRange = 3.5f;
    settings.passiveBreedChancePerSecond = 1.0f;
    settings.passiveBreedCooldownSeconds = 300.0f;
    settings.passiveBabyGrowSeconds = 1200.0f;
    settings.passiveBabyScale = 0.58f;
    settings.maxPassiveMobsAfterBreeding = 4;
    settings.maxNearbySameKindForBreeding = 8;
    settings.passiveBreedCrowdRadius = 8.0f;

    const vibecraft::game::MobDimensions cowDims =
        vibecraft::game::adultDimensionsForMobKind(vibecraft::game::MobKind::Cow);
    std::vector<vibecraft::game::MobInstance> mobs{
        vibecraft::game::MobInstance{
            .id = 1,
            .kind = vibecraft::game::MobKind::Cow,
            .feetX = 0.0f,
            .feetY = 65.0f,
            .feetZ = 0.0f,
            .health = vibecraft::game::mobKindDefaultMaxHealth(vibecraft::game::MobKind::Cow),
            .halfWidth = cowDims.halfWidth,
            .height = cowDims.height,
        },
        vibecraft::game::MobInstance{
            .id = 2,
            .kind = vibecraft::game::MobKind::Cow,
            .feetX = 1.5f,
            .feetY = 65.0f,
            .feetZ = 0.2f,
            .health = vibecraft::game::mobKindDefaultMaxHealth(vibecraft::game::MobKind::Cow),
            .halfWidth = cowDims.halfWidth,
            .height = cowDims.height,
        },
    };

    std::mt19937 rng(12345u);
    std::uint32_t nextId = 10;
    vibecraft::game::tickPassiveBreeding(mobs, settings, rng, 1.0f, nextId);

    REQUIRE(mobs.size() == 3);
    CHECK(nextId == 11);
    CHECK(mobs[0].breedCooldownSeconds == doctest::Approx(settings.passiveBreedCooldownSeconds));
    CHECK(mobs[1].breedCooldownSeconds == doctest::Approx(settings.passiveBreedCooldownSeconds));

    const vibecraft::game::MobInstance& baby = mobs.back();
    CHECK(baby.kind == vibecraft::game::MobKind::Cow);
    CHECK(baby.growthSecondsRemaining == doctest::Approx(settings.passiveBabyGrowSeconds));
    CHECK(baby.halfWidth < cowDims.halfWidth);
    CHECK(baby.height < cowDims.height);
    CHECK(baby.feetX == doctest::Approx(0.75f));
}

TEST_CASE("baby passive mobs grow back to adult dimensions")
{
    vibecraft::game::MobSpawnSettings settings;
    settings.passiveBabyGrowSeconds = 1200.0f;
    settings.passiveBabyScale = 0.58f;

    const vibecraft::game::MobDimensions adultDims =
        vibecraft::game::adultDimensionsForMobKind(vibecraft::game::MobKind::Sheep);
    const vibecraft::game::MobDimensions babyDims =
        vibecraft::game::scaledMobDimensions(vibecraft::game::MobKind::Sheep, settings.passiveBabyScale);
    std::vector<vibecraft::game::MobInstance> mobs{
        vibecraft::game::MobInstance{
            .id = 4,
            .kind = vibecraft::game::MobKind::Sheep,
            .feetX = 0.0f,
            .feetY = 70.0f,
            .feetZ = 0.0f,
            .growthSecondsRemaining = settings.passiveBabyGrowSeconds,
            .health = vibecraft::game::mobKindDefaultMaxHealth(vibecraft::game::MobKind::Sheep),
            .halfWidth = babyDims.halfWidth,
            .height = babyDims.height,
        },
    };

    vibecraft::game::tickPassiveMobLifecycle(mobs, settings, settings.passiveBabyGrowSeconds * 0.5f);
    CHECK(mobs[0].growthSecondsRemaining == doctest::Approx(settings.passiveBabyGrowSeconds * 0.5f));
    CHECK(mobs[0].halfWidth > babyDims.halfWidth);
    CHECK(mobs[0].halfWidth < adultDims.halfWidth);

    vibecraft::game::tickPassiveMobLifecycle(mobs, settings, settings.passiveBabyGrowSeconds);
    CHECK(mobs[0].growthSecondsRemaining == doctest::Approx(0.0f));
    CHECK(mobs[0].halfWidth == doctest::Approx(adultDims.halfWidth));
    CHECK(mobs[0].height == doctest::Approx(adultDims.height));
}
