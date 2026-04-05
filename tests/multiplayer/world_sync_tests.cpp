#include <doctest/doctest.h>

#include "vibecraft/multiplayer/WorldSync.hpp"

TEST_CASE("chunk snapshots rebuild the same chunk contents on a client world")
{
    using vibecraft::multiplayer::applyChunkSnapshot;
    using vibecraft::multiplayer::buildChunkSnapshot;
    using vibecraft::world::BlockType;
    using vibecraft::world::Chunk;
    using vibecraft::world::ChunkCoord;
    using vibecraft::world::World;

    Chunk hostChunk(ChunkCoord{2, -1});
    auto& hostStorage = hostChunk.mutableBlockStorage();
    for (std::size_t blockIndex = 0; blockIndex < hostStorage.size(); ++blockIndex)
    {
        switch (blockIndex % 4)
        {
        case 0:
            hostStorage[blockIndex] = BlockType::Air;
            break;
        case 1:
            hostStorage[blockIndex] = BlockType::Dirt;
            break;
        case 2:
            hostStorage[blockIndex] = BlockType::Stone;
            break;
        default:
            hostStorage[blockIndex] = BlockType::Water;
            break;
        }
    }

    const auto snapshot = buildChunkSnapshot(hostChunk);
    World clientWorld;
    applyChunkSnapshot(clientWorld, snapshot);

    const auto clientChunkIt = clientWorld.chunks().find(ChunkCoord{2, -1});
    REQUIRE(clientChunkIt != clientWorld.chunks().end());
    const auto& clientStorage = clientChunkIt->second.blockStorage();
    REQUIRE(clientStorage.size() == hostStorage.size());
    CHECK(clientStorage[0] == hostStorage[0]);
    CHECK(clientStorage[1] == hostStorage[1]);
    CHECK(clientStorage[255] == hostStorage[255]);
    CHECK(clientStorage[4096] == hostStorage[4096]);
    CHECK(clientStorage.back() == hostStorage.back());
}

TEST_CASE("block edit event helpers keep host and client terrain in sync")
{
    using vibecraft::multiplayer::applyBlockEditEvent;
    using vibecraft::multiplayer::applyChunkSnapshot;
    using vibecraft::multiplayer::buildChunkSnapshot;
    using vibecraft::multiplayer::protocol::BlockEditEventMessage;
    using vibecraft::world::BlockType;
    using vibecraft::world::ChunkCoord;
    using vibecraft::world::World;
    using vibecraft::world::WorldEditAction;

    World hostWorld;
    World clientWorld;

    const BlockEditEventMessage placeStone{
        .authorClientId = 7,
        .action = WorldEditAction::Place,
        .x = 20,
        .y = 70,
        .z = -5,
        .blockType = BlockType::Stone,
    };
    REQUIRE(applyBlockEditEvent(hostWorld, placeStone));
    REQUIRE(hostWorld.blockAt(20, 70, -5) == BlockType::Stone);

    const auto hostChunkIt = hostWorld.chunks().find(ChunkCoord{1, -1});
    REQUIRE(hostChunkIt != hostWorld.chunks().end());
    applyChunkSnapshot(clientWorld, buildChunkSnapshot(hostChunkIt->second));
    CHECK(clientWorld.blockAt(20, 70, -5) == BlockType::Stone);

    const BlockEditEventMessage replaceWithDirt{
        .authorClientId = 7,
        .action = WorldEditAction::Place,
        .x = 20,
        .y = 70,
        .z = -5,
        .blockType = BlockType::Dirt,
    };
    REQUIRE(applyBlockEditEvent(hostWorld, replaceWithDirt));
    REQUIRE(applyBlockEditEvent(clientWorld, replaceWithDirt));
    CHECK(hostWorld.blockAt(20, 70, -5) == BlockType::Dirt);
    CHECK(clientWorld.blockAt(20, 70, -5) == BlockType::Dirt);

    const BlockEditEventMessage removeBlock{
        .authorClientId = 7,
        .action = WorldEditAction::Remove,
        .x = 20,
        .y = 70,
        .z = -5,
        .blockType = BlockType::Air,
    };
    REQUIRE(applyBlockEditEvent(hostWorld, removeBlock));
    REQUIRE(applyBlockEditEvent(clientWorld, removeBlock));
    CHECK(hostWorld.blockAt(20, 70, -5) == BlockType::Air);
    CHECK(clientWorld.blockAt(20, 70, -5) == BlockType::Air);
}
