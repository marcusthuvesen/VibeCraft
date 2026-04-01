#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/game/MobTypes.hpp"
#include "vibecraft/world/Chunk.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

namespace vibecraft::multiplayer::protocol
{
inline constexpr std::uint32_t kProtocolMagic = 0x56434d50;  // "VCMP"
inline constexpr std::uint16_t kProtocolVersion = 7;
inline constexpr std::size_t kMaxPlayerNameLength = 32;
inline constexpr std::size_t kMaxStringLength = 256;
inline constexpr std::size_t kMaxPlayersPerSnapshot = 8;
inline constexpr std::size_t kMaxMobsPerSnapshot = 96;

enum class MessageType : std::uint8_t
{
    JoinRequest = 1,
    JoinAccept = 2,
    JoinReject = 3,
    ClientInput = 4,
    ServerSnapshot = 5,
    BlockEditEvent = 6,
    ChunkSnapshot = 7,
    Ping = 8,
    Pong = 9,
    Disconnect = 10,
    ChunkSnapshotPart = 11,
};

struct MessageHeader
{
    std::uint32_t magic = kProtocolMagic;
    std::uint16_t version = kProtocolVersion;
    MessageType type = MessageType::Ping;
    std::uint8_t reserved = 0;
    std::uint32_t sequence = 0;
    std::uint32_t tick = 0;
};

struct JoinRequestMessage
{
    std::string playerName;
};

struct JoinAcceptMessage
{
    std::uint16_t clientId = 0;
    std::uint32_t worldSeed = 0;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    float spawnZ = 0.0f;
    float dayNightElapsedSeconds = 0.0f;
    float weatherElapsedSeconds = 0.0f;
};

struct JoinRejectMessage
{
    std::string reason;
};

struct ClientInputMessage
{
    std::uint16_t clientId = 0;
    float dtSeconds = 0.0f;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float health = 20.0f;
    float air = 10.0f;
    app::EquippedItem selectedEquippedItem = app::EquippedItem::None;
    world::BlockType selectedBlockType = world::BlockType::Air;
    bool jump = false;
    bool breakBlock = false;
    bool placeBlock = false;
    /// Client requests host-authoritative melee vs replicated mob id (protocol v5+).
    bool mobMeleeSwing = false;
    /// Sneaking (protocol v6+): affects host ray origin for melee validation.
    bool isSneaking = false;
    std::int32_t targetX = 0;
    std::int32_t targetY = 0;
    std::int32_t targetZ = 0;
    std::uint8_t selectedHotbarIndex = 0;
    world::BlockType placeBlockType = world::BlockType::Air;
    std::uint32_t mobMeleeTargetId = 0;
    /// Eye/camera world Y for host melee ray origin (protocol v7+). Invalid values fall back to feet+eye height.
    float cameraEyeY = 0.0f;
};

struct PlayerSnapshotMessage
{
    std::uint16_t clientId = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float health = 20.0f;
    float air = 10.0f;
    app::EquippedItem selectedEquippedItem = app::EquippedItem::None;
    world::BlockType selectedBlockType = world::BlockType::Air;
};

struct DroppedItemSnapshotMessage
{
    world::BlockType blockType = world::BlockType::Air;
    app::EquippedItem equippedItem = app::EquippedItem::None;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float velocityZ = 0.0f;
    float ageSeconds = 0.0f;
    float spinRadians = 0.0f;
};

/// Host-authoritative mob pose for client rendering (protocol v4+).
struct MobSnapshotMessage
{
    std::uint32_t id = 0;
    game::MobKind kind = game::MobKind::VoidStrider;
    float feetX = 0.0f;
    float feetY = 0.0f;
    float feetZ = 0.0f;
    float yawRadians = 0.0f;
    float halfWidth = 0.28f;
    float height = 1.75f;
    /// Authoritative health (protocol v6+); older snapshots omit it on the wire.
    float health = 0.0f;
};

struct ServerSnapshotMessage
{
    std::uint32_t serverTick = 0;
    float dayNightElapsedSeconds = 0.0f;
    float weatherElapsedSeconds = 0.0f;
    std::vector<PlayerSnapshotMessage> players;
    std::vector<DroppedItemSnapshotMessage> droppedItems;
    std::vector<MobSnapshotMessage> mobs;
};

struct BlockEditEventMessage
{
    std::uint16_t authorClientId = 0;
    world::WorldEditAction action = world::WorldEditAction::Place;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    world::BlockType blockType = world::BlockType::Air;
};

struct ChunkSnapshotMessage
{
    world::ChunkCoord coord{};
    std::array<std::uint8_t, static_cast<std::size_t>(world::Chunk::kBlockCount)> blocks{};
};

inline constexpr int kChunkSnapshotSectionHeight = 16;
inline constexpr std::size_t kChunkSnapshotSectionBlockCount =
    static_cast<std::size_t>(world::Chunk::kSize * world::Chunk::kSize * kChunkSnapshotSectionHeight);
inline constexpr std::size_t kChunkSnapshotSectionCount =
    static_cast<std::size_t>(world::Chunk::kHeight / kChunkSnapshotSectionHeight);
static_assert(
    world::Chunk::kHeight % kChunkSnapshotSectionHeight == 0,
    "Chunk snapshot sections require chunk height divisible by section height.");

struct ChunkSnapshotPartMessage
{
    world::ChunkCoord coord{};
    std::uint8_t sectionIndex = 0;
    std::array<std::uint8_t, kChunkSnapshotSectionBlockCount> blocks{};
};

struct PingMessage
{
    std::uint32_t clientTimeMs = 0;
};

struct PongMessage
{
    std::uint32_t clientTimeMs = 0;
    std::uint32_t serverTimeMs = 0;
};

struct DisconnectMessage
{
    std::string reason;
};

using MessagePayload = std::variant<
    JoinRequestMessage,
    JoinAcceptMessage,
    JoinRejectMessage,
    ClientInputMessage,
    ServerSnapshotMessage,
    BlockEditEventMessage,
    ChunkSnapshotMessage,
    ChunkSnapshotPartMessage,
    PingMessage,
    PongMessage,
    DisconnectMessage>;

struct DecodedMessage
{
    MessageHeader header{};
    MessagePayload payload;
};

[[nodiscard]] std::vector<std::uint8_t> encodeMessage(
    const MessageHeader& header,
    const MessagePayload& payload);

[[nodiscard]] std::optional<DecodedMessage> decodeMessage(std::span<const std::uint8_t> bytes);
}  // namespace vibecraft::multiplayer::protocol
