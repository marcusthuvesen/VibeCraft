#pragma once

#include <filesystem>

namespace vibecraft::audio
{
/// Resolves the Minecraft asset pack directory (music + dig SFX), trying runtime base path and cwd.
[[nodiscard]] std::filesystem::path resolveMinecraftAudioRoot();

/// Logs whether the resolved pack looks usable (expects `music/` and/or `dig/grass1.ogg`).
void logMinecraftAudioPackDiagnostics(const std::filesystem::path& root);
}  // namespace vibecraft::audio
