#pragma once

#include <SDL3/SDL_audio.h>

namespace vibecraft::audio
{
/// One logical playback device with two bound streams (music + SFX). Avoids opening the default device twice.
class SharedAudioOutput
{
  public:
    bool initialize();
    void shutdown();

    [[nodiscard]] SDL_AudioStream* musicStream() const
    {
        return musicStream_;
    }
    [[nodiscard]] SDL_AudioStream* sfxStream() const
    {
        return sfxStream_;
    }
    [[nodiscard]] bool ok() const
    {
        return deviceId_ != 0 && musicStream_ != nullptr && sfxStream_ != nullptr;
    }

  private:
    SDL_AudioDeviceID deviceId_ = 0;
    SDL_AudioStream* musicStream_ = nullptr;
    SDL_AudioStream* sfxStream_ = nullptr;
};
}  // namespace vibecraft::audio
