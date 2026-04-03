#include <doctest/doctest.h>

#include <array>
#include <string>

#include "vibecraft/app/crafting/Crafting.hpp"
#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("inventory labels present minecraft-style block names")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::app::InventorySlot;
    using vibecraft::world::BlockType;

    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Grass)) == "Grass Block");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::JungleGrass)) == "Mossy Grass Block");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Sand)) == "Sand");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::BirchLog)) == "Birch Log");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::BirchLeaves)) == "Birch Leaves");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::DarkOakLog)) == "Dark Oak Log");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::DarkOakLeaves)) == "Dark Oak Leaves");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::DiamondOre)) == "Diamond Ore");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Glowstone)) == "Glowstone");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Fern)) == "Fern");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Podzol)) == "Podzol");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::CoarseDirt)) == "Coarse Dirt");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::OxygenGenerator)) == "Industrial Relay");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatPanel)) == "Habitat Panel");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatFloor)) == "Habitat Floor");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::HabitatFrame)) == "Habitat Frame");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::AirlockPanel)) == "Airlock Panel");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::PowerConduit)) == "Power Conduit");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::GreenhouseGlass)) == "Greenhouse Glass");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::PlanterTray)) == "Planter Tray");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FiberSapling)) == "Fiber Sapling");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FiberSprout)) == "Fiber Sprout");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::GrassTuft)) == "Grass");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::SparseTuft)) == "Sparse Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FlowerTuft)) == "Flower Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::DryTuft)) == "Dry Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::LushTuft)) == "Lush Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::FrostTuft)) == "Frost Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::CloverTuft)) == "Clover Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::SproutTuft)) == "Sprout Tuft");
    CHECK(std::string(vibecraft::app::blockTypeLabel(BlockType::Torch)) == "Torch");
    CHECK(std::string(vibecraft::app::equippedItemLabel(EquippedItem::ScoutHelmet)) == "Scout Helmet");
    CHECK(std::string(vibecraft::app::equippedItemLabel(EquippedItem::Coal)) == "Coal");
    CHECK(
        vibecraft::app::inventorySlotLabel(InventorySlot{
            .blockType = BlockType::Air,
            .count = 1,
            .equippedItem = EquippedItem::IronPickaxe,
        })
        == "Iron Pickaxe");
}

TEST_CASE("surface biome labels reflect the minecraft-style biome map")
{
    using vibecraft::world::SurfaceBiome;

    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Plains)) == "plains");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Forest)) == "forest");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::BirchForest)) == "birch forest");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::DarkForest)) == "dark forest");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Taiga)) == "taiga");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::SnowyTaiga)) == "snowy taiga");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Desert)) == "desert");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::SnowyPlains)) == "snowy plains");
    CHECK(std::string(vibecraft::world::surfaceBiomeLabel(SurfaceBiome::Jungle)) == "jungle");
}

TEST_CASE("torches can be crafted from glowstone")
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

