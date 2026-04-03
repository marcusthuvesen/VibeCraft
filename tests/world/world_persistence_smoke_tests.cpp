#include <filesystem>

#include <doctest/doctest.h>

#include "vibecraft/app/SingleplayerSave.hpp"
#include "vibecraft/world/TerrainGenerator.hpp"
#include "vibecraft/world/World.hpp"

TEST_CASE("world save and load round-trips edited blocks")
{
    vibecraft::world::World world;
    world.setGenerationSeed(0x13579bdfU);
    vibecraft::world::TerrainGenerator terrainGenerator;
    terrainGenerator.setWorldSeed(0x13579bdfU);
    world.generateRadius(terrainGenerator, 1);
    int editY = terrainGenerator.surfaceHeightAt(2, 2) + 1;
    while (editY <= vibecraft::world::kWorldMaxY && world.blockAt(2, editY, 2) != vibecraft::world::BlockType::Air)
    {
        ++editY;
    }
    REQUIRE(editY <= vibecraft::world::kWorldMaxY);
    REQUIRE(world.applyEditCommand({
        .action = vibecraft::world::WorldEditAction::Place,
        .position = {2, editY, 2},
        .blockType = vibecraft::world::BlockType::CoalOre,
    }));

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "vibecraft_world_smoke_test.bin";

    REQUIRE(world.save(tempPath));

    vibecraft::world::World loadedWorld;
    REQUIRE(loadedWorld.load(tempPath));
    CHECK(loadedWorld.blockAt(2, editY, 2) == vibecraft::world::BlockType::CoalOre);
    CHECK(loadedWorld.generationSeed() == 0x13579bdfU);

    std::error_code errorCode;
    std::filesystem::remove(tempPath, errorCode);
}

TEST_CASE("singleplayer save serializer round-trips metadata and player state")
{
    namespace fs = std::filesystem;
    const fs::path tempDir = fs::temp_directory_path() / "vibecraft_singleplayer_save_test";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir);

    vibecraft::app::SingleplayerWorldMetadata metadata{
        .displayName = "World 7",
        .seed = 0x2468ace0U,
        .createdUnixSeconds = 111,
        .lastPlayedUnixSeconds = 222,
    };
    REQUIRE(vibecraft::app::SingleplayerSaveSerializer::saveMetadata(metadata, tempDir / "meta.json"));
    const auto loadedMetadata =
        vibecraft::app::SingleplayerSaveSerializer::loadMetadata(tempDir / "meta.json");
    REQUIRE(loadedMetadata.has_value());
    CHECK(loadedMetadata->displayName == metadata.displayName);
    CHECK(loadedMetadata->seed == metadata.seed);
    CHECK(loadedMetadata->createdUnixSeconds == metadata.createdUnixSeconds);
    CHECK(loadedMetadata->lastPlayedUnixSeconds == metadata.lastPlayedUnixSeconds);

    vibecraft::app::SingleplayerPlayerState playerState;
    playerState.playerFeetPosition = {10.0f, 65.0f, -12.0f};
    playerState.spawnFeetPosition = {8.0f, 70.0f, -6.0f};
    playerState.cameraYawDegrees = 135.0f;
    playerState.cameraPitchDegrees = -18.0f;
    playerState.health = 14.0f;
    playerState.air = 6.0f;
    playerState.creativeModeEnabled = true;
    playerState.selectedHotbarIndex = 4;
    playerState.dayNightElapsedSeconds = 123.0f;
    playerState.weatherElapsedSeconds = 45.0f;
    playerState.hotbarSlots[0] = {
        .blockType = vibecraft::world::BlockType::Stone,
        .count = 32,
        .equippedItem = vibecraft::app::EquippedItem::None,
    };
    playerState.hotbarSlots[1] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = vibecraft::app::EquippedItem::StonePickaxe,
    };
    playerState.bagSlots[3] = {
        .blockType = vibecraft::world::BlockType::Torch,
        .count = 12,
        .equippedItem = vibecraft::app::EquippedItem::None,
    };
    playerState.equipmentSlots[static_cast<std::size_t>(vibecraft::app::EquipmentSlotKind::Helmet)] = {
        .blockType = vibecraft::world::BlockType::Air,
        .count = 1,
        .equippedItem = vibecraft::app::EquippedItem::ScoutHelmet,
    };
    playerState.droppedItems.push_back({
        .blockType = vibecraft::world::BlockType::Cobblestone,
        .equippedItem = vibecraft::app::EquippedItem::None,
        .worldPosition = {1.0f, 2.0f, 3.0f},
        .velocity = {0.1f, 0.2f, 0.3f},
        .ageSeconds = 4.0f,
        .pickupDelaySeconds = 0.5f,
        .spinRadians = 1.25f,
    });
    playerState.chestSlotsByPosition[123456789LL][0] = {
        .blockType = vibecraft::world::BlockType::OakPlanks,
        .count = 16,
        .equippedItem = vibecraft::app::EquippedItem::None,
    };

    REQUIRE(vibecraft::app::SingleplayerSaveSerializer::savePlayerState(playerState, tempDir / "player.bin"));
    const auto loadedPlayerState =
        vibecraft::app::SingleplayerSaveSerializer::loadPlayerState(tempDir / "player.bin");
    REQUIRE(loadedPlayerState.has_value());
    CHECK(loadedPlayerState->playerFeetPosition.x == doctest::Approx(playerState.playerFeetPosition.x));
    CHECK(loadedPlayerState->playerFeetPosition.y == doctest::Approx(playerState.playerFeetPosition.y));
    CHECK(loadedPlayerState->playerFeetPosition.z == doctest::Approx(playerState.playerFeetPosition.z));
    CHECK(loadedPlayerState->spawnFeetPosition.y == doctest::Approx(playerState.spawnFeetPosition.y));
    CHECK(loadedPlayerState->cameraYawDegrees == doctest::Approx(playerState.cameraYawDegrees));
    CHECK(loadedPlayerState->cameraPitchDegrees == doctest::Approx(playerState.cameraPitchDegrees));
    CHECK(loadedPlayerState->health == doctest::Approx(playerState.health));
    CHECK(loadedPlayerState->air == doctest::Approx(playerState.air));
    CHECK(loadedPlayerState->creativeModeEnabled == playerState.creativeModeEnabled);
    CHECK(loadedPlayerState->selectedHotbarIndex == playerState.selectedHotbarIndex);
    CHECK(loadedPlayerState->hotbarSlots[0].blockType == vibecraft::world::BlockType::Stone);
    CHECK(loadedPlayerState->hotbarSlots[1].equippedItem == vibecraft::app::EquippedItem::StonePickaxe);
    CHECK(loadedPlayerState->bagSlots[3].blockType == vibecraft::world::BlockType::Torch);
    CHECK(
        loadedPlayerState->equipmentSlots[static_cast<std::size_t>(vibecraft::app::EquipmentSlotKind::Helmet)]
            .equippedItem
        == vibecraft::app::EquippedItem::ScoutHelmet);
    REQUIRE(loadedPlayerState->droppedItems.size() == 1);
    CHECK(loadedPlayerState->droppedItems[0].blockType == vibecraft::world::BlockType::Cobblestone);
    CHECK(loadedPlayerState->droppedItems[0].pickupDelaySeconds == doctest::Approx(0.5f));
    REQUIRE(loadedPlayerState->chestSlotsByPosition.contains(123456789LL));
    CHECK(loadedPlayerState->chestSlotsByPosition.at(123456789LL)[0].count == 16);

    fs::remove_all(tempDir);
}
