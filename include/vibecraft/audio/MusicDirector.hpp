#pragma once

#include <SDL3/SDL_audio.h>

#include <filesystem>
#include <string>
#include <vector>

#include "vibecraft/audio/MusicCatalog.hpp"

namespace vibecraft::audio
{
class MusicDirector
{
  public:
    bool initialize(const std::filesystem::path& audioRoot);
    void shutdown();
    void update(float deltaTimeSeconds, MusicContext desiredContext);

    [[nodiscard]] bool initialized() const
    {
        return stream_ != nullptr;
    }

  private:
    struct DecodedTrack
    {
        std::string relativePath;
        float gain = 1.0f;
        std::vector<float> pcmF32Stereo;
    };

    [[nodiscard]] bool decodeTrack(const MusicTrackDefinition& definition, DecodedTrack& outTrack);
    [[nodiscard]] bool pickAndDecodeNextTrack(MusicContext context);
    void queueSilenceFrames(int frameCount);
    void queueTrackAudioFrames(int frameCount);
    void refillQueue();
    void scheduleGapAfterTrack(MusicContext context);
    [[nodiscard]] int randomInclusive(int minValue, int maxValue);

    std::filesystem::path audioRoot_;
    SDL_AudioStream* stream_ = nullptr;
    MusicContext context_ = MusicContext::Menu;
    bool hasContext_ = false;

    DecodedTrack activeTrack_{};
    bool hasActiveTrack_ = false;
    std::size_t activeTrackFrameCursor_ = 0;
    int queuedSilenceFrames_ = 0;
    int lastTrackIndexByContext_[4] = {-1, -1, -1, -1};
    std::uint32_t rngState_ = 0x91e10da5u;
};
}  // namespace vibecraft::audio