TEST_CASE("terrain generator still exposes all overworld biome families")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    bool foundPlains = false;
    bool foundDesert = false;
    bool foundSnowyPlains = false;
    bool foundJungle = false;

    for (int worldX = -16384; worldX <= 16384; worldX += 96)
    {
        for (int worldZ = -16384; worldZ <= 16384; worldZ += 96)
        {
            switch (terrainGenerator.surfaceBiomeAt(worldX, worldZ))
            {
            case vibecraft::world::SurfaceBiome::Plains:
            case vibecraft::world::SurfaceBiome::SunflowerPlains:
            case vibecraft::world::SurfaceBiome::Meadow:
            case vibecraft::world::SurfaceBiome::WindsweptHills:
            case vibecraft::world::SurfaceBiome::Savanna:
            case vibecraft::world::SurfaceBiome::SavannaPlateau:
            case vibecraft::world::SurfaceBiome::WindsweptSavanna:
            case vibecraft::world::SurfaceBiome::Forest:
            case vibecraft::world::SurfaceBiome::FlowerForest:
            case vibecraft::world::SurfaceBiome::Swamp:
            case vibecraft::world::SurfaceBiome::MushroomField:
                foundPlains = true;
                break;
            case vibecraft::world::SurfaceBiome::Desert:
                foundDesert = true;
                break;
            case vibecraft::world::SurfaceBiome::SnowyPlains:
            case vibecraft::world::SurfaceBiome::IcePlains:
            case vibecraft::world::SurfaceBiome::IceSpikePlains:
            case vibecraft::world::SurfaceBiome::SnowyTaiga:
            case vibecraft::world::SurfaceBiome::SnowySlopes:
            case vibecraft::world::SurfaceBiome::FrozenPeaks:
            case vibecraft::world::SurfaceBiome::JaggedPeaks:
                foundSnowyPlains = true;
                break;
            case vibecraft::world::SurfaceBiome::Jungle:
            case vibecraft::world::SurfaceBiome::SparseJungle:
            case vibecraft::world::SurfaceBiome::BambooJungle:
                foundJungle = true;
                break;
            case vibecraft::world::SurfaceBiome::BirchForest:
            case vibecraft::world::SurfaceBiome::OldGrowthBirchForest:
            case vibecraft::world::SurfaceBiome::DarkForest:
            case vibecraft::world::SurfaceBiome::Taiga:
            case vibecraft::world::SurfaceBiome::OldGrowthSpruceTaiga:
            case vibecraft::world::SurfaceBiome::OldGrowthPineTaiga:
            case vibecraft::world::SurfaceBiome::StonyPeaks:
                foundPlains = true;
                break;
            }

            if (foundPlains && foundDesert && foundSnowyPlains && foundJungle)
            {
                break;
            }
        }

        if (foundPlains && foundDesert && foundSnowyPlains && foundJungle)
        {
            break;
        }
    }

    CHECK(foundPlains);
    CHECK(foundDesert);
    CHECK(foundSnowyPlains);
    CHECK(foundJungle);
}

TEST_CASE("generated forest can grow oak trees")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Forest);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundOakTree = false;
    for (int worldX = -160; worldX <= 160 && !foundOakTree; ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && !foundOakTree; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundOakTree =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::OakLog
                || world.blockAt(worldX, surfaceY + 2, worldZ) == vibecraft::world::BlockType::OakLog
                || world.blockAt(worldX, surfaceY + 3, worldZ) == vibecraft::world::BlockType::OakLeaves;
        }
    }

    CHECK(foundOakTree);
}

TEST_CASE("generated snowy taiga can grow spruce trees")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::SnowyTaiga);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundSpruceTree = false;
    for (int worldX = -160; worldX <= 160 && !foundSpruceTree; ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && !foundSpruceTree; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundSpruceTree =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::SpruceLog
                || world.blockAt(worldX, surfaceY + 2, worldZ) == vibecraft::world::BlockType::SpruceLog
                || world.blockAt(worldX, surfaceY + 3, worldZ) == vibecraft::world::BlockType::SpruceLeaves;
        }
    }

    CHECK(foundSpruceTree);
}

TEST_CASE("generated jungle can grow jungle trees")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Jungle);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundJungleTree = false;
    for (int worldX = -160; worldX <= 160 && !foundJungleTree; ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && !foundJungleTree; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            foundJungleTree =
                world.blockAt(worldX, surfaceY + 1, worldZ) == vibecraft::world::BlockType::JungleLog
                || world.blockAt(worldX, surfaceY + 2, worldZ) == vibecraft::world::BlockType::JungleLog
                || world.blockAt(worldX, surfaceY + 3, worldZ) == vibecraft::world::BlockType::JungleLeaves;
        }
    }

    CHECK(foundJungleTree);
}

TEST_CASE("generated jungle can place bamboo or melons")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Jungle);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundJungleDecor = false;
    for (int worldX = -160; worldX <= 160 && !foundJungleDecor; ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && !foundJungleDecor; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const auto blockAbove = world.blockAt(worldX, surfaceY + 1, worldZ);
            foundJungleDecor =
                blockAbove == vibecraft::world::BlockType::Bamboo
                || blockAbove == vibecraft::world::BlockType::Melon;
        }
    }

    CHECK(foundJungleDecor);
}

TEST_CASE("generated desert surface stays sand-first like Minecraft")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Desert);

    std::array<vibecraft::world::BlockType, vibecraft::world::kWorldHeight> columnBlocks{};
    terrainGenerator.fillColumn(0, 0, columnBlocks.data());
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    const auto surfaceBlock = columnBlocks[static_cast<std::size_t>(surfaceY - vibecraft::world::kWorldMinY)];
    const bool isExpectedDesertSurface = surfaceBlock == vibecraft::world::BlockType::Sand
        || surfaceBlock == vibecraft::world::BlockType::Sandstone;
    CHECK(isExpectedDesertSurface);
    CHECK(surfaceBlock != vibecraft::world::BlockType::Gravel);
}

