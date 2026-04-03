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
}

TEST_CASE("host and client sessions establish and exchange snapshots")
{
    constexpr std::uint16_t kTestPort = 45123;
    vibecraft::multiplayer::HostSession host(std::make_unique<vibecraft::multiplayer::UdpTransport>());
    vibecraft::multiplayer::ClientSession client(std::make_unique<vibecraft::multiplayer::UdpTransport>());

    REQUIRE(host.start(kTestPort));
    REQUIRE(client.connect("127.0.0.1", kTestPort, "test-player"));

    bool connected = false;
    for (int i = 0; i < 200 && !connected; ++i)
    {
        host.poll();
        client.poll();
        connected = client.connected() && !host.clients().empty();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    REQUIRE(connected);

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

    bool receivedSnapshot = false;
    for (int i = 0; i < 200 && !receivedSnapshot; ++i)
    {
        host.poll();
        client.poll();
        const auto snapshots = client.takeSnapshots();
        if (!snapshots.empty())
        {
            CHECK(snapshots.back().serverTick == 55);
            receivedSnapshot = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    CHECK(receivedSnapshot);
    client.disconnect();
    host.shutdown();
}
