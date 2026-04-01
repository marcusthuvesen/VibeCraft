#include <doctest/doctest.h>

#include "vibecraft/world/TerrainGenerator.hpp"

TEST_CASE("terrain pass gives major biomes more than one readable surface material")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    bool sandySawSand = false;
    bool sandySawRockyFace = false;
    bool snowySawSnowShelf = false;
    bool snowySawExposedFace = false;
    bool jungleSawLivingGround = false;
    bool jungleSawWetOrCliffFace = false;

    for (int worldX = -384; worldX <= 384; worldX += 8)
    {
        for (int worldZ = -384; worldZ <= 384; worldZ += 8)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surfaceY, worldZ);
            switch (terrainGenerator.surfaceBiomeAt(worldX, worldZ))
            {
            case SurfaceBiome::Sandy:
                sandySawSand = sandySawSand || surfaceBlock == BlockType::Sand;
                sandySawRockyFace = sandySawRockyFace || surfaceBlock == BlockType::Sandstone
                    || surfaceBlock == BlockType::Gravel;
                break;
            case SurfaceBiome::Snowy:
                snowySawSnowShelf = snowySawSnowShelf || surfaceBlock == BlockType::SnowGrass;
                snowySawExposedFace = snowySawExposedFace || surfaceBlock == BlockType::Stone
                    || surfaceBlock == BlockType::Gravel;
                break;
            case SurfaceBiome::Jungle:
                jungleSawLivingGround = jungleSawLivingGround || surfaceBlock == BlockType::JungleGrass
                    || surfaceBlock == BlockType::MossBlock;
                jungleSawWetOrCliffFace = jungleSawWetOrCliffFace || surfaceBlock == BlockType::MossBlock
                    || surfaceBlock == BlockType::Stone;
                break;
            case SurfaceBiome::TemperateGrassland:
            default:
                break;
            }
        }
    }

    CHECK(sandySawSand);
    CHECK(sandySawRockyFace);
    CHECK(snowySawSnowShelf);
    CHECK(snowySawExposedFace);
    CHECK(jungleSawLivingGround);
    CHECK(jungleSawWetOrCliffFace);
}
