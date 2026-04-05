#pragma once

#include "vibecraft/multiplayer/Protocol.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::multiplayer
{
[[nodiscard]] protocol::ChunkSnapshotMessage buildChunkSnapshot(const world::Chunk& chunk);
void applyChunkSnapshot(world::World& world, const protocol::ChunkSnapshotMessage& chunkMessage);
[[nodiscard]] bool applyBlockEditEvent(world::World& world, const protocol::BlockEditEventMessage& editMessage);
}  // namespace vibecraft::multiplayer
