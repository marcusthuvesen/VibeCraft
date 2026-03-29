#include "vibecraft/multiplayer/Protocol.hpp"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace vibecraft::multiplayer::protocol
{
namespace
{
class ByteWriter
{
  public:
    void writeU8(const std::uint8_t value)
    {
        bytes_.push_back(value);
    }

    void writeU16(const std::uint16_t value)
    {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xffU));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    }

    void writeU32(const std::uint32_t value)
    {
        bytes_.push_back(static_cast<std::uint8_t>(value & 0xffU));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
        bytes_.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    }

    void writeI32(const std::int32_t value)
    {
        writeU32(static_cast<std::uint32_t>(value));
    }

    void writeF32(const float value)
    {
        std::uint32_t bits = 0;
        static_assert(sizeof(float) == sizeof(std::uint32_t), "float must be 32-bit");
        std::memcpy(&bits, &value, sizeof(float));
        writeU32(bits);
    }

    void writeString(const std::string& value, const std::size_t maxLength)
    {
        const std::size_t clampedLength = value.size() > maxLength ? maxLength : value.size();
        writeU16(static_cast<std::uint16_t>(clampedLength));
        bytes_.insert(
            bytes_.end(),
            reinterpret_cast<const std::uint8_t*>(value.data()),
            reinterpret_cast<const std::uint8_t*>(value.data() + clampedLength));
    }

    void writeBytes(std::span<const std::uint8_t> bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }

    [[nodiscard]] std::vector<std::uint8_t> takeBytes()
    {
        return std::move(bytes_);
    }

  private:
    std::vector<std::uint8_t> bytes_;
};

class ByteReader
{
  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool readU8(std::uint8_t& out)
    {
        if (!canRead(1))
        {
            return false;
        }
        out = bytes_[offset_++];
        return true;
    }

    [[nodiscard]] bool readU16(std::uint16_t& out)
    {
        if (!canRead(2))
        {
            return false;
        }
        out = static_cast<std::uint16_t>(bytes_[offset_]) | (static_cast<std::uint16_t>(bytes_[offset_ + 1]) << 8U);
        offset_ += 2;
        return true;
    }

    [[nodiscard]] bool readU32(std::uint32_t& out)
    {
        if (!canRead(4))
        {
            return false;
        }
        out = static_cast<std::uint32_t>(bytes_[offset_])
            | (static_cast<std::uint32_t>(bytes_[offset_ + 1]) << 8U)
            | (static_cast<std::uint32_t>(bytes_[offset_ + 2]) << 16U)
            | (static_cast<std::uint32_t>(bytes_[offset_ + 3]) << 24U);
        offset_ += 4;
        return true;
    }

    [[nodiscard]] bool readI32(std::int32_t& out)
    {
        std::uint32_t raw = 0;
        if (!readU32(raw))
        {
            return false;
        }
        out = static_cast<std::int32_t>(raw);
        return true;
    }

    [[nodiscard]] bool readF32(float& out)
    {
        std::uint32_t bits = 0;
        if (!readU32(bits))
        {
            return false;
        }
        std::memcpy(&out, &bits, sizeof(float));
        return true;
    }

    [[nodiscard]] bool readString(std::string& out, const std::size_t maxLength)
    {
        std::uint16_t length = 0;
        if (!readU16(length))
        {
            return false;
        }
        if (length > maxLength || !canRead(length))
        {
            return false;
        }
        out.assign(
            reinterpret_cast<const char*>(bytes_.data() + offset_),
            reinterpret_cast<const char*>(bytes_.data() + offset_ + length));
        offset_ += length;
        return true;
    }

    [[nodiscard]] bool readBytes(std::span<std::uint8_t> out)
    {
        if (!canRead(out.size()))
        {
            return false;
        }
        std::memcpy(out.data(), bytes_.data() + offset_, out.size());
        offset_ += out.size();
        return true;
    }

    [[nodiscard]] bool atEnd() const
    {
        return offset_ == bytes_.size();
    }

