#pragma once

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/audio/MusicCatalog.hpp"

namespace vibecraft::audio
{
class MusicDirector
{
  public:
    /// `stream` must stay valid until `shutdown()` (typically from SharedAudioOutput).
    bool initialize(SDL_AudioStream* stream, const std::filesystem::path& audioRoot);
    void shutdown();
    void update(float deltaTimeSeconds, MusicContext desiredContext);
    void setMasterGain(float gain);
    [[nodiscard]] float masterGain() const;

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
    void queueProceduralFallbackFrames(int frameCount);
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
    float streamGain_ = 0.85f;
    std::uint64_t proceduralPhase_ = 0;
    bool loggedMissingMusicAssets_ = false;
    /// Set when `pickAndDecodeNextTrack` exhausts all files for the current context (avoids re-decoding every refill).
    bool decodeExhaustedForContext_ = false;
    std::unordered_map<std::string, DecodedTrack> decodedTrackCache_;
    float decodeAttemptCooldownSeconds_ = 0.0f;
};
}  // namespace vibecraft::audio
