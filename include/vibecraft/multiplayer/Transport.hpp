#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace vibecraft::multiplayer
{
struct NetworkEndpoint
{
    std::uint32_t ipv4Address = 0;
    std::uint16_t port = 0;

    [[nodiscard]] bool operator==(const NetworkEndpoint&) const = default;
};

struct ReceivedPacket
{
    NetworkEndpoint from{};
    std::vector<std::uint8_t> bytes;
};

class INetworkTransport
{
  public:
    virtual ~INetworkTransport() = default;

    virtual bool open(std::optional<std::uint16_t> localPort) = 0;
    virtual void close() = 0;
    virtual bool setPeer(const std::string& host, std::uint16_t port) = 0;
    virtual std::optional<NetworkEndpoint> peer() const = 0;
    virtual bool sendTo(const NetworkEndpoint& endpoint, std::span<const std::uint8_t> bytes) = 0;
    virtual std::vector<ReceivedPacket> poll() = 0;
    [[nodiscard]] virtual std::string lastError() const = 0;
};

[[nodiscard]] std::string endpointToString(const NetworkEndpoint& endpoint);
}  // namespace vibecraft::multiplayer
