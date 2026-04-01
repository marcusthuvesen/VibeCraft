#pragma once

#include <vector>

#include <glm/vec3.hpp>

#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"

namespace vibecraft::app
{
[[nodiscard]] std::vector<render::FrameDebugData::WorldBirdHud> buildAmbientBirdHud(
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& cameraPosition,
    world::SurfaceBiome biome,
    float weatherTimeSeconds,
    float rainIntensity,
    float sunVisibility);
}  // namespace vibecraft::app
