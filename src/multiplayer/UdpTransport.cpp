#include "vibecraft/multiplayer/UdpTransport.hpp"

#include <array>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace vibecraft::multiplayer
{
namespace
{
[[nodiscard]] int closeSocketHandle(const int handle)
{
#if defined(_WIN32)
    return closesocket(handle);
#else
    return close(handle);
#endif
}

[[nodiscard]] std::string lastSocketErrorString()
{
#if defined(_WIN32)
    return "winsock error";
#else
    return std::strerror(errno);
#endif
}

[[nodiscard]] sockaddr_in endpointToSockaddr(const NetworkEndpoint& endpoint)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    address.sin_addr.s_addr = endpoint.ipv4Address;
    return address;
}

[[nodiscard]] NetworkEndpoint sockaddrToEndpoint(const sockaddr_in& address)
{
    return NetworkEndpoint{
        .ipv4Address = address.sin_addr.s_addr,
        .port = ntohs(address.sin_port),
    };
}
}  // namespace

std::string endpointToString(const NetworkEndpoint& endpoint)
{
    in_addr address{};
    address.s_addr = endpoint.ipv4Address;
    const char* const addressString = inet_ntoa(address);
    if (addressString == nullptr)
    {
        return "invalid-address:0";
    }
    return std::string(addressString) + ":" + std::to_string(endpoint.port);
}

UdpTransport::~UdpTransport()
{
    close();
}

bool UdpTransport::open(const std::optional<std::uint16_t> localPort)
{
    close();

#if defined(_WIN32)
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        lastError_ = "WSAStartup failed";
        return false;
    }
#endif

    socketHandle_ = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, 0));
    if (socketHandle_ < 0)
    {
        lastError_ = "Failed creating UDP socket: " + lastSocketErrorString();
        return false;
    }

#if defined(_WIN32)
    u_long nonBlocking = 1;
    if (ioctlsocket(socketHandle_, FIONBIO, &nonBlocking) != 0)
    {
        lastError_ = "Failed setting non-blocking UDP socket.";
        close();
        return false;
    }
#else
    const int flags = fcntl(socketHandle_, F_GETFL, 0);
    if (flags < 0 || fcntl(socketHandle_, F_SETFL, flags | O_NONBLOCK) != 0)
    {
        lastError_ = "Failed setting non-blocking UDP socket: " + lastSocketErrorString();
        close();
        return false;
    }
#endif

    if (localPort.has_value())
    {
        sockaddr_in bindAddress{};
        bindAddress.sin_family = AF_INET;
        bindAddress.sin_port = htons(*localPort);
        bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(socketHandle_, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress)) != 0)
        {
            lastError_ = "Failed binding UDP socket: " + lastSocketErrorString();
            close();
            return false;
        }
    }

    lastError_.clear();
    return true;
}

void UdpTransport::close()
{
    if (socketHandle_ >= 0)
    {
        static_cast<void>(closeSocketHandle(socketHandle_));
        socketHandle_ = -1;
    }
    peer_.reset();
#if defined(_WIN32)
    WSACleanup();
#endif
}

bool UdpTransport::setPeer(const std::string& host, const std::uint16_t port)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || result == nullptr)
    {
        lastError_ = "Failed resolving host '" + host + "'.";
        return false;
    }

    const auto* const ipv4Address = reinterpret_cast<const sockaddr_in*>(result->ai_addr);
    peer_ = NetworkEndpoint{
        .ipv4Address = ipv4Address->sin_addr.s_addr,
        .port = port,
    };
    freeaddrinfo(result);
    return true;
}

std::optional<NetworkEndpoint> UdpTransport::peer() const
{
    return peer_;
}

bool UdpTransport::sendTo(const NetworkEndpoint& endpoint, const std::span<const std::uint8_t> bytes)
{
    if (socketHandle_ < 0)
    {
        lastError_ = "UDP socket is not open.";
        return false;
    }
    const sockaddr_in targetAddress = endpointToSockaddr(endpoint);
    const int result = sendto(
        socketHandle_,
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()),
        0,
        reinterpret_cast<const sockaddr*>(&targetAddress),
        sizeof(targetAddress));
    if (result < 0)
    {
        lastError_ = "sendto failed: " + lastSocketErrorString();
        return false;
    }
    return true;
}

std::vector<ReceivedPacket> UdpTransport::poll()
{
    std::vector<ReceivedPacket> packets;
    if (socketHandle_ < 0)
    {
        return packets;
    }

    std::array<std::uint8_t, 65535> buffer{};
    while (true)
    {
        sockaddr_in sourceAddress{};
        socklen_t sourceLength = sizeof(sourceAddress);
        const int bytesRead = recvfrom(
            socketHandle_,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&sourceAddress),
            &sourceLength);

        if (bytesRead <= 0)
        {
            break;
        }

        ReceivedPacket packet;
        packet.from = sockaddrToEndpoint(sourceAddress);
        packet.bytes.assign(buffer.begin(), buffer.begin() + bytesRead);
        packets.push_back(std::move(packet));
    }

    return packets;
}

std::string UdpTransport::lastError() const
{
    return lastError_;
}
}  // namespace vibecraft::multiplayer
