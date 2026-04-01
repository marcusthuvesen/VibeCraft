#include <doctest/doctest.h>

#include <string>

#include "vibecraft/app/Crafting.hpp"
#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("inventory labels present alien-world block names")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::app::InventorySlot;
    using vibecraft::world::BlockType;

    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Grass)) == "Regolith Turf");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::JungleGrass)) == "Oxygen Moss");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Sand)) == "Red Dust");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::DiamondOre)) == "Azure Crystal");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Glowstone)) == "Lumen Crystal");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::OxygenGenerator)) == "Atmos Relay");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatPanel)) == "Hab Panel");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatFloor)) == "Deck Plating");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatFrame)) == "Support Frame");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::AirlockPanel)) == "Airlock Panel");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::PowerConduit)) == "Power Conduit");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::GreenhouseGlass)) == "Greenhouse Glass");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::PlanterTray)) == "Planter Tray");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FiberSapling)) == "Fiber Sapling");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FiberSprout)) == "Fiber Sprout");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Torch)) == "Flare Lamp");
    CHECK(std::string(vibecraft::app::equippedItemLabel(EquippedItem::OxygenCanister)) == "Oxygen Canister");
    CHECK(std::string(vibecraft::app::equippedItemLabel(EquippedItem::FieldTank)) == "Field Tank");
    CHECK(std::string(vibecraft::app::equippedItemLabel(EquippedItem::ExpeditionTank)) == "Expedition Tank");
    CHECK(
        vibecraft::app::inventorySlotLabel(InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = EquippedItem::FieldTank,
        })
        == "Field Tank");
}

TEST_CASE("surface biome labels reflect the lush alien biome map")
{
    using vibecraft::world::SurfaceBiome;

    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::TemperateGrassland)) == "glow forest fringe");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Sandy)) == "dry wastes");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Snowy)) == "crystal expanse");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Jungle)) == "glow forest heart");
}

TEST_CASE("portable flare lamps can be crafted from lumen crystals")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::app::EquippedItem;
    using vibecraft::world::BlockType;

    CraftingGridSlots lampGrid{};
    lampGrid[0].blockType = BlockType::Glowstone;
    lampGrid[0].count = 1;
    lampGrid[3].blockType = BlockType::Air;
    lampGrid[3].equippedItem = EquippedItem::Stick;
    lampGrid[3].count = 1;

    const auto lampMatch = vibecraft::app::evaluateCraftingGrid(lampGrid, CraftingMode::Inventory2x2);
    REQUIRE(lampMatch.has_value());
    CHECK(lampMatch->output.blockType == BlockType::Torch);
    CHECK(lampMatch->output.count == 4);
}

