#include <doctest/doctest.h>

#include <cstdint>
#include <set>

#include "vibecraft/world/World.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/biomes/BiomeBlending.hpp"
#include "vibecraft/world/biomes/FloraVariantRules.hpp"
#include "vibecraft/world/biomes/BiomeVariation.hpp"
#include "vibecraft/world/biomes/BiomeTransition.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"
#include "vibecraft/world/biomes/SurfaceVariantRules.hpp"

namespace
{
[[nodiscard]] bool isTreeLog(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    return blockType == BlockType::OakLog
        || blockType == BlockType::BirchLog
        || blockType == BlockType::DarkOakLog
        || blockType == BlockType::SpruceLog
        || blockType == BlockType::JungleLog;
}

[[nodiscard]] bool hasWideTrunk(const vibecraft::world::Chunk& chunk)
{
    using vibecraft::world::Chunk;
    for (int y = vibecraft::world::kWorldMinY; y <= vibecraft::world::kWorldMaxY; ++y)
    {
        for (int localZ = 0; localZ < Chunk::kSize - 1; ++localZ)
        {
            for (int localX = 0; localX < Chunk::kSize - 1; ++localX)
            {
                const auto a = chunk.blockAt(localX, y, localZ);
                if (!isTreeLog(a))
                {
                    continue;
                }
                if (chunk.blockAt(localX + 1, y, localZ) == a
                    && chunk.blockAt(localX, y, localZ + 1) == a
                    && chunk.blockAt(localX + 1, y, localZ + 1) == a)
                {
                    return true;
                }
            }
        }
    }
    return false;
}
}  // namespace

TEST_CASE("woodland biome variation is deterministic and diverse")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::BiomeVariationSample;
    using vibecraft::world::biomes::WoodlandVariant;
    using vibecraft::world::biomes::sampleBiomeVariation;

    const std::uint32_t worldSeed = 0x42f0a17u;
    const BiomeVariationSample a = sampleBiomeVariation(SurfaceBiome::Forest, 128, -384, worldSeed);
    const BiomeVariationSample b = sampleBiomeVariation(SurfaceBiome::Forest, 128, -384, worldSeed);

    CHECK(a.primaryVariant == b.primaryVariant);
    CHECK(a.lushness == doctest::Approx(b.lushness));
    CHECK(a.dryness == doctest::Approx(b.dryness));
    CHECK(a.roughness == doctest::Approx(b.roughness));
    CHECK(a.canopyDensity == doctest::Approx(b.canopyDensity));
    CHECK(a.hilliness == doctest::Approx(b.hilliness));
    CHECK(a.mountainness == doctest::Approx(b.mountainness));

    std::set<WoodlandVariant> variants;
    for (int worldX = -1536; worldX <= 1536; worldX += 64)
    {
        for (int worldZ = -1536; worldZ <= 1536; worldZ += 64)
        {
            variants.insert(sampleBiomeVariation(SurfaceBiome::Forest, worldX, worldZ, worldSeed).primaryVariant);
        }
    }

    CHECK(variants.count(WoodlandVariant::WoodlandCore) == 1);
    CHECK(variants.count(WoodlandVariant::DryClearing) == 1);
    CHECK(variants.count(WoodlandVariant::MossyHollow) == 1);
    CHECK(variants.count(WoodlandVariant::FernGrove) == 1);
    CHECK(variants.count(WoodlandVariant::WoodedHills) == 1);
    CHECK(variants.count(WoodlandVariant::WoodedMountains) == 1);
    CHECK(variants.size() >= 6);
}

TEST_CASE("woodland biome variation lifts hills and mountains instead of carving ravines")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::BiomeVariationSample;
    using vibecraft::world::biomes::WoodlandVariant;
    using vibecraft::world::biomes::localSurfaceHeightDelta;
    using vibecraft::world::biomes::sampleBiomeVariation;

    const std::uint32_t worldSeed = 0x42f0a17u;
    bool foundHills = false;
    bool foundMountains = false;
    double maxLift = 0.0;

    for (int worldX = -2048; worldX <= 2048; worldX += 32)
    {
        for (int worldZ = -2048; worldZ <= 2048; worldZ += 32)
        {
            const BiomeVariationSample variation = sampleBiomeVariation(SurfaceBiome::Forest, worldX, worldZ, worldSeed);
            const double heightDelta = localSurfaceHeightDelta(SurfaceBiome::Forest, variation, worldX, worldZ, worldSeed);
            maxLift = std::max(maxLift, heightDelta);
            foundHills = foundHills || variation.primaryVariant == WoodlandVariant::WoodedHills;
            foundMountains = foundMountains || variation.primaryVariant == WoodlandVariant::WoodedMountains;
        }
    }

    CHECK(foundHills);
    CHECK(foundMountains);
    CHECK(maxLift >= 3.0);
}

