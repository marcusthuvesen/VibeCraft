#include "vibecraft/audio/SharedAudioOutput.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include "vibecraft/core/Logger.hpp"

namespace vibecraft::audio
{
namespace
{
constexpr int kAppChannels = 2;
constexpr int kAppFrequency = 44100;
}  // namespace

bool SharedAudioOutput::initialize()
{
    shutdown();

    SDL_AudioSpec hint{};
    hint.format = SDL_AUDIO_F32;
    hint.channels = kAppChannels;
    hint.freq = kAppFrequency;

    deviceId_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &hint);
    if (deviceId_ == 0)
    {
        deviceId_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    }
    if (deviceId_ == 0)
    {
        core::logWarning(fmt::format("Shared audio: SDL_OpenAudioDevice failed ({})", SDL_GetError()));
        return false;
    }

    SDL_AudioSpec deviceFormat{};
    if (!SDL_GetAudioDeviceFormat(deviceId_, &deviceFormat, nullptr))
    {
        core::logWarning(fmt::format("Shared audio: SDL_GetAudioDeviceFormat failed ({})", SDL_GetError()));
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
        return false;
    }

    SDL_AudioSpec appFormat{};
    appFormat.format = SDL_AUDIO_F32;
    appFormat.channels = kAppChannels;
    appFormat.freq = kAppFrequency;

    musicStream_ = SDL_CreateAudioStream(&appFormat, &deviceFormat);
    sfxStream_ = SDL_CreateAudioStream(&appFormat, &deviceFormat);
    if (musicStream_ == nullptr || sfxStream_ == nullptr)
    {
        core::logWarning(fmt::format("Shared audio: SDL_CreateAudioStream failed ({})", SDL_GetError()));
        if (musicStream_ != nullptr)
        {
            SDL_DestroyAudioStream(musicStream_);
            musicStream_ = nullptr;
        }
        if (sfxStream_ != nullptr)
        {
            SDL_DestroyAudioStream(sfxStream_);
            sfxStream_ = nullptr;
        }
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
        return false;
    }

    if (!SDL_BindAudioStream(deviceId_, musicStream_) || !SDL_BindAudioStream(deviceId_, sfxStream_))
    {
        core::logWarning(fmt::format("Shared audio: SDL_BindAudioStream failed ({})", SDL_GetError()));
        SDL_DestroyAudioStream(musicStream_);
        SDL_DestroyAudioStream(sfxStream_);
        musicStream_ = nullptr;
        sfxStream_ = nullptr;
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
        return false;
    }

    if (!SDL_ResumeAudioDevice(deviceId_))
    {
        core::logWarning(fmt::format("Shared audio: SDL_ResumeAudioDevice failed ({})", SDL_GetError()));
        SDL_DestroyAudioStream(musicStream_);
        SDL_DestroyAudioStream(sfxStream_);
        musicStream_ = nullptr;
        sfxStream_ = nullptr;
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
        return false;
    }

    return true;
}

void SharedAudioOutput::shutdown()
{
    if (musicStream_ != nullptr)
    {
        SDL_DestroyAudioStream(musicStream_);
        musicStream_ = nullptr;
    }
    if (sfxStream_ != nullptr)
    {
        SDL_DestroyAudioStream(sfxStream_);
        sfxStream_ = nullptr;
    }
    if (deviceId_ != 0)
    {
        SDL_CloseAudioDevice(deviceId_);
        deviceId_ = 0;
    }
}
}  // namespace vibecraft::audio
