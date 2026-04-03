#include <doctest/doctest.h>

#include "vibecraft/world/TerrainGenerator.hpp"

TEST_CASE("terrain pass gives major biomes readable primary surface materials")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;

    bool desertSawSand = false;
    bool forestSawGrass = false;
    bool taigaSawGrass = false;
    bool snowyTaigaSawSnowShelf = false;
    bool jungleSawLivingGround = false;

    const auto sampleBiome = [&](const SurfaceBiome biome) {
        TerrainGenerator terrainGenerator;
        terrainGenerator.setWorldSeed(0x42f0a17u);
        terrainGenerator.setBiomeOverride(biome);

        for (int worldX = -768; worldX <= 768; worldX += 8)
        {
            for (int worldZ = -768; worldZ <= 768; worldZ += 8)
            {
                const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
                const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surfaceY, worldZ);
                switch (biome)
                {
                case SurfaceBiome::Desert:
                    desertSawSand = desertSawSand || surfaceBlock == BlockType::Sand;
                    break;
                case SurfaceBiome::Forest:
                    forestSawGrass = forestSawGrass || surfaceBlock == BlockType::Grass;
                    break;
                case SurfaceBiome::Taiga:
                    taigaSawGrass = taigaSawGrass || surfaceBlock == BlockType::Grass
                        || surfaceBlock == BlockType::Podzol
                        || surfaceBlock == BlockType::CoarseDirt;
                    break;
                case SurfaceBiome::SnowyTaiga:
                    snowyTaigaSawSnowShelf = snowyTaigaSawSnowShelf || surfaceBlock == BlockType::SnowGrass;
                    break;
                case SurfaceBiome::Jungle:
                    jungleSawLivingGround = jungleSawLivingGround || surfaceBlock == BlockType::JungleGrass
                        || surfaceBlock == BlockType::MossBlock;
                    break;
                default:
                    break;
                }
            }
        }
    };

    for (const SurfaceBiome biome :
         {SurfaceBiome::Desert, SurfaceBiome::Forest, SurfaceBiome::Taiga, SurfaceBiome::SnowyTaiga, SurfaceBiome::Jungle})
    {
        sampleBiome(biome);
    }

    CHECK(desertSawSand);
    CHECK(forestSawGrass);
    CHECK(taigaSawGrass);
    CHECK(snowyTaigaSawSnowShelf);
    CHECK(jungleSawLivingGround);
}

TEST_CASE("starter region resolves to forest woodland by default")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    for (int worldX = -256; worldX <= 256; worldX += 32)
    {
        for (int worldZ = -256; worldZ <= 256; worldZ += 32)
        {
            CHECK(terrainGenerator.surfaceBiomeAt(worldX, worldZ) == vibecraft::world::SurfaceBiome::Forest);
        }
    }
}

TEST_CASE("forest-first biome pass discovers each woodland biome across a large sample")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    bool foundForest = false;
    bool foundBirchForest = false;
    bool foundDarkForest = false;
    bool foundTaiga = false;
    bool foundSnowyTaiga = false;
    bool foundPlains = false;
    bool foundDesert = false;
    bool foundJungle = false;

    for (int worldX = -24576; worldX <= 24576; worldX += 96)
    {
        for (int worldZ = -24576; worldZ <= 24576; worldZ += 96)
        {
            switch (terrainGenerator.surfaceBiomeAt(worldX, worldZ))
            {
            case vibecraft::world::SurfaceBiome::Forest:
                foundForest = true;
                break;
            case vibecraft::world::SurfaceBiome::BirchForest:
                foundBirchForest = true;
                break;
            case vibecraft::world::SurfaceBiome::DarkForest:
                foundDarkForest = true;
                break;
            case vibecraft::world::SurfaceBiome::Taiga:
                foundTaiga = true;
                break;
            case vibecraft::world::SurfaceBiome::SnowyTaiga:
                foundSnowyTaiga = true;
                break;
            case vibecraft::world::SurfaceBiome::Plains:
                foundPlains = true;
                break;
            case vibecraft::world::SurfaceBiome::Desert:
                foundDesert = true;
                break;
            case vibecraft::world::SurfaceBiome::Jungle:
            case vibecraft::world::SurfaceBiome::SparseJungle:
            case vibecraft::world::SurfaceBiome::BambooJungle:
                foundJungle = true;
                break;
            case vibecraft::world::SurfaceBiome::SnowyPlains:
                break;
            }
        }
    }

    CHECK(foundForest);
    CHECK(foundBirchForest);
    CHECK(foundDarkForest);
    CHECK(foundTaiga);
    CHECK(foundSnowyTaiga);
    CHECK(foundPlains);
    CHECK(foundDesert);
    CHECK(foundJungle);
}
