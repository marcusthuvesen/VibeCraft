#include "vibecraft/audio/RuntimeAudioRoot.hpp"

#include <SDL3/SDL_filesystem.h>

#include <filesystem>

namespace vibecraft::audio
{
namespace
{
[[nodiscard]] bool looksLikeMinecraftAudioPack(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    return fs::is_directory(root / "music", ec) || fs::is_regular_file(root / "dig" / "grass1.ogg", ec);
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
            return candidate;
        }
    }

    return baseFromSdl / "audio" / "minecraft";
}
}  // namespace vibecraft::audio