TEST_CASE("starter habitat blocks can be crafted for Mars base building")
{
    using vibecraft::app::CraftingGridSlots;
    using vibecraft::app::CraftingMode;
    using vibecraft::world::BlockType;

    CraftingGridSlots panelGrid{};
    panelGrid[0] = {.blockType = BlockType::IronOre, .count = 1};
    panelGrid[1] = {.blockType = BlockType::Glass, .count = 1};
    panelGrid[3] = {.blockType = BlockType::Cobblestone, .count = 1};
    panelGrid[4] = {.blockType = BlockType::Cobblestone, .count = 1};
    const auto panelMatch = vibecraft::app::evaluateCraftingGrid(panelGrid, CraftingMode::Inventory2x2);
    REQUIRE(panelMatch.has_value());
    CHECK(panelMatch->output.blockType == BlockType::HabitatPanel);
    CHECK(panelMatch->output.count == 4);

    CraftingGridSlots frameGrid{};
    frameGrid[0] = {.blockType = BlockType::IronOre, .count = 1};
    frameGrid[3] = {.blockType = BlockType::Cobblestone, .count = 1};
    const auto frameMatch = vibecraft::app::evaluateCraftingGrid(frameGrid, CraftingMode::Inventory2x2);
    REQUIRE(frameMatch.has_value());
    CHECK(frameMatch->output.blockType == BlockType::HabitatFrame);
    CHECK(frameMatch->output.count == 4);

    CraftingGridSlots planterGrid{};
    planterGrid[0] = {.blockType = BlockType::HabitatFloor, .count = 1};
    planterGrid[1] = {.blockType = BlockType::MossBlock, .count = 1};
    planterGrid[3] = {.blockType = BlockType::Cobblestone, .count = 1};
    planterGrid[4] = {.blockType = BlockType::Glass, .count = 1};
    const auto planterMatch = vibecraft::app::evaluateCraftingGrid(planterGrid, CraftingMode::Inventory2x2);
    REQUIRE(planterMatch.has_value());
    CHECK(planterMatch->output.blockType == BlockType::PlanterTray);
    CHECK(planterMatch->output.count == 2);

    CraftingGridSlots saplingGrid{};
    saplingGrid[0] = {.blockType = BlockType::MossBlock, .count = 1};
    const auto saplingMatch = vibecraft::app::evaluateCraftingGrid(saplingGrid, CraftingMode::Inventory2x2);
    REQUIRE(saplingMatch.has_value());
    CHECK(saplingMatch->output.blockType == BlockType::FiberSapling);
    CHECK(saplingMatch->output.count == 1);

    CraftingGridSlots airlockGrid{};
    airlockGrid[0] = {.blockType = BlockType::HabitatPanel, .count = 1};
    airlockGrid[1] = {.blockType = BlockType::Glass, .count = 1};
    airlockGrid[3] = {.blockType = BlockType::HabitatFrame, .count = 1};
    airlockGrid[4] = {.blockType = BlockType::IronOre, .count = 1};
    const auto airlockMatch = vibecraft::app::evaluateCraftingGrid(airlockGrid, CraftingMode::Inventory2x2);
    REQUIRE(airlockMatch.has_value());
    CHECK(airlockMatch->output.blockType == BlockType::AirlockPanel);
    CHECK(airlockMatch->output.count == 2);

    CraftingGridSlots conduitGrid{};
    conduitGrid[0] = {.blockType = BlockType::Glowstone, .count = 1};
    conduitGrid[3] = {.blockType = BlockType::IronOre, .count = 1};
    const auto conduitMatch = vibecraft::app::evaluateCraftingGrid(conduitGrid, CraftingMode::Inventory2x2);
    REQUIRE(conduitMatch.has_value());
    CHECK(conduitMatch->output.blockType == BlockType::PowerConduit);
    CHECK(conduitMatch->output.count == 4);
}

TEST_CASE("terrain generator still exposes all expedition biome families")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundRegolithPlains = false;
    bool foundDustFlats = false;
    bool foundIceShelf = false;
    bool foundOxygenGrove = false;

    for (int worldX = -16384; worldX <= 16384; worldX += 96)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 96)
        {
            switch (terrainGenerator.surfaceBiomeAt(worldX, worldZ))
            {
            case vibecraft::world::SurfaceBiome::TemperateGrassland:
                foundRegolithPlains = true;
                break;
            case vibecraft::world::SurfaceBiome::Sandy:
                foundDustFlats = true;
                break;
            case vibecraft::world::SurfaceBiome::Snowy:
                foundIceShelf = true;
                break;
            case vibecraft::world::SurfaceBiome::Jungle:
                foundOxygenGrove = true;
                break;
            }

            if (foundRegolithPlains && foundDustFlats && foundIceShelf && foundOxygenGrove)
            {
                break;
            }
        }

        if (foundRegolithPlains && foundDustFlats && foundIceShelf && foundOxygenGrove)
        {
            break;
        }
    }

    CHECK(foundRegolithPlains);
    CHECK(foundDustFlats);
    CHECK(foundIceShelf);
    CHECK(foundOxygenGrove);
}

TEST_CASE("generated world decorates exposed terrain with lumen crystal outcrops")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 8);

    bool foundCrystalOutcrop = false;
    for (int worldX = -128; worldX <= 128 && !foundCrystalOutcrop; ++worldX)
    {
        for (int worldZ = -128; worldZ <= 128 && !foundCrystalOutcrop; ++worldZ)
        {
            const auto biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome == vibecraft::world::SurfaceBiome::Jungle)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundCrystalOutcrop =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::Glowstone;
        }
    }

    CHECK(foundCrystalOutcrop);
}

