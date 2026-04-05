#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include "vibecraft/multiplayer/Protocol.hpp"
#include "vibecraft/multiplayer/Session.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"

TEST_CASE("multiplayer protocol round-trips chunk and snapshot messages")
{
    using namespace vibecraft::multiplayer::protocol;

    ChunkSnapshotMessage chunk{
        .coord = {2, -3},
    };
    for (std::size_t i = 0; i < chunk.blocks.size(); ++i)
    {
        chunk.blocks[i] = static_cast<std::uint8_t>(i % 16);
    }

    const MessageHeader chunkHeader{
        .type = MessageType::ChunkSnapshot,
        .sequence = 42,
        .tick = 7,
    };
    const std::vector<std::uint8_t> encodedChunk = encodeMessage(chunkHeader, chunk);
    const std::optional<DecodedMessage> decodedChunk = decodeMessage(encodedChunk);
    REQUIRE(decodedChunk.has_value());
    CHECK(decodedChunk->header.type == MessageType::ChunkSnapshot);
    const auto* decodedChunkPayload = std::get_if<ChunkSnapshotMessage>(&decodedChunk->payload);
    REQUIRE(decodedChunkPayload != nullptr);
    CHECK(decodedChunkPayload->coord == chunk.coord);
    CHECK(decodedChunkPayload->blocks[0] == chunk.blocks[0]);
    CHECK(decodedChunkPayload->blocks[100] == chunk.blocks[100]);
    CHECK(decodedChunkPayload->blocks.back() == chunk.blocks.back());

    ServerSnapshotMessage snapshot{
        .serverTick = 123,
        .dayNightElapsedSeconds = 44.0f,
        .weatherElapsedSeconds = 12.0f,
        .players =
            {{
                .clientId = 1,
                .posX = 10.0f,
                .posY = 64.0f,
                .posZ = -5.0f,
                .yawDegrees = 90.0f,
                .pitchDegrees = -10.0f,
                .health = 18.0f,
                .air = 7.5f,
            }},
    };
    const MessageHeader snapshotHeader{
        .type = MessageType::ServerSnapshot,
        .sequence = 43,
        .tick = 8,
    };
    const std::vector<std::uint8_t> encodedSnapshot = encodeMessage(snapshotHeader, snapshot);
    const std::optional<DecodedMessage> decodedSnapshot = decodeMessage(encodedSnapshot);
    REQUIRE(decodedSnapshot.has_value());
    const auto* decodedSnapshotPayload = std::get_if<ServerSnapshotMessage>(&decodedSnapshot->payload);
    REQUIRE(decodedSnapshotPayload != nullptr);
    CHECK(decodedSnapshotPayload->serverTick == snapshot.serverTick);
    REQUIRE(decodedSnapshotPayload->players.size() == 1);
    CHECK(decodedSnapshotPayload->players[0].clientId == 1);
    CHECK(decodedSnapshotPayload->players[0].posX == doctest::Approx(10.0f));

    const MessageHeader requestHeader{
        .type = MessageType::CommandRequest,
        .sequence = 44,
        .tick = 9,
    };
    const std::vector<std::uint8_t> encodedRequest = encodeMessage(
        requestHeader,
        CommandRequestMessage{
            .clientId = 3,
            .commandText = "/weather rain",
        });
    const std::optional<DecodedMessage> decodedRequest = decodeMessage(encodedRequest);
    REQUIRE(decodedRequest.has_value());
    const auto* decodedRequestPayload = std::get_if<CommandRequestMessage>(&decodedRequest->payload);
    REQUIRE(decodedRequestPayload != nullptr);
    CHECK(decodedRequestPayload->clientId == 3);
    CHECK(decodedRequestPayload->commandText == "/weather rain");

    const MessageHeader feedbackHeader{
        .type = MessageType::CommandFeedback,
        .sequence = 45,
        .tick = 10,
    };
    const std::vector<std::uint8_t> encodedFeedback = encodeMessage(
        feedbackHeader,
        CommandFeedbackMessage{
            .isError = false,
            .feedback = "Weather set to rain.",
        });
    const std::optional<DecodedMessage> decodedFeedback = decodeMessage(encodedFeedback);
    REQUIRE(decodedFeedback.has_value());
    const auto* decodedFeedbackPayload = std::get_if<CommandFeedbackMessage>(&decodedFeedback->payload);
    REQUIRE(decodedFeedbackPayload != nullptr);
    CHECK_FALSE(decodedFeedbackPayload->isError);
    CHECK(decodedFeedbackPayload->feedback == "Weather set to rain.");
}

