#include "vibecraft/game/mobs/MobBreeding.hpp"

#include "vibecraft/game/mobs/MobSpecies.hpp"
#include "vibecraft/game/mobs/MobTargeting.hpp"

#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

namespace vibecraft::game
{
namespace
{
[[nodiscard]] bool isAdultReadyToBreed(const MobInstance& mob)
{
    return isBreedablePassiveMobKind(mob.kind) && mob.growthSecondsRemaining <= 0.0f && mob.breedCooldownSeconds <= 0.0f;
}

[[nodiscard]] std::size_t countPassiveMobs(const std::vector<MobInstance>& mobs)
{
    std::size_t count = 0;
    for (const MobInstance& mob : mobs)
    {
        if (isPassiveMob(mob.kind))
        {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] std::size_t countNearbySameKind(
    const std::vector<MobInstance>& mobs,
    const MobKind kind,
    const glm::vec3& centerFeet,
    const float radius)
{
    std::size_t count = 0;
    for (const MobInstance& mob : mobs)
    {
        if (mob.kind != kind)
        {
            continue;
        }
        const glm::vec3 mobFeet{mob.feetX, mob.feetY, mob.feetZ};
        if (horizontalDistSqXZ(mobFeet, centerFeet) <= radius * radius)
        {
            ++count;
        }
    }
    return count;
}
}  // namespace

void tickPassiveMobLifecycle(
    std::vector<MobInstance>& mobs,
    const MobSpawnSettings& settings,
    const float deltaSeconds)
{
    if (deltaSeconds <= 0.0f)
    {
        return;
    }

    const float babyGrowSeconds = std::max(settings.passiveBabyGrowSeconds, 0.001f);
    const float babyScale = std::clamp(settings.passiveBabyScale, 0.2f, 0.95f);
    for (MobInstance& mob : mobs)
    {
        if (!isPassiveMob(mob.kind))
        {
            continue;
        }

        mob.breedCooldownSeconds = std::max(0.0f, mob.breedCooldownSeconds - deltaSeconds);
        mob.growthSecondsRemaining = std::max(0.0f, mob.growthSecondsRemaining - deltaSeconds);

        const float growthProgress = 1.0f - (mob.growthSecondsRemaining / babyGrowSeconds);
        const float scale = mob.growthSecondsRemaining > 0.0f
            ? std::lerp(babyScale, 1.0f, std::clamp(growthProgress, 0.0f, 1.0f))
            : 1.0f;
        const MobDimensions dims = scaledMobDimensions(mob.kind, scale);
        mob.halfWidth = dims.halfWidth;
        mob.height = dims.height;
    }
}

void tickPassiveBreeding(
    std::vector<MobInstance>& mobs,
    const MobSpawnSettings& settings,
    std::mt19937& rng,
    const float deltaSeconds,
    std::uint32_t& nextId)
{
    if (deltaSeconds <= 0.0f
        || settings.passiveBreedChancePerSecond <= 0.0f
        || settings.passiveBreedRange <= 0.0f
        || settings.maxPassiveMobsAfterBreeding == 0)
    {
        return;
    }

    std::size_t passiveCount = countPassiveMobs(mobs);
    if (passiveCount >= settings.maxPassiveMobsAfterBreeding)
    {
        return;
    }

    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    const float breedChance = std::clamp(settings.passiveBreedChancePerSecond * deltaSeconds, 0.0f, 1.0f);
    const float breedRangeSq = settings.passiveBreedRange * settings.passiveBreedRange;
    const float crowdRadius = std::max(settings.passiveBreedCrowdRadius, settings.passiveBreedRange);

    std::vector<bool> reserved(mobs.size(), false);
    std::vector<MobInstance> newborns;
    newborns.reserve(2);

    for (std::size_t i = 0; i < mobs.size(); ++i)
    {
        if (reserved[i] || !isAdultReadyToBreed(mobs[i]))
        {
            continue;
        }

        const glm::vec3 parentFeetA{mobs[i].feetX, mobs[i].feetY, mobs[i].feetZ};
        std::optional<std::size_t> partnerIndex;
        float bestDistanceSq = breedRangeSq;

        for (std::size_t j = i + 1; j < mobs.size(); ++j)
        {
            if (reserved[j] || !isAdultReadyToBreed(mobs[j]) || mobs[j].kind != mobs[i].kind)
            {
                continue;
            }

            const glm::vec3 parentFeetB{mobs[j].feetX, mobs[j].feetY, mobs[j].feetZ};
            const float distanceSq = horizontalDistSqXZ(parentFeetA, parentFeetB);
            if (distanceSq > bestDistanceSq)
            {
                continue;
            }

            bestDistanceSq = distanceSq;
            partnerIndex = j;
        }

        if (!partnerIndex.has_value() || chanceDist(rng) > breedChance)
        {
            continue;
        }

        const std::size_t j = *partnerIndex;
        const glm::vec3 parentFeetB{mobs[j].feetX, mobs[j].feetY, mobs[j].feetZ};
        const glm::vec3 babyFeet = (parentFeetA + parentFeetB) * 0.5f;

        if (countNearbySameKind(mobs, mobs[i].kind, babyFeet, crowdRadius) >= settings.maxNearbySameKindForBreeding)
        {
            continue;
        }
        if (passiveCount + newborns.size() >= settings.maxPassiveMobsAfterBreeding)
        {
            break;
        }

        reserved[i] = true;
        reserved[j] = true;
        mobs[i].breedCooldownSeconds = settings.passiveBreedCooldownSeconds;
        mobs[j].breedCooldownSeconds = settings.passiveBreedCooldownSeconds;

        const MobDimensions babyDims = scaledMobDimensions(mobs[i].kind, settings.passiveBabyScale);
        newborns.push_back(MobInstance{
            .id = nextId++,
            .kind = mobs[i].kind,
            .feetX = babyFeet.x,
            .feetY = babyFeet.y,
            .feetZ = babyFeet.z,
            .yawRadians = chanceDist(rng) * 6.28318530718f,
            .attackCooldownSeconds = 0.0f,
            .wanderTimerSeconds = 0.0f,
            .wanderYawRadians = chanceDist(rng) * 6.28318530718f,
            .breedCooldownSeconds = 0.0f,
            .growthSecondsRemaining = settings.passiveBabyGrowSeconds,
            .health = mobKindDefaultMaxHealth(mobs[i].kind),
            .halfWidth = babyDims.halfWidth,
            .height = babyDims.height,
        });
    }

    if (!newborns.empty())
    {
        mobs.insert(mobs.end(), newborns.begin(), newborns.end());
    }
}
}  // namespace vibecraft::game