TEST_CASE("generated crystal expanses can form multi-block crystal spires")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 12);

    bool foundCrystalSpire = false;
    for (int worldX = -192; worldX <= 192 && !foundCrystalSpire; ++worldX)
    {
        for (int worldZ = -192; worldZ <= 192 && !foundCrystalSpire; ++worldZ)
        {
            if (terrainGenerator.surfaceBiomeAt(worldX, worldZ) != vibecraft::world::SurfaceBiome::Snowy)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (world.blockAt(worldX, surfaceY + 1, worldZ) != vibecraft::world::BlockType::Glowstone)
            {
                continue;
            }

            foundCrystalSpire = world.blockAt(worldX, surfaceY + 2, worldZ) == vibecraft::world::BlockType::Glowstone;
        }
    }

    CHECK(foundCrystalSpire);
}

TEST_CASE("generated regolith plains expose ferrite outcrops for starter relay crafting")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 8);

    bool foundFerriteOutcrop = false;
    for (int worldX = -128; worldX <= 128 && !foundFerriteOutcrop; ++worldX)
    {
        for (int worldZ = -128; worldZ <= 128 && !foundFerriteOutcrop; ++worldZ)
        {
            const auto biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome != vibecraft::world::SurfaceBiome::TemperateGrassland)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundFerriteOutcrop =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::IronOre;
        }
    }

    CHECK(foundFerriteOutcrop);
}

TEST_CASE("generated dust flats can surface gravel lag deposits")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 12);

    bool foundDustGravel = false;
    for (int worldX = -256; worldX <= 256 && !foundDustGravel; worldX += 2)
    {
        for (int worldZ = -256; worldZ <= 256 && !foundDustGravel; worldZ += 2)
        {
            const auto biome = terrainGenerator.surfaceBiomeAt(worldX, worldZ);
            if (biome != vibecraft::world::SurfaceBiome::Sandy)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            if (world.blockAt(worldX, surfaceY, worldZ) == vibecraft::world::BlockType::Gravel)
            {
                foundDustGravel = true;
            }
        }
    }

    CHECK(foundDustGravel);
}

TEST_CASE("generated dry wastes can raise sandstone ridge outcrops")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 12);

    bool foundSandstoneOutcrop = false;
    for (int worldX = -192; worldX <= 192 && !foundSandstoneOutcrop; ++worldX)
    {
        for (int worldZ = -192; worldZ <= 192 && !foundSandstoneOutcrop; ++worldZ)
        {
            if (terrainGenerator.surfaceBiomeAt(worldX, worldZ) != vibecraft::world::SurfaceBiome::Sandy)
            {
                continue;
            }

            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundSandstoneOutcrop =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::Sandstone;
        }
    }

    CHECK(foundSandstoneOutcrop);
}

TEST_CASE("lumen crystals remain breakable field resources")
{
    using vibecraft::world::BlockType;

    const vibecraft::world::BlockMetadata crystalMetadata = vibecraft::world::blockMetadata(BlockType::Glowstone);
    CHECK(crystalMetadata.breakable);
    CHECK(crystalMetadata.hardness == doctest::Approx(0.9f));
}

TEST_CASE("starter habitat blocks use reserved atlas tail tiles")
{
    using vibecraft::world::BlockFace;
    using vibecraft::world::BlockType;

    CHECK(vibecraft::world::textureTileIndex(BlockType::HabitatPanel, BlockFace::Top) == 68);
    CHECK(vibecraft::world::textureTileIndex(BlockType::HabitatFloor, BlockFace::Top) == 69);
    CHECK(vibecraft::world::textureTileIndex(BlockType::HabitatFrame, BlockFace::Top) == 70);
    CHECK(vibecraft::world::textureTileIndex(BlockType::GreenhouseGlass, BlockFace::Top) == 71);
    CHECK(vibecraft::world::textureTileIndex(BlockType::PlanterTray, BlockFace::Top) == 72);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FiberSapling, BlockFace::Top) == 73);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FiberSprout, BlockFace::Top) == 74);
    CHECK(vibecraft::world::textureTileIndex(BlockType::AirlockPanel, BlockFace::Top) == 75);
    CHECK(vibecraft::world::textureTileIndex(BlockType::PowerConduit, BlockFace::Top) == 76);
}
