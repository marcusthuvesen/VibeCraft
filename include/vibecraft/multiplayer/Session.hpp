#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/multiplayer/Protocol.hpp"
#include "vibecraft/multiplayer/Transport.hpp"

namespace vibecraft::multiplayer
{
struct ConnectedClient
{
    std::uint16_t clientId = 0;
    NetworkEndpoint endpoint{};
    std::string playerName;
    bool initialWorldSent = false;
};

class HostSession
{
  public:
    explicit HostSession(std::unique_ptr<INetworkTransport> transport);

    bool start(std::uint16_t port);
    void shutdown();
    void poll();

    [[nodiscard]] const std::string& lastError() const;
    [[nodiscard]] bool running() const;

    [[nodiscard]] const std::vector<ConnectedClient>& clients() const;
    [[nodiscard]] std::vector<protocol::ClientInputMessage> takePendingInputs();

    void broadcastSnapshot(
        std::uint32_t serverTick,
        float dayNightElapsedSeconds,
        float weatherElapsedSeconds,
        const std::vector<protocol::PlayerSnapshotMessage>& players);
    void broadcastBlockEdit(const protocol::BlockEditEventMessage& edit);
    void sendChunkSnapshot(std::uint16_t clientId, const protocol::ChunkSnapshotMessage& chunk);

  private:
    std::uint32_t nextSequence_ = 1;
    std::uint16_t nextClientId_ = 1;
    bool running_ = false;
    std::string lastError_;
    std::unique_ptr<INetworkTransport> transport_;
    std::vector<ConnectedClient> clients_;
    std::unordered_map<std::uint16_t, std::size_t> clientIndexById_;
    std::vector<protocol::ClientInputMessage> pendingInputs_;

    void sendToClient(
        const ConnectedClient& client,
        protocol::MessageType type,
        const protocol::MessagePayload& payload,
        std::uint32_t tick = 0);
    std::optional<ConnectedClient> findClientByEndpoint(const NetworkEndpoint& endpoint) const;
};

class ClientSession
{
  public:
    explicit ClientSession(std::unique_ptr<INetworkTransport> transport);

    bool connect(const std::string& host, std::uint16_t port, std::string playerName);
    void disconnect();
    void poll();

    [[nodiscard]] bool connected() const;
    [[nodiscard]] bool connecting() const;
    [[nodiscard]] std::uint16_t clientId() const;
    [[nodiscard]] const std::string& lastError() const;

    void sendInput(const protocol::ClientInputMessage& input, std::uint32_t tick);
    [[nodiscard]] std::vector<protocol::ServerSnapshotMessage> takeSnapshots();
    [[nodiscard]] std::vector<protocol::BlockEditEventMessage> takeBlockEdits();
    [[nodiscard]] std::vector<protocol::ChunkSnapshotMessage> takeChunkSnapshots();
    [[nodiscard]] std::optional<protocol::JoinAcceptMessage> takeJoinAccept();

  private:
    std::uint32_t nextSequence_ = 1;
    bool connecting_ = false;
    bool connected_ = false;
    std::uint16_t clientId_ = 0;
    std::string playerName_;
    std::string lastError_;
    std::unique_ptr<INetworkTransport> transport_;
    std::vector<protocol::ServerSnapshotMessage> pendingSnapshots_;
    std::vector<protocol::BlockEditEventMessage> pendingBlockEdits_;
    std::vector<protocol::ChunkSnapshotMessage> pendingChunkSnapshots_;
    std::optional<protocol::JoinAcceptMessage> pendingJoinAccept_;

    void sendMessage(protocol::MessageType type, const protocol::MessagePayload& payload, std::uint32_t tick = 0);
};
}  // namespace vibecraft::multiplayer