  private:
    [[nodiscard]] bool canRead(const std::size_t size) const
    {
        return offset_ + size <= bytes_.size();
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
};

void writeHeader(ByteWriter& writer, const MessageHeader& header)
{
    writer.writeU32(header.magic);
    writer.writeU16(header.version);
    writer.writeU8(static_cast<std::uint8_t>(header.type));
    writer.writeU8(header.reserved);
    writer.writeU32(header.sequence);
    writer.writeU32(header.tick);
}

[[nodiscard]] bool readHeader(ByteReader& reader, MessageHeader& header)
{
    if (!reader.readU32(header.magic) || !reader.readU16(header.version))
    {
        return false;
    }
    std::uint8_t typeValue = 0;
    if (!reader.readU8(typeValue) || !reader.readU8(header.reserved) || !reader.readU32(header.sequence)
        || !reader.readU32(header.tick))
    {
        return false;
    }
    header.type = static_cast<MessageType>(typeValue);
    return true;
}
}  // namespace

std::vector<std::uint8_t> encodeMessage(const MessageHeader& header, const MessagePayload& payload)
{
    ByteWriter writer;
    writeHeader(writer, header);

    std::visit(
        [&writer](const auto& message)
        {
            using MessageT = std::decay_t<decltype(message)>;

            if constexpr (std::is_same_v<MessageT, JoinRequestMessage>)
            {
                writer.writeString(message.playerName, kMaxPlayerNameLength);
            }
            else if constexpr (std::is_same_v<MessageT, JoinAcceptMessage>)
            {
                writer.writeU16(message.clientId);
                writer.writeU32(message.worldSeed);
                writer.writeF32(message.spawnX);
                writer.writeF32(message.spawnY);
                writer.writeF32(message.spawnZ);
                writer.writeF32(message.dayNightElapsedSeconds);
                writer.writeF32(message.weatherElapsedSeconds);
            }
            else if constexpr (std::is_same_v<MessageT, JoinRejectMessage>)
            {
                writer.writeString(message.reason, kMaxStringLength);
            }
            else if constexpr (std::is_same_v<MessageT, ClientInputMessage>)
            {
                writer.writeU16(message.clientId);
                writer.writeF32(message.dtSeconds);
                writer.writeF32(message.moveX);
                writer.writeF32(message.moveZ);
                writer.writeF32(message.yawDelta);
                writer.writeF32(message.pitchDelta);
                writer.writeF32(message.positionX);
                writer.writeF32(message.positionY);
                writer.writeF32(message.positionZ);
                writer.writeF32(message.health);
                writer.writeF32(message.air);
                std::uint8_t flags = 0;
                if (message.jump)
                {
                    flags |= 1U << 0U;
                }
                if (message.breakBlock)
                {
                    flags |= 1U << 1U;
                }
                if (message.placeBlock)
                {
                    flags |= 1U << 2U;
                }
                writer.writeU8(flags);
                writer.writeI32(message.targetX);
                writer.writeI32(message.targetY);
                writer.writeI32(message.targetZ);
                writer.writeU8(message.selectedHotbarIndex);
                writer.writeU8(static_cast<std::uint8_t>(message.placeBlockType));
            }
            else if constexpr (std::is_same_v<MessageT, ServerSnapshotMessage>)
            {
                writer.writeU32(message.serverTick);
                writer.writeF32(message.dayNightElapsedSeconds);
                writer.writeF32(message.weatherElapsedSeconds);
                const std::size_t clampedPlayerCount = std::min(message.players.size(), kMaxPlayersPerSnapshot);
                writer.writeU8(static_cast<std::uint8_t>(clampedPlayerCount));
                for (std::size_t i = 0; i < clampedPlayerCount; ++i)
                {
                    const PlayerSnapshotMessage& player = message.players[i];
                    writer.writeU16(player.clientId);
                    writer.writeF32(player.posX);
                    writer.writeF32(player.posY);
                    writer.writeF32(player.posZ);
                    writer.writeF32(player.yawDegrees);
                    writer.writeF32(player.pitchDegrees);
                    writer.writeF32(player.health);
                    writer.writeF32(player.air);
                }
                writer.writeU16(static_cast<std::uint16_t>(message.droppedItems.size()));
                for (const DroppedItemSnapshotMessage& droppedItem : message.droppedItems)
                {
                    writer.writeU8(static_cast<std::uint8_t>(droppedItem.blockType));
                    writer.writeF32(droppedItem.posX);
                    writer.writeF32(droppedItem.posY);
                    writer.writeF32(droppedItem.posZ);
                    writer.writeF32(droppedItem.velocityX);
                    writer.writeF32(droppedItem.velocityY);
                    writer.writeF32(droppedItem.velocityZ);
                    writer.writeF32(droppedItem.ageSeconds);
                    writer.writeF32(droppedItem.spinRadians);
                }
            }
            else if constexpr (std::is_same_v<MessageT, BlockEditEventMessage>)
            {
                writer.writeU16(message.authorClientId);
                writer.writeU8(static_cast<std::uint8_t>(message.action));
                writer.writeI32(message.x);
                writer.writeI32(message.y);
                writer.writeI32(message.z);
                writer.writeU8(static_cast<std::uint8_t>(message.blockType));
            }
            else if constexpr (std::is_same_v<MessageT, ChunkSnapshotMessage>)
            {
                writer.writeI32(message.coord.x);
                writer.writeI32(message.coord.z);
                writer.writeBytes(message.blocks);
            }
            else if constexpr (std::is_same_v<MessageT, ChunkSnapshotPartMessage>)
            {
                writer.writeI32(message.coord.x);
                writer.writeI32(message.coord.z);
                writer.writeU8(message.sectionIndex);
                writer.writeBytes(message.blocks);
            }
            else if constexpr (std::is_same_v<MessageT, PingMessage>)
            {
                writer.writeU32(message.clientTimeMs);
            }
            else if constexpr (std::is_same_v<MessageT, PongMessage>)
            {
                writer.writeU32(message.clientTimeMs);
                writer.writeU32(message.serverTimeMs);
            }
            else if constexpr (std::is_same_v<MessageT, DisconnectMessage>)
            {
                writer.writeString(message.reason, kMaxStringLength);
            }
        },
        payload);

    return writer.takeBytes();
}

