#pragma once

#include <optional>
#include <string>
#include <vector>

#include "vibecraft/multiplayer/Transport.hpp"

namespace vibecraft::multiplayer
{
class UdpTransport final : public INetworkTransport
{
  public:
    UdpTransport() = default;
    ~UdpTransport() override;

    bool open(std::optional<std::uint16_t> localPort) override;
    void close() override;
    bool setPeer(const std::string& host, std::uint16_t port) override;
    std::optional<NetworkEndpoint> peer() const override;
    bool sendTo(const NetworkEndpoint& endpoint, std::span<const std::uint8_t> bytes) override;
    std::vector<ReceivedPacket> poll() override;
    [[nodiscard]] std::string lastError() const override;

  private:
    int socketHandle_ = -1;
    std::optional<NetworkEndpoint> peer_;
    std::string lastError_;
};
}  // namespace vibecraft::multiplayer
