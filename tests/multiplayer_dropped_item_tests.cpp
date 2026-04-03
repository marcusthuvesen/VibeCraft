#include <doctest/doctest.h>

#include "vibecraft/multiplayer/Protocol.hpp"

TEST_CASE("server snapshots preserve dropped equipped item identity")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::multiplayer::protocol::DroppedItemSnapshotMessage;
    using vibecraft::multiplayer::protocol::MessageHeader;
    using vibecraft::multiplayer::protocol::MessageType;
    using vibecraft::multiplayer::protocol::ServerSnapshotMessage;
    using vibecraft::world::BlockType;

    const ServerSnapshotMessage snapshot{
        .serverTick = 77,
        .dayNightElapsedSeconds = 10.0f,
        .weatherElapsedSeconds = 5.0f,
        .players = {},
        .droppedItems =
            {{
                .blockType = BlockType::Air,
                .equippedItem = EquippedItem::IronPickaxe,
                .posX = 1.0f,
                .posY = 2.0f,
                .posZ = 3.0f,
                .velocityX = 0.1f,
                .velocityY = 0.2f,
                .velocityZ = 0.3f,
                .ageSeconds = 4.0f,
                .spinRadians = 1.25f,
            }},
    };

    const MessageHeader header{
        .type = MessageType::ServerSnapshot,
        .sequence = 9,
        .tick = 12,
    };
    const auto encoded = vibecraft::multiplayer::protocol::encodeMessage(header, snapshot);
    const auto decoded = vibecraft::multiplayer::protocol::decodeMessage(encoded);
    REQUIRE(decoded.has_value());

    const auto* payload = std::get_if<ServerSnapshotMessage>(&decoded->payload);
    REQUIRE(payload != nullptr);
    REQUIRE(payload->droppedItems.size() == 1);

    const DroppedItemSnapshotMessage& droppedItem = payload->droppedItems.front();
    CHECK(droppedItem.blockType == BlockType::Air);
    CHECK(droppedItem.equippedItem == EquippedItem::IronPickaxe);
    CHECK(droppedItem.posX == doctest::Approx(1.0f));
    CHECK(droppedItem.spinRadians == doctest::Approx(1.25f));
}

TEST_CASE("server snapshots preserve scout armor equipped item on drops")
{
    using vibecraft::app::EquippedItem;
    using vibecraft::multiplayer::protocol::DroppedItemSnapshotMessage;
    using vibecraft::multiplayer::protocol::MessageHeader;
    using vibecraft::multiplayer::protocol::MessageType;
    using vibecraft::multiplayer::protocol::ServerSnapshotMessage;
    using vibecraft::world::BlockType;

    const ServerSnapshotMessage snapshot{
        .serverTick = 1,
        .dayNightElapsedSeconds = 0.0f,
        .weatherElapsedSeconds = 0.0f,
        .players = {},
        .droppedItems =
            {{
                .blockType = BlockType::Air,
                .equippedItem = EquippedItem::ScoutChestRig,
                .posX = 0.0f,
                .posY = 0.0f,
                .posZ = 0.0f,
                .velocityX = 0.0f,
                .velocityY = 0.0f,
                .velocityZ = 0.0f,
                .ageSeconds = 0.0f,
                .spinRadians = 0.0f,
            }},
    };

    const MessageHeader header{.type = MessageType::ServerSnapshot, .sequence = 1, .tick = 1};
    const auto encoded = vibecraft::multiplayer::protocol::encodeMessage(header, snapshot);
    const auto decoded = vibecraft::multiplayer::protocol::decodeMessage(encoded);
    REQUIRE(decoded.has_value());
    const auto* payload = std::get_if<ServerSnapshotMessage>(&decoded->payload);
    REQUIRE(payload != nullptr);
    REQUIRE(payload->droppedItems.size() == 1);
    CHECK(payload->droppedItems.front().equippedItem == EquippedItem::ScoutChestRig);
}

TEST_CASE("server snapshots round-trip mob poses for protocol v4+")
{
    using vibecraft::game::MobKind;
    using vibecraft::multiplayer::protocol::MessageHeader;
    using vibecraft::multiplayer::protocol::MessageType;
    using vibecraft::multiplayer::protocol::MobSnapshotMessage;
    using vibecraft::multiplayer::protocol::ServerSnapshotMessage;

    ServerSnapshotMessage snapshot{
        .serverTick = 42,
        .dayNightElapsedSeconds = 1.0f,
        .weatherElapsedSeconds = 2.0f,
        .players = {},
        .droppedItems = {},
        .mobs =
            {
                MobSnapshotMessage{
                    .id = 7,
                    .kind = MobKind::Zombie,
                    .feetX = 10.0f,
                    .feetY = 64.0f,
                    .feetZ = -3.0f,
                    .yawRadians = 1.25f,
                    .halfWidth = 0.28f,
                    .height = 1.75f,
                    .health = 12.5f,
                },
            },
    };

    const MessageHeader header{.type = MessageType::ServerSnapshot, .sequence = 2, .tick = 3};
    const auto encoded = vibecraft::multiplayer::protocol::encodeMessage(header, snapshot);
    const auto decoded = vibecraft::multiplayer::protocol::decodeMessage(encoded);
    REQUIRE(decoded.has_value());
    const auto* payload = std::get_if<ServerSnapshotMessage>(&decoded->payload);
    REQUIRE(payload != nullptr);
    REQUIRE(payload->mobs.size() == 1);
    CHECK(payload->mobs[0].id == 7);
    CHECK(payload->mobs[0].kind == MobKind::Zombie);
    CHECK(payload->mobs[0].feetX == doctest::Approx(10.0f));
    CHECK(payload->mobs[0].yawRadians == doctest::Approx(1.25f));
    CHECK(payload->mobs[0].health == doctest::Approx(12.5f));
}

TEST_CASE("client input round-trips mob melee sneaking and camera eye Y for protocol v7")
{
    using vibecraft::multiplayer::protocol::ClientInputMessage;
    using vibecraft::multiplayer::protocol::MessageHeader;
    using vibecraft::multiplayer::protocol::MessageType;

    const ClientInputMessage input{
        .clientId = 2,
        .dtSeconds = 1.0f / 60.0f,
        .moveX = 0.1f,
        .moveZ = -0.2f,
        .yawDelta = 45.0f,
        .pitchDelta = -10.0f,
        .positionX = 3.0f,
        .positionY = 64.0f,
        .positionZ = -1.0f,
        .health = 18.0f,
        .air = 9.0f,
        .mobMeleeSwing = true,
        .isSneaking = true,
        .mobMeleeTargetId = 42,
        .cameraEyeY = 65.62f,
    };

    const MessageHeader header{.type = MessageType::ClientInput, .sequence = 1, .tick = 9};
    const auto encoded = vibecraft::multiplayer::protocol::encodeMessage(header, input);
    const auto decoded = vibecraft::multiplayer::protocol::decodeMessage(encoded);
    REQUIRE(decoded.has_value());
    const auto* payload = std::get_if<ClientInputMessage>(&decoded->payload);
    REQUIRE(payload != nullptr);
    CHECK(payload->mobMeleeSwing);
    CHECK(payload->isSneaking);
    CHECK(payload->mobMeleeTargetId == 42);
    CHECK(payload->cameraEyeY == doctest::Approx(65.62f));
    CHECK(payload->yawDelta == doctest::Approx(45.0f));
}
