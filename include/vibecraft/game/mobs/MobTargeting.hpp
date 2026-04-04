#pragma once

#include "vibecraft/game/MobTypes.hpp"
#include "vibecraft/game/PlayerVitals.hpp"

#include <glm/vec3.hpp>

#include <cstddef>
#include <optional>
#include <span>

namespace vibecraft::game
{
struct LivingPlayerTarget
{
    glm::vec3 feet{};
    /// 0 = host vitals, `1 + i` = remote index `i` in parallel feet/health spans.
    std::size_t index = 0;
};

[[nodiscard]] bool horizontalDistLessThan(
    float ax,
    float az,
    float bx,
    float bz,
    float threshold);

[[nodiscard]] float horizontalDistSqXZ(const glm::vec3& a, const glm::vec3& b);

[[nodiscard]] std::optional<LivingPlayerTarget> findNearestLivingPlayer(
    const glm::vec3& mobFeet,
    const glm::vec3& hostFeet,
    const PlayerVitals& hostVitals,
    bool multiTarget,
    std::span<const glm::vec3> remoteFeet,
    std::span<const float> remoteHealth);

[[nodiscard]] glm::vec3 nearestPlayerFeetForFacing(
    const glm::vec3& candidateFeet,
    const glm::vec3& hostFeet,
    std::span<const glm::vec3> remoteFeet);
}  // namespace vibecraft::game
