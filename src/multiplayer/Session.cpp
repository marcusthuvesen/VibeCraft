#include "vibecraft/multiplayer/Session.hpp"

#include <algorithm>
#include <utility>

namespace vibecraft::multiplayer
{
HostSession::HostSession(std::unique_ptr<INetworkTransport> transport) : transport_(std::move(transport)) {}

bool HostSession::start(const std::uint16_t port)
{
    if (transport_ == nullptr)
    {
        lastError_ = "Missing transport implementation.";
        return false;
    }
    if (!transport_->open(port))
    {
        lastError_ = transport_->lastError();
        return false;
    }
    running_ = true;
    clients_.clear();
    clientIndexById_.clear();
    pendingInputs_.clear();
    lastError_.clear();
    return true;
}

void HostSession::shutdown()
{
    running_ = false;
    clients_.clear();
    clientIndexById_.clear();
    pendingInputs_.clear();
    if (transport_ != nullptr)
    {
        transport_->close();
    }
}

void HostSession::poll()
{
    if (!running_ || transport_ == nullptr)
    {
        return;
    }

    const std::vector<ReceivedPacket> packets = transport_->poll();
    for (const ReceivedPacket& packet : packets)
    {
        const std::optional<protocol::DecodedMessage> decoded = protocol::decodeMessage(packet.bytes);
        if (!decoded.has_value())
        {
            continue;
        }

        switch (decoded->header.type)
        {
        case protocol::MessageType::JoinRequest:
        {
            const auto* const join = std::get_if<protocol::JoinRequestMessage>(&decoded->payload);
            if (join == nullptr)
            {
                break;
            }
            if (clients_.size() >= protocol::kMaxPlayersPerSnapshot)
            {
                sendToClient(
                    ConnectedClient{
                        .clientId = 0,
                        .endpoint = packet.from,
                        .playerName = join->playerName,
                    },
                    protocol::MessageType::JoinReject,
                    protocol::JoinRejectMessage{
                        .reason = "Server full",
                    });
                break;
            }

            const std::optional<ConnectedClient> existing = findClientByEndpoint(packet.from);
            if (existing.has_value())
            {
                break;
            }

            ConnectedClient client{
                .clientId = nextClientId_++,
                .endpoint = packet.from,
                .playerName = join->playerName,
                .initialWorldSent = false,
            };
            clientIndexById_[client.clientId] = clients_.size();
            clients_.push_back(client);
            sendToClient(
                client,
                protocol::MessageType::JoinAccept,
                protocol::JoinAcceptMessage{
                    .clientId = client.clientId,
                });
            break;
        }
        case protocol::MessageType::ClientInput:
        {
            const auto* const input = std::get_if<protocol::ClientInputMessage>(&decoded->payload);
            if (input == nullptr)
            {
                break;
            }
            const std::optional<ConnectedClient> client = findClientByEndpoint(packet.from);
            if (!client.has_value())
            {
                break;
            }
            protocol::ClientInputMessage normalizedInput = *input;
            normalizedInput.clientId = client->clientId;
            pendingInputs_.push_back(normalizedInput);
            break;
        }
        case protocol::MessageType::Disconnect:
        {
            const std::optional<ConnectedClient> client = findClientByEndpoint(packet.from);
            if (!client.has_value())
            {
                break;
            }
            clients_.erase(
                std::remove_if(
                    clients_.begin(),
                    clients_.end(),
                    [clientId = client->clientId](const ConnectedClient& connectedClient)
                    {
                        return connectedClient.clientId == clientId;
                    }),
                clients_.end());
            clientIndexById_.clear();
            for (std::size_t i = 0; i < clients_.size(); ++i)
            {
                clientIndexById_[clients_[i].clientId] = i;
            }
            break;
        }
        default:
            break;
        }
    }
}

const std::string& HostSession::lastError() const
{
    return lastError_;
}

bool HostSession::running() const
{
    return running_;
}

const std::vector<ConnectedClient>& HostSession::clients() const
{
    return clients_;
}

std::vector<protocol::ClientInputMessage> HostSession::takePendingInputs()
{
    std::vector<protocol::ClientInputMessage> pending = std::move(pendingInputs_);
    pendingInputs_.clear();
    return pending;
}

void HostSession::broadcastSnapshot(
    const std::uint32_t serverTick,
    const float dayNightElapsedSeconds,
    const float weatherElapsedSeconds,
    const std::vector<protocol::PlayerSnapshotMessage>& players)
{
    protocol::ServerSnapshotMessage snapshot{
        .serverTick = serverTick,
        .dayNightElapsedSeconds = dayNightElapsedSeconds,
        .weatherElapsedSeconds = weatherElapsedSeconds,
        .players = players,
    };
    for (const ConnectedClient& client : clients_)
    {
        sendToClient(client, protocol::MessageType::ServerSnapshot, snapshot, serverTick);
    }
}

void HostSession::broadcastBlockEdit(const protocol::BlockEditEventMessage& edit)
{
    for (const ConnectedClient& client : clients_)
    {
        sendToClient(client, protocol::MessageType::BlockEditEvent, edit);
    }
}

void HostSession::sendChunkSnapshot(const std::uint16_t clientId, const protocol::ChunkSnapshotMessage& chunk)
{
    const auto it = clientIndexById_.find(clientId);
    if (it == clientIndexById_.end())
    {
        return;
    }
    sendToClient(clients_[it->second], protocol::MessageType::ChunkSnapshot, chunk);
}

void HostSession::sendToClient(
    const ConnectedClient& client,
    const protocol::MessageType type,
    const protocol::MessagePayload& payload,
    const std::uint32_t tick)
{
    if (transport_ == nullptr)
    {
        return;
    }
    const protocol::MessageHeader header{
        .magic = protocol::kProtocolMagic,
        .version = protocol::kProtocolVersion,
        .type = type,
        .reserved = 0,
        .sequence = nextSequence_++,
        .tick = tick,
    };
    const std::vector<std::uint8_t> encoded = protocol::encodeMessage(header, payload);
    static_cast<void>(transport_->sendTo(client.endpoint, encoded));
}

std::optional<ConnectedClient> HostSession::findClientByEndpoint(const NetworkEndpoint& endpoint) const
{
    const auto it = std::find_if(
        clients_.begin(),
        clients_.end(),
        [&endpoint](const ConnectedClient& client)
        {
            return client.endpoint == endpoint;
        });
    if (it == clients_.end())
    {
        return std::nullopt;
    }
    return *it;
}

ClientSession::ClientSession(std::unique_ptr<INetworkTransport> transport) : transport_(std::move(transport)) {}

bool ClientSession::connect(const std::string& host, const std::uint16_t port, std::string playerName)
{
    if (transport_ == nullptr)
    {
        lastError_ = "Missing transport implementation.";
        return false;
    }
    if (!transport_->open(std::nullopt))
    {
        lastError_ = transport_->lastError();
        return false;
    }
    if (!transport_->setPeer(host, port))
    {
        lastError_ = transport_->lastError();
        return false;
    }

    playerName_ = std::move(playerName);
    connecting_ = true;
    connected_ = false;
    clientId_ = 0;
    pendingSnapshots_.clear();
    pendingBlockEdits_.clear();
    pendingChunkSnapshots_.clear();
    pendingJoinAccept_.reset();
    lastError_.clear();
    sendMessage(
        protocol::MessageType::JoinRequest,
        protocol::JoinRequestMessage{
            .playerName = playerName_,
        });
    return true;
}

void ClientSession::disconnect()
{
    if (transport_ != nullptr && connected_)
    {
        sendMessage(
            protocol::MessageType::Disconnect,
            protocol::DisconnectMessage{
                .reason = "Client disconnected",
            });
    }
    connecting_ = false;
    connected_ = false;
    clientId_ = 0;
    pendingSnapshots_.clear();
    pendingBlockEdits_.clear();
    pendingChunkSnapshots_.clear();
    pendingJoinAccept_.reset();
    if (transport_ != nullptr)
    {
        transport_->close();
    }
}

void ClientSession::poll()
{
    if (transport_ == nullptr)
    {
        return;
    }
    const std::vector<ReceivedPacket> packets = transport_->poll();
    for (const ReceivedPacket& packet : packets)
    {
        const std::optional<protocol::DecodedMessage> decoded = protocol::decodeMessage(packet.bytes);
        if (!decoded.has_value())
        {
            continue;
        }

        switch (decoded->header.type)
        {
        case protocol::MessageType::JoinAccept:
        {
            const auto* const accept = std::get_if<protocol::JoinAcceptMessage>(&decoded->payload);
            if (accept == nullptr)
            {
                break;
            }
            connecting_ = false;
            connected_ = true;
            clientId_ = accept->clientId;
            pendingJoinAccept_ = *accept;
            break;
        }
        case protocol::MessageType::JoinReject:
        {
            const auto* const reject = std::get_if<protocol::JoinRejectMessage>(&decoded->payload);
            if (reject == nullptr)
            {
                break;
            }
            connecting_ = false;
            connected_ = false;
            clientId_ = 0;
            lastError_ = reject->reason;
            break;
        }
        case protocol::MessageType::ServerSnapshot:
        {
            const auto* const snapshot = std::get_if<protocol::ServerSnapshotMessage>(&decoded->payload);
            if (snapshot != nullptr)
            {
                pendingSnapshots_.push_back(*snapshot);
            }
            break;
        }
        case protocol::MessageType::BlockEditEvent:
        {
            const auto* const edit = std::get_if<protocol::BlockEditEventMessage>(&decoded->payload);
            if (edit != nullptr)
            {
                pendingBlockEdits_.push_back(*edit);
            }
            break;
        }
        case protocol::MessageType::ChunkSnapshot:
        {
            const auto* const chunk = std::get_if<protocol::ChunkSnapshotMessage>(&decoded->payload);
            if (chunk != nullptr)
            {
                pendingChunkSnapshots_.push_back(*chunk);
            }
            break;
        }
        default:
            break;
        }
    }
}

bool ClientSession::connected() const
{
    return connected_;
}

bool ClientSession::connecting() const
{
    return connecting_;
}

std::uint16_t ClientSession::clientId() const
{
    return clientId_;
}

const std::string& ClientSession::lastError() const
{
    return lastError_;
}

void ClientSession::sendInput(const protocol::ClientInputMessage& input, const std::uint32_t tick)
{
    if (!connected_)
    {
        return;
    }
    sendMessage(protocol::MessageType::ClientInput, input, tick);
}

std::vector<protocol::ServerSnapshotMessage> ClientSession::takeSnapshots()
{
    std::vector<protocol::ServerSnapshotMessage> snapshots = std::move(pendingSnapshots_);
    pendingSnapshots_.clear();
    return snapshots;
}

std::vector<protocol::BlockEditEventMessage> ClientSession::takeBlockEdits()
{
    std::vector<protocol::BlockEditEventMessage> edits = std::move(pendingBlockEdits_);
    pendingBlockEdits_.clear();
    return edits;
}

std::vector<protocol::ChunkSnapshotMessage> ClientSession::takeChunkSnapshots()
{
    std::vector<protocol::ChunkSnapshotMessage> snapshots = std::move(pendingChunkSnapshots_);
    pendingChunkSnapshots_.clear();
    return snapshots;
}

std::optional<protocol::JoinAcceptMessage> ClientSession::takeJoinAccept()
{
    const std::optional<protocol::JoinAcceptMessage> accept = pendingJoinAccept_;
    pendingJoinAccept_.reset();
    return accept;
}

void ClientSession::sendMessage(
    const protocol::MessageType type,
    const protocol::MessagePayload& payload,
    const std::uint32_t tick)
{
    if (transport_ == nullptr)
    {
        return;
    }
    const std::optional<NetworkEndpoint> endpoint = transport_->peer();
    if (!endpoint.has_value())
    {
        return;
    }
    const protocol::MessageHeader header{
        .magic = protocol::kProtocolMagic,
        .version = protocol::kProtocolVersion,
        .type = type,
        .reserved = 0,
        .sequence = nextSequence_++,
        .tick = tick,
    };
    const std::vector<std::uint8_t> encoded = protocol::encodeMessage(header, payload);
    if (!transport_->sendTo(*endpoint, encoded))
    {
        lastError_ = transport_->lastError();
    }
}
}  // namespace vibecraft::multiplayer
