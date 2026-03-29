#include "vibecraft/audio/RuntimeAudioRoot.hpp"

#include <SDL3/SDL_filesystem.h>

#include <fmt/format.h>

#include <filesystem>

#include "vibecraft/core/Logger.hpp"

namespace vibecraft::audio
{
namespace
{
[[nodiscard]] bool looksLikeMinecraftAudioPack(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::is_directory(root / "music", ec) || fs::is_regular_file(root / "dig" / "grass1.ogg", ec)
        || fs::is_directory(root / "sounds" / "music", ec)
        || fs::is_regular_file(root / "sounds" / "dig" / "grass1.ogg", ec);
}

[[nodiscard]] std::filesystem::path normalizeAudioRoot(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(root / "sounds", ec))
    {
        return root / "sounds";
    }
    return root;
}
}  // namespace

std::filesystem::path resolveMinecraftAudioRoot()
{
    namespace fs = std::filesystem;
    const fs::path baseFromSdl = []()
    {
        const char* const base = SDL_GetBasePath();
        return base != nullptr ? fs::path(base) : fs::path();
    }();

    const fs::path candidates[] = {
        baseFromSdl / "audio" / "minecraft",
        baseFromSdl / ".." / "audio" / "minecraft",
        fs::current_path() / "audio" / "minecraft",
        fs::current_path() / "assets" / "audio" / "minecraft",
        fs::path("audio") / "minecraft",
        fs::path("assets") / "audio" / "minecraft",
    };

    for (const fs::path& candidate : candidates)
    {
        if (looksLikeMinecraftAudioPack(candidate))
        {
            return normalizeAudioRoot(candidate);
        }
    }

    return normalizeAudioRoot(baseFromSdl / "audio" / "minecraft");
}

void logMinecraftAudioPackDiagnostics(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const bool ok = looksLikeMinecraftAudioPack(root);
    if (ok)
    {
        core::logInfo(fmt::format("Minecraft audio pack found at {}", root.generic_string()));
        return;
    }

    const bool rootExists = fs::exists(root, ec);
    core::logWarning(fmt::format(
        "Minecraft audio pack missing or empty at '{}'{}."
        " Expected `music/` (OGG music) and/or `dig/grass1.ogg` (SFX). "
        "Copy `assets/audio/minecraft` from a Minecraft 1.20+ jar or install the pack next to the binary under `audio/minecraft`. "
        "Placeholder music and click SFX will play until files are present.",
        root.generic_string(),
        rootExists ? "" : " (path does not exist)"));
}
}  // namespace vibecraft::audio
