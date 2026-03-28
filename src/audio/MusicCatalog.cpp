#include "vibecraft/audio/MusicCatalog.hpp"

#include <array>

namespace vibecraft::audio
{
namespace
{
constexpr std::array<MusicTrackDefinition, 9> kMenuTracks{{
    {"music/game/below_and_above.ogg", 0.40f},
    {"music/game/broken_clocks.ogg", 0.40f},
    {"music/game/fireflies.ogg", 0.40f},
    {"music/game/lilypad.ogg", 0.40f},
    {"music/game/os_piano.ogg", 0.40f},
    {"music/menu/beginning_2.ogg", 1.00f},
    {"music/menu/floating_trees.ogg", 1.00f},
    {"music/menu/moog_city_2.ogg", 1.00f},
    {"music/menu/mutation.ogg", 1.00f},
}};

constexpr std::array<MusicTrackDefinition, 31> kOverworldDayTracks{{
    {"music/game/a_familiar_room.ogg", 0.40f},
    {"music/game/below_and_above.ogg", 0.40f},
    {"music/game/broken_clocks.ogg", 0.40f},
    {"music/game/clark.ogg", 1.00f},
    {"music/game/comforting_memories.ogg", 0.40f},
    {"music/game/creative/aria_math.ogg", 1.00f},
    {"music/game/creative/biome_fest.ogg", 1.00f},
    {"music/game/creative/blind_spots.ogg", 1.00f},
    {"music/game/creative/dreiton.ogg", 1.00f},
    {"music/game/creative/haunt_muskie.ogg", 1.00f},
    {"music/game/creative/taswell.ogg", 1.00f},
    {"music/game/danny.ogg", 1.00f},
    {"music/game/dry_hands.ogg", 1.00f},
    {"music/game/featherfall.ogg", 0.40f},
    {"music/game/fireflies.ogg", 0.40f},
    {"music/game/floating_dream.ogg", 0.40f},
    {"music/game/haggstrom.ogg", 1.00f},
    {"music/game/key.ogg", 1.00f},
    {"music/game/komorebi.ogg", 0.80f},
    {"music/game/left_to_bloom.ogg", 0.40f},
    {"music/game/lilypad.ogg", 0.40f},
    {"music/game/living_mice.ogg", 1.00f},
    {"music/game/minecraft.ogg", 1.00f},
    {"music/game/one_more_day.ogg", 0.40f},
    {"music/game/os_piano.ogg", 0.40f},
    {"music/game/oxygene.ogg", 1.00f},
    {"music/game/puzzlebox.ogg", 0.40f},
    {"music/game/subwoofer_lullaby.ogg", 1.00f},
    {"music/game/sweden.ogg", 1.00f},
    {"music/game/watcher.ogg", 0.40f},
    {"music/game/yakusoku.ogg", 0.80f},
}};

constexpr std::array<MusicTrackDefinition, 11> kOverworldNightTracks{{
    {"music/game/dry_hands.ogg", 1.00f},
    {"music/game/haggstrom.ogg", 1.00f},
    {"music/game/key.ogg", 1.00f},
    {"music/game/living_mice.ogg", 1.00f},
    {"music/game/mice_on_venus.ogg", 1.00f},
    {"music/game/minecraft.ogg", 1.00f},
    {"music/game/oxygene.ogg", 1.00f},
    {"music/game/subwoofer_lullaby.ogg", 1.00f},
    {"music/game/sweden.ogg", 1.00f},
    {"music/game/wet_hands.ogg", 1.00f},
    {"music/game/watcher.ogg", 0.40f},
}};

constexpr std::array<MusicTrackDefinition, 3> kUnderwaterTracks{{
    {"music/game/water/axolotl.ogg", 1.00f},
    {"music/game/water/dragon_fish.ogg", 1.00f},
    {"music/game/water/shuniji.ogg", 1.00f},
}};
}  // namespace

std::span<const MusicTrackDefinition> musicTracksForContext(const MusicContext context)
{
    switch (context)
    {
    case MusicContext::Menu:
        return kMenuTracks;
    case MusicContext::OverworldNight:
        return kOverworldNightTracks;
    case MusicContext::Underwater:
        return kUnderwaterTracks;
    case MusicContext::OverworldDay:
    default:
        return kOverworldDayTracks;
    }
}

const char* musicContextName(const MusicContext context)
{
    switch (context)
    {
    case MusicContext::Menu:
        return "menu";
    case MusicContext::OverworldDay:
        return "overworld_day";
    case MusicContext::OverworldNight:
        return "overworld_night";
    case MusicContext::Underwater:
        return "underwater";
    default:
        return "unknown";
    }
}
}  // namespace vibecraft::audio