std::optional<DecodedMessage> decodeMessage(const std::span<const std::uint8_t> bytes)
{
    ByteReader reader(bytes);
    DecodedMessage decoded;
    if (!readHeader(reader, decoded.header))
    {
        return std::nullopt;
    }
    if (decoded.header.magic != kProtocolMagic || decoded.header.version != kProtocolVersion)
    {
        return std::nullopt;
    }

    switch (decoded.header.type)
    {
    case MessageType::JoinRequest:
    {
        JoinRequestMessage message;
        if (!reader.readString(message.playerName, kMaxPlayerNameLength))
        {
            return std::nullopt;
        }
        decoded.payload = std::move(message);
        break;
    }
    case MessageType::JoinAccept:
    {
        JoinAcceptMessage message;
        if (!reader.readU16(message.clientId) || !reader.readU32(message.worldSeed) || !reader.readF32(message.spawnX)
            || !reader.readF32(message.spawnY) || !reader.readF32(message.spawnZ)
            || !reader.readF32(message.dayNightElapsedSeconds) || !reader.readF32(message.weatherElapsedSeconds))
        {
            return std::nullopt;
        }
        decoded.payload = message;
        break;
    }
    case MessageType::JoinReject:
    {
        JoinRejectMessage message;
        if (!reader.readString(message.reason, kMaxStringLength))
        {
            return std::nullopt;
        }
        decoded.payload = std::move(message);
        break;
    }
    case MessageType::ClientInput:
    {
        ClientInputMessage message;
        std::uint8_t flags = 0;
        std::uint8_t blockType = 0;
        if (!reader.readU16(message.clientId) || !reader.readF32(message.dtSeconds) || !reader.readF32(message.moveX)
            || !reader.readF32(message.moveZ) || !reader.readF32(message.yawDelta)
            || !reader.readF32(message.pitchDelta) || !reader.readF32(message.positionX)
            || !reader.readF32(message.positionY) || !reader.readF32(message.positionZ)
            || !reader.readF32(message.health) || !reader.readF32(message.air) || !reader.readU8(flags)
            || !reader.readI32(message.targetX) || !reader.readI32(message.targetY) || !reader.readI32(message.targetZ)
            || !reader.readU8(message.selectedHotbarIndex) || !reader.readU8(blockType))
        {
            return std::nullopt;
        }
        message.jump = (flags & (1U << 0U)) != 0;
        message.breakBlock = (flags & (1U << 1U)) != 0;
        message.placeBlock = (flags & (1U << 2U)) != 0;
        message.placeBlockType = static_cast<world::BlockType>(blockType);
        decoded.payload = message;
        break;
    }
    case MessageType::ServerSnapshot:
    {
        ServerSnapshotMessage message;
        std::uint8_t playerCount = 0;
        std::uint16_t droppedItemCount = 0;
        if (!reader.readU32(message.serverTick) || !reader.readF32(message.dayNightElapsedSeconds)
            || !reader.readF32(message.weatherElapsedSeconds) || !reader.readU8(playerCount))
        {
            return std::nullopt;
        }
        message.players.reserve(playerCount);
        for (std::uint8_t i = 0; i < playerCount; ++i)
        {
            PlayerSnapshotMessage player;
            if (!reader.readU16(player.clientId) || !reader.readF32(player.posX) || !reader.readF32(player.posY)
                || !reader.readF32(player.posZ) || !reader.readF32(player.yawDegrees)
                || !reader.readF32(player.pitchDegrees) || !reader.readF32(player.health)
                || !reader.readF32(player.air))
            {
                return std::nullopt;
            }
            message.players.push_back(player);
        }
        if (!reader.readU16(droppedItemCount))
        {
            return std::nullopt;
        }
        message.droppedItems.reserve(droppedItemCount);
        for (std::uint16_t i = 0; i < droppedItemCount; ++i)
        {
            DroppedItemSnapshotMessage droppedItem;
            std::uint8_t blockType = 0;
            if (!reader.readU8(blockType) || !reader.readF32(droppedItem.posX) || !reader.readF32(droppedItem.posY)
                || !reader.readF32(droppedItem.posZ) || !reader.readF32(droppedItem.velocityX)
                || !reader.readF32(droppedItem.velocityY) || !reader.readF32(droppedItem.velocityZ)
                || !reader.readF32(droppedItem.ageSeconds) || !reader.readF32(droppedItem.spinRadians))
            {
                return std::nullopt;
            }
            droppedItem.blockType = static_cast<world::BlockType>(blockType);
            message.droppedItems.push_back(droppedItem);
        }
        decoded.payload = std::move(message);
        break;
    }
    case MessageType::BlockEditEvent:
    {
        BlockEditEventMessage message;
        std::uint8_t action = 0;
        std::uint8_t blockType = 0;
        if (!reader.readU16(message.authorClientId) || !reader.readU8(action) || !reader.readI32(message.x)
            || !reader.readI32(message.y) || !reader.readI32(message.z) || !reader.readU8(blockType))
        {
            return std::nullopt;
        }
        message.action = static_cast<world::WorldEditAction>(action);
        message.blockType = static_cast<world::BlockType>(blockType);
        decoded.payload = message;
        break;
    }
    case MessageType::ChunkSnapshot:
    {
        ChunkSnapshotMessage message;
        if (!reader.readI32(message.coord.x) || !reader.readI32(message.coord.z) || !reader.readBytes(message.blocks))
        {
            return std::nullopt;
        }
        decoded.payload = message;
        break;
    }
    case MessageType::ChunkSnapshotPart:
    {
        ChunkSnapshotPartMessage message;
        if (!reader.readI32(message.coord.x) || !reader.readI32(message.coord.z)
            || !reader.readU8(message.sectionIndex) || !reader.readBytes(message.blocks))
        {
            return std::nullopt;
        }
        if (message.sectionIndex >= kChunkSnapshotSectionCount)
        {
            return std::nullopt;
        }
        decoded.payload = message;
        break;
    }
    case MessageType::Ping:
    {
        PingMessage message;
        if (!reader.readU32(message.clientTimeMs))
        {
            return std::nullopt;
        }
        decoded.payload = message;
        break;
    }
    case MessageType::Pong:
    {
        PongMessage message;
        if (!reader.readU32(message.clientTimeMs) || !reader.readU32(message.serverTimeMs))
        {
            return std::nullopt;
        }
        decoded.payload = message;
        break;
    }
    case MessageType::Disconnect:
    {
        DisconnectMessage message;
        if (!reader.readString(message.reason, kMaxStringLength))
        {
            return std::nullopt;
        }
        decoded.payload = std::move(message);
        break;
    }
    default:
        return std::nullopt;
    }

    if (!reader.atEnd())
    {
        return std::nullopt;
    }
    return decoded;
}
}  // namespace vibecraft::multiplayer::protocol
