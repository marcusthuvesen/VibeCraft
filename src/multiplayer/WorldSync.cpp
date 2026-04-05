#include "vibecraft/multiplayer/WorldSync.hpp"

namespace vibecraft::multiplayer
{
protocol::ChunkSnapshotMessage buildChunkSnapshot(const world::Chunk& chunk)
{
    protocol::ChunkSnapshotMessage snapshot{
        .coord = chunk.coord(),
    };
    const auto& blockStorage = chunk.blockStorage();
    for (std::size_t blockIndex = 0; blockIndex < blockStorage.size(); ++blockIndex)
    {
        snapshot.blocks[blockIndex] = static_cast<std::uint8_t>(blockStorage[blockIndex]);
    }
    return snapshot;
}

void applyChunkSnapshot(world::World& world, const protocol::ChunkSnapshotMessage& chunkMessage)
{
    world::Chunk chunk(chunkMessage.coord);
    auto& storage = chunk.mutableBlockStorage();
    for (std::size_t blockIndex = 0; blockIndex < storage.size(); ++blockIndex)
    {
        storage[blockIndex] = static_cast<world::BlockType>(chunkMessage.blocks[blockIndex]);
    }
    world.replaceChunk(std::move(chunk));
}

bool applyBlockEditEvent(world::World& world, const protocol::BlockEditEventMessage& editMessage)
{
    return world.applyEditCommand({
        .action = editMessage.action,
        .position = {editMessage.x, editMessage.y, editMessage.z},
        .blockType = editMessage.blockType,
    });
}
}  // namespace vibecraft::multiplayer
