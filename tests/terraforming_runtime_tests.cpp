#include <doctest/doctest.h>

#include <glm/vec2.hpp>

#include <optional>

#include "vibecraft/app/ApplicationTerraformingRuntime.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

namespace
{
[[nodiscard]] bool hasSurfaceBlockNear(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::ivec2& center,
    const int radius,
    const vibecraft::world::BlockType targetBlock)
{
    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int worldX = center.x + dx;
            const int worldZ = center.y + dz;
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (world.blockAt(worldX, surfaceY, worldZ) == targetBlock)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] int firstOpenYAboveSurface(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ)
{
    const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
    for (int y = surfaceY + 1; y <= surfaceY + 6; ++y)
    {
        if (world.blockAt(worldX, y, worldZ) == vibecraft::world::BlockType::Air)
        {
            return y;
        }
    }
    return surfaceY + 1;
}

[[nodiscard]] std::optional<glm::ivec3> findRelayPlacementNear(
    const vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::ivec2& center,
    const int radius)
{
    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int worldX = center.x + dx;
            const int worldZ = center.y + dz;
            const int relayY = firstOpenYAboveSurface(world, terrainGenerator, worldX, worldZ);
            if (world.blockAt(worldX, relayY, worldZ) == vibecraft::world::BlockType::Air)
            {
                return glm::ivec3(worldX, relayY, worldZ);
            }
        }
    }
    return std::nullopt;
}

void paintSurfacePatch(
    vibecraft::world::World& world,
    const vibecraft::world::TerrainGenerator& terrainGenerator,
    const glm::ivec2& center,
    const int radius,
    const vibecraft::world::BlockType blockType)
{
    for (int dz = -radius; dz <= radius; ++dz)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            const int worldX = center.x + dx;
            const int worldZ = center.y + dz;
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (world.blockAt(worldX, surfaceY, worldZ) != blockType)
            {
                CHECK(world.applyEditCommand({
                    .action = vibecraft::world::WorldEditAction::Place,
                    .position = {worldX, surfaceY, worldZ},
                    .blockType = blockType,
                }));
            }
            if (world.blockAt(worldX, surfaceY + 1, worldZ) != vibecraft::world::BlockType::Air)
            {
                CHECK(world.applyEditCommand({
                    .action = vibecraft::world::WorldEditAction::Remove,
                    .position = {worldX, surfaceY + 1, worldZ},
                    .blockType = vibecraft::world::BlockType::Air,
                }));
            }
        }
    }
}
}  // namespace

TEST_CASE("relay zones slowly terraform red dust into fertile soil")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    const glm::ivec2 patchCenter{0, 0};
    paintSurfacePatch(world, terrainGenerator, patchCenter, 3, vibecraft::world::BlockType::Sand);

    const int surfaceY = terrainGenerator.surfaceHeightAt(patchCenter.x, patchCenter.y);
    const auto relayPlacement = findRelayPlacementNear(world, terrainGenerator, patchCenter, 1);
    REQUIRE(relayPlacement.has_value());
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = *relayPlacement,
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    vibecraft::app::TerraformingRuntimeState runtimeState{};
    const vibecraft::app::TerraformingTickResult tickResult = vibecraft::app::tickLocalTerraforming(
        3.2f,
        world,
        terrainGenerator,
        glm::vec3(static_cast<float>(patchCenter.x), static_cast<float>(surfaceY), static_cast<float>(patchCenter.y)),
        runtimeState);

    CHECK(tickResult.blocksChanged > 0);
    CHECK(hasSurfaceBlockNear(world, terrainGenerator, patchCenter, 3, vibecraft::world::BlockType::Dirt));
}

TEST_CASE("stacked relays can push restored ground into oxygen moss")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    const glm::ivec2 patchCenter{8, 8};
    paintSurfacePatch(world, terrainGenerator, patchCenter, 4, vibecraft::world::BlockType::Grass);

    const int surfaceY = terrainGenerator.surfaceHeightAt(patchCenter.x, patchCenter.y);
    const auto relayPlacement0 = findRelayPlacementNear(world, terrainGenerator, patchCenter, 1);
    const auto relayPlacement1 = findRelayPlacementNear(world, terrainGenerator, {patchCenter.x + 4, patchCenter.y}, 1);
    REQUIRE(relayPlacement0.has_value());
    REQUIRE(relayPlacement1.has_value());
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = *relayPlacement0,
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));
    CHECK(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = *relayPlacement1,
        .blockType = vibecraft::world::BlockType::OxygenGenerator,
    }));

    vibecraft::app::TerraformingRuntimeState runtimeState{};
    const vibecraft::app::TerraformingTickResult tickResult = vibecraft::app::tickLocalTerraforming(
        3.2f,
        world,
        terrainGenerator,
        glm::vec3(static_cast<float>(patchCenter.x), static_cast<float>(surfaceY), static_cast<float>(patchCenter.y)),
        runtimeState);

    CHECK(tickResult.blocksChanged > 0);
    CHECK(hasSurfaceBlockNear(world, terrainGenerator, patchCenter, 4, vibecraft::world::BlockType::JungleGrass));
}
