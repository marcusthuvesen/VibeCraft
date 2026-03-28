#pragma once

#include <filesystem>

namespace vibecraft::audio
{
/// Resolves the Minecraft asset pack directory (music + dig SFX), trying runtime base path and cwd.
[[nodiscard]] std::filesystem::path resolveMinecraftAudioRoot();
}  // namespace vibecraft::audio