TEST_CASE("biome transition sampling is deterministic and detects borders")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::sampleBiomeTransition;

    const auto borderSampler = [](const int worldX, const int) {
        return worldX < 0 ? SurfaceBiome::Forest : SurfaceBiome::DarkForest;
    };
    const auto uniformSampler = [](const int, const int) { return SurfaceBiome::Forest; };

    const auto borderA = sampleBiomeTransition(SurfaceBiome::Forest, -2, 0, borderSampler);
    const auto borderB = sampleBiomeTransition(SurfaceBiome::Forest, -2, 0, borderSampler);
    const auto uniform = sampleBiomeTransition(SurfaceBiome::Forest, -64, 0, uniformSampler);

    CHECK(borderA.neighboringBiome == SurfaceBiome::DarkForest);
    CHECK(borderA.edgeStrength == doctest::Approx(borderB.edgeStrength));
    CHECK(borderA.edgeStrength > 0.0f);
    CHECK(uniform.edgeStrength == doctest::Approx(0.0f));
}

TEST_CASE("macro biome blending keeps stable centers and softens majority borders")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::kBiomeBlendNeighborCount;
    using vibecraft::world::biomes::selectBlendedSurfaceBiome;

    std::array<SurfaceBiome, kBiomeBlendNeighborCount> uniformForest{};
    uniformForest.fill(SurfaceBiome::Forest);
    CHECK(selectBlendedSurfaceBiome(SurfaceBiome::Forest, uniformForest) == SurfaceBiome::Forest);

    std::array<SurfaceBiome, kBiomeBlendNeighborCount> darkForestBorder{};
    darkForestBorder.fill(SurfaceBiome::DarkForest);
    darkForestBorder[8] = SurfaceBiome::Forest;
    darkForestBorder[9] = SurfaceBiome::Forest;
    CHECK(selectBlendedSurfaceBiome(SurfaceBiome::Forest, darkForestBorder) == SurfaceBiome::DarkForest);
}

TEST_CASE("strong biome edge softens forest surface variants")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::BiomeTransitionSample;
    using vibecraft::world::biomes::SurfaceVariantDecision;
    using vibecraft::world::biomes::biomeProfile;
    using vibecraft::world::biomes::softenSurfaceVariantForBiomeEdge;

    BlockType surfaceBlock = BlockType::MossBlock;
    BlockType subsurfaceBlock = BlockType::Dirt;
    SurfaceVariantDecision decision{
        .topsoilDepthDelta = 1,
    };

    softenSurfaceVariantForBiomeEdge(
        biomeProfile(SurfaceBiome::Forest),
        BiomeTransitionSample{
            .neighboringBiome = SurfaceBiome::Plains,
            .edgeStrength = 0.90f,
        },
        surfaceBlock,
        subsurfaceBlock,
        decision);

    CHECK(surfaceBlock == BlockType::Grass);
    CHECK(subsurfaceBlock == BlockType::Dirt);
    CHECK(decision.topsoilDepthDelta == 0);
}

TEST_CASE("strong biome edge tempers forest decor tuning")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::biomes::BiomeTransitionSample;
    using vibecraft::world::biomes::softenTemperateForestDecorForBiomeEdge;
    using vibecraft::world::biomes::temperateForestDecorProfile;

    const auto base = temperateForestDecorProfile(SurfaceBiome::Forest);
    const auto softened = softenTemperateForestDecorForBiomeEdge(
        SurfaceBiome::Forest,
        BiomeTransitionSample{
            .neighboringBiome = SurfaceBiome::Plains,
            .edgeStrength = 0.90f,
        },
        base);

    CHECK(softened.patchEnterThreshold > base.patchEnterThreshold);
    CHECK(softened.denseForestThreshold > base.denseForestThreshold);
    CHECK(softened.fernChanceDense < base.fernChanceDense);
    CHECK(softened.woodlandGroundPatchRollMax < base.woodlandGroundPatchRollMax);
}

