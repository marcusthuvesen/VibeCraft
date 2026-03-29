#include "vibecraft/platform/LocalNetworkAddress.hpp"

#if defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace vibecraft::platform
{
std::string primaryLanIPv4String()
{
#if defined(__APPLE__) || defined(__linux__)
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0)
    {
        return {};
    }

    std::string firstNonLoopback;
    std::string preferred192;
    std::string preferred10;
    for (ifaddrs* iface = interfaces; iface != nullptr; iface = iface->ifa_next)
    {
        if (iface->ifa_addr == nullptr || iface->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }
        const auto* const addr = reinterpret_cast<const sockaddr_in*>(iface->ifa_addr);
        const std::uint32_t raw = addr->sin_addr.s_addr;
        if (raw == htonl(INADDR_LOOPBACK) || raw == 0)
        {
            continue;
        }

        char buffer[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)) == nullptr)
        {
            continue;
        }

        const std::string candidate(buffer);
        if (firstNonLoopback.empty())
        {
            firstNonLoopback = candidate;
        }
        if (candidate.rfind("192.168.", 0) == 0)
        {
            preferred192 = candidate;
        }
        else if (candidate.rfind("10.", 0) == 0)
        {
            if (preferred10.empty())
            {
                preferred10 = candidate;
            }
        }
    }

    freeifaddrs(interfaces);
    if (!preferred192.empty())
    {
        return preferred192;
    }
    if (!preferred10.empty())
    {
        return preferred10;
    }
    return firstNonLoopback;
#else
    return {};
#endif
}
}  // namespace vibecraft::platform
