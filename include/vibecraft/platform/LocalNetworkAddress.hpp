#pragma once

#include <string>

namespace vibecraft::platform
{
/// Best-effort non-loopback IPv4 for showing "tell your friend" on LAN. Empty if unknown.
[[nodiscard]] std::string primaryLanIPv4String();
}  // namespace vibecraft::platform