TEST_CASE("generated desert exposes sandstone in surface strata")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Desert);

    std::array<vibecraft::world::BlockType, vibecraft::world::kWorldHeight> columnBlocks{};
    terrainGenerator.fillColumn(0, 0, columnBlocks.data());
    const int surfaceY = terrainGenerator.surfaceHeightAt(0, 0);
    bool foundSandstoneStrata = false;
    for (int depth = 0; depth <= 8 && !foundSandstoneStrata; ++depth)
    {
        const int y = surfaceY - depth;
        if (y < vibecraft::world::kWorldMinY)
        {
            break;
        }
        foundSandstoneStrata =
            columnBlocks[static_cast<std::size_t>(y - vibecraft::world::kWorldMinY)] == vibecraft::world::BlockType::Sandstone;
    }

    CHECK(foundSandstoneStrata);
}

TEST_CASE("generated desert can place cactus or dead bushes")
{
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x42f0a17u);
    terrainGenerator.setBiomeOverride(vibecraft::world::SurfaceBiome::Desert);

    vibecraft::world::World world;
    world.generateRadius(terrainGenerator, 10);

    bool foundDesertDecor = false;
    for (int worldX = -160; worldX <= 160 && !foundDesertDecor; ++worldX)
    {
        for (int worldZ = -160; worldZ <= 160 && !foundDesertDecor; ++worldZ)
        {
            const int surfaceY = terrainGenerator.surfaceHeightAt(worldX, worldZ);
            const auto blockAbove = world.blockAt(worldX, surfaceY + 1, worldZ);
            foundDesertDecor =
                blockAbove == vibecraft::world::BlockType::Cactus
                || blockAbove == vibecraft::world::BlockType::DeadBush;
        }
    }

    CHECK(foundDesertDecor);
}

TEST_CASE("glowstone remains a breakable field resource")
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
    CHECK(vibecraft::world::textureTileIndex(BlockType::GrassTuft, BlockFace::Side) == 80);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SparseTuft, BlockFace::Side) == 81);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FlowerTuft, BlockFace::Side) == 82);
    CHECK(vibecraft::world::textureTileIndex(BlockType::DryTuft, BlockFace::Side) == 83);
    CHECK(vibecraft::world::textureTileIndex(BlockType::LushTuft, BlockFace::Side) == 84);
    CHECK(vibecraft::world::textureTileIndex(BlockType::FrostTuft, BlockFace::Side) == 85);
    CHECK(vibecraft::world::textureTileIndex(BlockType::CloverTuft, BlockFace::Side) == 86);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SproutTuft, BlockFace::Side) == 87);
    CHECK(vibecraft::world::textureTileIndex(BlockType::BirchLog, BlockFace::Top) == 92);
    CHECK(vibecraft::world::textureTileIndex(BlockType::BirchLog, BlockFace::Side) == 93);
    CHECK(vibecraft::world::textureTileIndex(BlockType::BirchLeaves, BlockFace::Side) == 94);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Fern, BlockFace::Side) == 95);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Podzol, BlockFace::Top) == 96);
    CHECK(vibecraft::world::textureTileIndex(BlockType::Podzol, BlockFace::Side) == 97);
    CHECK(vibecraft::world::textureTileIndex(BlockType::CoarseDirt, BlockFace::Side) == 98);
    CHECK(vibecraft::world::textureTileIndex(BlockType::DarkOakLog, BlockFace::Top) == 99);
    CHECK(vibecraft::world::textureTileIndex(BlockType::DarkOakLog, BlockFace::Side) == 100);
    CHECK(vibecraft::world::textureTileIndex(BlockType::DarkOakLeaves, BlockFace::Side) == 101);
    CHECK(vibecraft::world::textureTileIndex(BlockType::SculkBlock, BlockFace::Side) == 104);
    CHECK(vibecraft::world::textureTileIndex(BlockType::DripstoneBlock, BlockFace::Side) == 105);
}
