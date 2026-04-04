#include "vibecraft/game/mobs/MobTargeting.hpp"

#include <limits>

namespace vibecraft::game
{
bool horizontalDistLessThan(
    const float ax,
    const float az,
    const float bx,
    const float bz,
    const float threshold)
{
    const float dx = ax - bx;
    const float dz = az - bz;
    return (dx * dx + dz * dz) < threshold * threshold;
}

float horizontalDistSqXZ(const glm::vec3& a, const glm::vec3& b)
{
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

std::optional<LivingPlayerTarget> findNearestLivingPlayer(
    const glm::vec3& mobFeet,
    const glm::vec3& hostFeet,
    const PlayerVitals& hostVitals,
    const bool multiTarget,
    const std::span<const glm::vec3> remoteFeet,
    const std::span<const float> remoteHealth)
{
    std::optional<LivingPlayerTarget> best;
    float bestDistSq = std::numeric_limits<float>::infinity();

    if (!hostVitals.isDead())
    {
        bestDistSq = horizontalDistSqXZ(mobFeet, hostFeet);
        best = LivingPlayerTarget{hostFeet, 0};
    }

    if (multiTarget && remoteFeet.size() == remoteHealth.size())
    {
        for (std::size_t i = 0; i < remoteFeet.size(); ++i)
        {
            if (remoteHealth[i] <= 0.0f)
            {
                continue;
            }

            const float candidateDistSq = horizontalDistSqXZ(mobFeet, remoteFeet[i]);
            if (!best.has_value() || candidateDistSq < bestDistSq)
            {
                bestDistSq = candidateDistSq;
                best = LivingPlayerTarget{remoteFeet[i], 1 + i};
            }
        }
    }

    return best;
}

glm::vec3 nearestPlayerFeetForFacing(
    const glm::vec3& candidateFeet,
    const glm::vec3& hostFeet,
    const std::span<const glm::vec3> remoteFeet)
{
    glm::vec3 nearest = hostFeet;
    float bestDistance = horizontalDistSqXZ(candidateFeet, hostFeet);
    for (const glm::vec3& remoteFeetPosition : remoteFeet)
    {
        const float remoteDistance = horizontalDistSqXZ(candidateFeet, remoteFeetPosition);
        if (remoteDistance < bestDistance)
        {
            bestDistance = remoteDistance;
            nearest = remoteFeetPosition;
        }
    }

    return nearest;
}
}  // namespace vibecraft::game
