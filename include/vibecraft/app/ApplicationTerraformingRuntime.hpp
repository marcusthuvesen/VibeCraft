#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>

#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct TerraformingRuntimeState
{
    float pulseAccumulatorSeconds = 0.0f;
    std::uint32_t pulseCounter = 0;
};

struct TerraformingTickResult
{
    std::size_t blocksChanged = 0;
};

[[nodiscard]] TerraformingTickResult tickLocalTerraforming(
    float deltaTimeSeconds,
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::vec3& referencePosition,
    TerraformingRuntimeState& runtimeState);
}  // namespace vibecraft::app
