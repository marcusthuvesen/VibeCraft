#pragma once

#include <span>
#include <string_view>

namespace vibecraft::audio
{
enum class MusicContext
{
    Menu,
    OverworldDay,
    OverworldNight,
    Underwater,
};

struct MusicTrackDefinition
{
    std::string_view relativePath;
    float gain = 1.0f;
};

[[nodiscard]] std::span<const MusicTrackDefinition> musicTracksForContext(MusicContext context);
[[nodiscard]] const char* musicContextName(MusicContext context);
}  // namespace vibecraft::audio