TEST_CASE("host and client sessions establish and exchange snapshots")
{
    constexpr std::uint16_t kTestPort = 45123;
    vibecraft::multiplayer::HostSession host(std::make_unique<vibecraft::multiplayer::UdpTransport>());
    vibecraft::multiplayer::ClientSession client(std::make_unique<vibecraft::multiplayer::UdpTransport>());

    REQUIRE(host.start(kTestPort));
    REQUIRE(client.connect("127.0.0.1", kTestPort, "test-player"));

    std::optional<vibecraft::multiplayer::PendingClientJoin> pendingJoin;
    for (int i = 0; i < 200 && !pendingJoin.has_value(); ++i)
    {
        host.poll();
        client.poll();
        const auto pendingJoins = host.takePendingJoins();
        if (!pendingJoins.empty())
        {
            pendingJoin = pendingJoins.front();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(pendingJoin.has_value());
    CHECK(client.connecting());
    CHECK_FALSE(client.connected());

    host.acceptPendingJoin(
        pendingJoin->clientId,
        {
            .clientId = pendingJoin->clientId,
            .worldSeed = 2468,
            .spawnX = 24.0f,
            .spawnY = 72.0f,
            .spawnZ = -8.0f,
            .dayNightElapsedSeconds = 12.0f,
            .weatherElapsedSeconds = 8.0f,
        });

    vibecraft::multiplayer::protocol::ChunkSnapshotMessage chunk{
        .coord = {3, -2},
    };
    for (std::size_t blockIndex = 0; blockIndex < chunk.blocks.size(); ++blockIndex)
    {
        chunk.blocks[blockIndex] = static_cast<std::uint8_t>((blockIndex * 7) % 32);
    }

    bool receivedJoinAccept = false;
    bool receivedSnapshot = false;
    bool receivedChunk = false;
    bool sentWorldState = false;
    for (int i = 0; i < 300 && (!receivedJoinAccept || !receivedSnapshot || !receivedChunk); ++i)
    {
        host.poll();
        client.poll();
        if (!receivedJoinAccept)
        {
            const auto accepted = client.takeJoinAccept();
            if (accepted.has_value())
            {
                CHECK(accepted->clientId == pendingJoin->clientId);
                CHECK(accepted->worldSeed == 2468);
                CHECK(accepted->spawnX == doctest::Approx(24.0f));
                CHECK(accepted->spawnY == doctest::Approx(72.0f));
                CHECK(accepted->spawnZ == doctest::Approx(-8.0f));
                CHECK(accepted->dayNightElapsedSeconds == doctest::Approx(12.0f));
                CHECK(accepted->weatherElapsedSeconds == doctest::Approx(8.0f));
                receivedJoinAccept = true;
            }
        }
        if (receivedJoinAccept && !sentWorldState)
        {
            host.sendChunkSnapshot(pendingJoin->clientId, chunk);
            host.broadcastSnapshot({
                .serverTick = 55,
                .dayNightElapsedSeconds = 12.0f,
                .weatherElapsedSeconds = 8.0f,
                .players =
                    {{
                        .clientId = 0,
                        .posX = 1.0f,
                        .posY = 2.0f,
                        .posZ = 3.0f,
                        .yawDegrees = 45.0f,
                        .pitchDegrees = 0.0f,
                        .health = 20.0f,
                        .air = 10.0f,
                    }},
            });
            sentWorldState = true;
        }
        const auto snapshots = client.takeSnapshots();
        if (!snapshots.empty())
        {
            CHECK(snapshots.back().serverTick == 55);
            receivedSnapshot = true;
        }
        const auto chunks = client.takeChunkSnapshots();
        if (!chunks.empty())
        {
            CHECK(chunks.back().coord == chunk.coord);
            CHECK(chunks.back().blocks[0] == chunk.blocks[0]);
            CHECK(chunks.back().blocks[123] == chunk.blocks[123]);
            CHECK(chunks.back().blocks.back() == chunk.blocks.back());
            receivedChunk = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK(receivedJoinAccept);
    CHECK(receivedSnapshot);
    CHECK(receivedChunk);
    CHECK(client.connected());
    client.disconnect();
    host.shutdown();
}
