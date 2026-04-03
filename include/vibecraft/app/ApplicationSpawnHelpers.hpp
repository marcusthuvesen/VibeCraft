#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/PlayerVitals.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
enum class SpawnPreset : std::uint8_t;
enum class SpawnBiomeTarget : std::uint8_t;

[[nodiscard]] std::uint32_t generateRandomWorldSeed();
[[nodiscard]] glm::vec3 resolveSpawnFeetPosition(
    vibecraft::world::World& worldState,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    SpawnPreset spawnPreset,
    SpawnBiomeTarget spawnBiomeTarget,
    const glm::vec3& fallbackCameraPosition,
    float colliderHeight);
[[nodiscard]] bool isGroundedAtFeetPosition(
    const vibecraft::world::World& worldState,
    const glm::vec3& feetPosition,
    float colliderHeight);
[[nodiscard]] vibecraft::game::EnvironmentalHazards samplePlayerHazards(
    const vibecraft::world::World& worldState,
    const glm::vec3& feetPosition,
    float colliderHeight,
    float eyeHeight);
void applyDefaultHotbarLoadout(HotbarSlots& hotbarSlots, std::size_t& selectedHotbarIndex);
}  // namespace vibecraft::app
