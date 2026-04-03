#include <doctest/doctest.h>
#include <glm/vec3.hpp>

#include "vibecraft/app/ApplicationAmbientLife.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"

TEST_CASE("ambient bird flocks prefer lush biomes")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x2468ace0U);

    const auto jungleBirds = vibecraft::app::buildAmbientBirdHud(
        terrainGenerator,
        glm::vec3(0.0f, 90.0f, 0.0f),
        vibecraft::world::SurfaceBiome::Jungle,
        12.0f,
        0.0f,
        1.0f);
    CHECK(!jungleBirds.empty());

    const auto sandyBirds = vibecraft::app::buildAmbientBirdHud(
        terrainGenerator,
        glm::vec3(0.0f, 90.0f, 0.0f),
        vibecraft::world::SurfaceBiome::Desert,
        12.0f,
        0.0f,
        1.0f);
    // Sandy uses sparse bird activity; jungle should still dominate at the same viewpoint.
    CHECK(jungleBirds.size() > sandyBirds.size());
}