TEST_CASE("natural forest biomes keep grassy tops with occasional moss accents")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;
    using vibecraft::world::biomes::isForestSurfaceBiome;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    bool sawGrass = false;
    bool sawMoss = false;
    bool sawEarthyPatch = false;

    for (int worldX = -4096; worldX <= 4096; worldX += 8)
    {
        for (int worldZ = -4096; worldZ <= 4096; worldZ += 8)
        {
            const SurfaceBiome biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (!isForestSurfaceBiome(biome))
            {
                continue;
            }
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const BlockType surfaceBlock = terrainGenerator.blockTypeAt(worldX, surfaceY, worldZ);
            sawGrass = sawGrass || surfaceBlock == BlockType::Grass;
            sawMoss = sawMoss || surfaceBlock == BlockType::MossBlock;
            sawEarthyPatch = sawEarthyPatch || surfaceBlock == BlockType::CoarseDirt || surfaceBlock == BlockType::Podzol;
        }
    }

    CHECK(sawGrass);
    CHECK(sawMoss);
    CHECK_FALSE(sawEarthyPatch);
}

TEST_CASE("starter forest keeps stone patches rare on normal terrain")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    int sampledForestColumns = 0;
    int stoneSurfaceColumns = 0;
    for (int worldX = -320; worldX <= 320; worldX += 4)
    {
        for (int worldZ = -320; worldZ <= 320; worldZ += 4)
        {
            if (terrainGenerator.surfaceBiomeAt(worldX, worldZ) != SurfaceBiome::Forest)
            {
                continue;
            }
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (surfaceY >= 100)
            {
                continue;
            }
            ++sampledForestColumns;
            if (terrainGenerator.blockTypeAt(worldX, surfaceY, worldZ) == BlockType::Stone)
            {
                ++stoneSurfaceColumns;
            }
        }
    }

    REQUIRE(sampledForestColumns > 1000);
    const float stoneRatio = static_cast<float>(stoneSurfaceColumns) / static_cast<float>(sampledForestColumns);
    CHECK(stoneRatio < 0.06f);
}

TEST_CASE("forest terrain no longer opens shallow surface ravines")
{
    using vibecraft::world::BlockType;
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    for (int worldX = -1024; worldX <= 1024; worldX += 8)
    {
        for (int worldZ = -1024; worldZ <= 1024; worldZ += 8)
        {
            const SurfaceBiome biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome != SurfaceBiome::Forest
                && biome != SurfaceBiome::FlowerForest
                && biome != SurfaceBiome::BirchForest
                && biome != SurfaceBiome::OldGrowthBirchForest
                && biome != SurfaceBiome::DarkForest
                && biome != SurfaceBiome::Taiga
                && biome != SurfaceBiome::OldGrowthSpruceTaiga
                && biome != SurfaceBiome::OldGrowthPineTaiga
                && biome != SurfaceBiome::SnowyTaiga)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            for (int depth = 1; depth <= 5; ++depth)
            {
                const BlockType blockType = terrainGenerator.blockTypeAt(worldX, surfaceY - depth, worldZ);
                CHECK(blockType != BlockType::Air);
            }
        }
    }
}

TEST_CASE("forest override avoids sheer local plateau steps")
{
    using vibecraft::world::TerrainGenerator;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Forest);

    int maxNeighborDelta = 0;
    for (int worldX = -384; worldX <= 384; worldX += 8)
    {
        for (int worldZ = -384; worldZ <= 384; worldZ += 8)
        {
            const int center = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            maxNeighborDelta = std::max(maxNeighborDelta, std::abs(center - terrainGenerator.surfaceHeightAt(worldX + 1, worldZ)));
            maxNeighborDelta = std::max(maxNeighborDelta, std::abs(center - terrainGenerator.surfaceHeightAt(worldX, worldZ + 1)));
        }
    }

    CHECK(maxNeighborDelta <= 7);
}

TEST_CASE("dark forest generation can produce wide trunk trees")
{
    using vibecraft::world::SurfaceBiome;
    using vibecraft::world::TerrainGenerator;
    using vibecraft::world::World;

    TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(SurfaceBiome::DarkForest);

    World world;
    world.setGenerationSeed(0x42f0a17u);
    world.generateMissingChunksAround(terrainGenerator, {0, 0}, 8);

    bool foundWideTrunk = false;
    for (const auto& [coord, chunk] : world.chunks())
    {
        static_cast<void>(coord);
        if (hasWideTrunk(chunk))
        {
            foundWideTrunk = true;
            break;
        }
    }

    CHECK(foundWideTrunk);
}
