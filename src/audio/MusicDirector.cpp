#include "vibecraft/audio/MusicDirector.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "vibecraft/core/Logger.hpp"

extern "C"
{
int stb_vorbis_decode_filename(const char* filename, int* channels, int* sample_rate, short** output);
}

namespace vibecraft::audio
{
namespace
{
constexpr int kOutputSampleRate = 44100;
constexpr int kOutputChannelCount = 2;
constexpr int kTargetQueuedSeconds = 8;

[[nodiscard]] int contextIndex(const MusicContext context)
{
    switch (context)
    {
    case MusicContext::Menu:
        return 0;
    case MusicContext::OverworldDay:
        return 1;
    case MusicContext::OverworldNight:
        return 2;
    case MusicContext::Underwater:
        return 3;
    default:
        return 0;
    }
}
}  // namespace

bool MusicDirector::initialize(SDL_AudioStream* const stream, const std::filesystem::path& audioRoot)
{
    shutdown();
    if (stream == nullptr)
    {
        core::logWarning("Music disabled: null audio stream.");
        return false;
    }
    stream_ = stream;
    audioRoot_ = audioRoot;

    setMasterGain(streamGain_);

    hasContext_ = false;
    hasActiveTrack_ = false;
    activeTrackFrameCursor_ = 0;
    queuedSilenceFrames_ = 0;
    std::fill(std::begin(lastTrackIndexByContext_), std::end(lastTrackIndexByContext_), -1);
    return true;
}

void MusicDirector::setMasterGain(const float gain)
{
    streamGain_ = std::clamp(gain, 0.0f, 1.0f);
    if (stream_ != nullptr && !SDL_SetAudioStreamGain(stream_, streamGain_))
    {
        core::logWarning(fmt::format("Music gain change failed: {}", SDL_GetError()));
    }
}

float MusicDirector::masterGain() const
{
    return streamGain_;
}

void MusicDirector::shutdown()
{
    if (stream_ != nullptr)
    {
        static_cast<void>(SDL_ClearAudioStream(stream_));
        stream_ = nullptr;
    }
    hasContext_ = false;
    hasActiveTrack_ = false;
    activeTrackFrameCursor_ = 0;
    queuedSilenceFrames_ = 0;
}

void MusicDirector::update(const float /*deltaTimeSeconds*/, const MusicContext desiredContext)
{
    if (stream_ == nullptr)
    {
        return;
    }

    if (!hasContext_ || desiredContext != context_)
    {
        const bool firstContext = !hasContext_;
        context_ = desiredContext;
        hasContext_ = true;
        hasActiveTrack_ = false;
        activeTrackFrameCursor_ = 0;
        if (firstContext)
        {
            queuedSilenceFrames_ = 0;
        }
        else
        {
            queuedSilenceFrames_ =
                randomInclusive(desiredContext == MusicContext::Menu ? 1 : 2, desiredContext == MusicContext::Menu ? 3 : 5)
                * kOutputSampleRate;
        }
        if (!SDL_ClearAudioStream(stream_))
        {
            core::logWarning(fmt::format("Failed to clear audio stream on context change: {}", SDL_GetError()));
        }
    }

    refillQueue();
}

bool MusicDirector::decodeTrack(const MusicTrackDefinition& definition, DecodedTrack& outTrack)
{
    const std::filesystem::path trackPath = audioRoot_ / definition.relativePath;
    const std::string trackPathUtf8 = trackPath.generic_string();

    int sourceChannels = 0;
    int sourceSampleRate = 0;
    short* sourcePcm16 = nullptr;
    const int decodedFrames =
        stb_vorbis_decode_filename(trackPathUtf8.c_str(), &sourceChannels, &sourceSampleRate, &sourcePcm16);
    if (decodedFrames <= 0 || sourcePcm16 == nullptr || sourceChannels <= 0 || sourceSampleRate <= 0)
    {
        core::logWarning(fmt::format(
            "Could not decode track '{}'; check audio asset copy path.",
            definition.relativePath));
        return false;
    }

    std::vector<float> sourceStereo;
    sourceStereo.resize(static_cast<std::size_t>(decodedFrames) * kOutputChannelCount);
    const float sampleGain = definition.gain / 32768.0f;
    for (int frame = 0; frame < decodedFrames; ++frame)
    {
        const int sourceBase = frame * sourceChannels;
        const short leftSample = sourcePcm16[sourceBase];
        const short rightSample = sourcePcm16[sourceBase + (sourceChannels > 1 ? 1 : 0)];
        sourceStereo[static_cast<std::size_t>(frame) * 2] = static_cast<float>(leftSample) * sampleGain;
        sourceStereo[static_cast<std::size_t>(frame) * 2 + 1] = static_cast<float>(rightSample) * sampleGain;
    }

    std::free(sourcePcm16);
    sourcePcm16 = nullptr;

    if (sourceSampleRate == kOutputSampleRate)
    {
        outTrack.relativePath = std::string(definition.relativePath);
        outTrack.gain = definition.gain;
        outTrack.pcmF32Stereo = std::move(sourceStereo);
        return true;
    }

    const std::size_t resampledFrameCount = static_cast<std::size_t>(
        std::max(1.0, std::ceil(
                          static_cast<double>(decodedFrames) * static_cast<double>(kOutputSampleRate)
                          / static_cast<double>(sourceSampleRate))));
    std::vector<float> resampledStereo;
    resampledStereo.resize(resampledFrameCount * kOutputChannelCount);

    const double sourcePerDest = static_cast<double>(sourceSampleRate) / static_cast<double>(kOutputSampleRate);
    for (std::size_t dstFrame = 0; dstFrame < resampledFrameCount; ++dstFrame)
    {
        const double sourceFramePos = static_cast<double>(dstFrame) * sourcePerDest;
        const std::size_t sourceFrame0 = static_cast<std::size_t>(sourceFramePos);
        const std::size_t sourceFrame1 =
            std::min(sourceFrame0 + 1, static_cast<std::size_t>(decodedFrames - 1));
        const float t = static_cast<float>(sourceFramePos - static_cast<double>(sourceFrame0));

        for (int channel = 0; channel < kOutputChannelCount; ++channel)
        {
            const float s0 = sourceStereo[sourceFrame0 * 2 + static_cast<std::size_t>(channel)];
            const float s1 = sourceStereo[sourceFrame1 * 2 + static_cast<std::size_t>(channel)];
            resampledStereo[dstFrame * 2 + static_cast<std::size_t>(channel)] = s0 + (s1 - s0) * t;
        }
    }

    outTrack.relativePath = std::string(definition.relativePath);
    outTrack.gain = definition.gain;
    outTrack.pcmF32Stereo = std::move(resampledStereo);
    return true;
}

bool MusicDirector::pickAndDecodeNextTrack(const MusicContext context)
{
    const std::span<const MusicTrackDefinition> tracks = musicTracksForContext(context);
    if (tracks.empty())
    {
        return false;
    }

    const int contextIdx = contextIndex(context);
    const int previousTrackIndex = lastTrackIndexByContext_[contextIdx];
    const int startIndex = randomInclusive(0, static_cast<int>(tracks.size()) - 1);

    for (std::size_t offset = 0; offset < tracks.size(); ++offset)
    {
        const int candidateIndex = static_cast<int>((static_cast<std::size_t>(startIndex) + offset) % tracks.size());
        if (tracks.size() > 1 && candidateIndex == previousTrackIndex)
        {
            continue;
        }

        DecodedTrack decodedTrack;
        if (!decodeTrack(tracks[static_cast<std::size_t>(candidateIndex)], decodedTrack))
        {
            continue;
        }

        activeTrack_ = std::move(decodedTrack);
        hasActiveTrack_ = true;
        activeTrackFrameCursor_ = 0;
        lastTrackIndexByContext_[contextIdx] = candidateIndex;
        return true;
    }

    return false;
}

void MusicDirector::queueSilenceFrames(int frameCount)
{
    if (stream_ == nullptr || frameCount <= 0)
    {
        return;
    }

    constexpr int kSilenceChunkFrames = 1024;
    static const std::array<float, kSilenceChunkFrames * kOutputChannelCount> kZeroedChunk{};

    int remaining = frameCount;
    while (remaining > 0)
    {
        const int chunkFrames = std::min(remaining, kSilenceChunkFrames);
        if (!SDL_PutAudioStreamData(stream_, kZeroedChunk.data(), chunkFrames * kOutputChannelCount * static_cast<int>(sizeof(float))))
        {
            core::logWarning(fmt::format("Failed queuing silence: {}", SDL_GetError()));
            return;
        }
        remaining -= chunkFrames;
    }
}

void MusicDirector::queueTrackAudioFrames(const int frameCount)
{
    if (!hasActiveTrack_ || stream_ == nullptr || frameCount <= 0)
    {
        return;
    }

    const std::size_t totalFrames = activeTrack_.pcmF32Stereo.size() / kOutputChannelCount;
    if (activeTrackFrameCursor_ >= totalFrames)
    {
        hasActiveTrack_ = false;
        activeTrackFrameCursor_ = 0;
        scheduleGapAfterTrack(context_);
        return;
    }

    const std::size_t availableFrames = totalFrames - activeTrackFrameCursor_;
    const std::size_t framesToQueue = std::min<std::size_t>(availableFrames, static_cast<std::size_t>(frameCount));
    const float* source = activeTrack_.pcmF32Stereo.data() + activeTrackFrameCursor_ * kOutputChannelCount;
    const int byteCount = static_cast<int>(framesToQueue * kOutputChannelCount * sizeof(float));
    if (!SDL_PutAudioStreamData(stream_, source, byteCount))
    {
        core::logWarning(fmt::format("Failed queuing track audio '{}': {}", activeTrack_.relativePath, SDL_GetError()));
        return;
    }

    activeTrackFrameCursor_ += framesToQueue;
    if (activeTrackFrameCursor_ >= totalFrames)
    {
        hasActiveTrack_ = false;
        activeTrackFrameCursor_ = 0;
        scheduleGapAfterTrack(context_);
    }
}

void MusicDirector::refillQueue()
{
    if (stream_ == nullptr)
    {
        return;
    }

    const int targetQueuedFrames = kOutputSampleRate * kTargetQueuedSeconds;
    int queuedBytes = SDL_GetAudioStreamQueued(stream_);
    if (queuedBytes < 0)
    {
        core::logWarning(fmt::format("Failed querying audio queue size: {}", SDL_GetError()));
        return;
    }

    int queuedFrames = queuedBytes / static_cast<int>(sizeof(float) * kOutputChannelCount);
    while (queuedFrames < targetQueuedFrames)
    {
        const int refillFrames = std::min(targetQueuedFrames - queuedFrames, kOutputSampleRate);
        if (queuedSilenceFrames_ > 0)
        {
            const int silenceFrames = std::min(refillFrames, queuedSilenceFrames_);
            queueSilenceFrames(silenceFrames);
            queuedSilenceFrames_ -= silenceFrames;
            queuedFrames += silenceFrames;
            continue;
        }

        if (!hasActiveTrack_ && !pickAndDecodeNextTrack(context_))
        {
            // Keep output alive even if assets are missing.
            queueSilenceFrames(kOutputSampleRate);
            queuedFrames += kOutputSampleRate;
            continue;
        }

        const std::size_t beforeCursor = activeTrackFrameCursor_;
        queueTrackAudioFrames(refillFrames);
        const std::size_t queuedTrackFrames = activeTrackFrameCursor_ >= beforeCursor
            ? activeTrackFrameCursor_ - beforeCursor
            : 0;
        if (queuedTrackFrames == 0)
        {
            queueSilenceFrames(kOutputSampleRate / 2);
            queuedFrames += kOutputSampleRate / 2;
        }
        else
        {
            queuedFrames += static_cast<int>(queuedTrackFrames);
        }
    }
}

void MusicDirector::scheduleGapAfterTrack(const MusicContext context)
{
    int minSeconds = 30;
    int maxSeconds = 100;
    switch (context)
    {
    case MusicContext::Menu:
        minSeconds = 8;
        maxSeconds = 25;
        break;
    case MusicContext::Underwater:
        minSeconds = 12;
        maxSeconds = 32;
        break;
    case MusicContext::OverworldDay:
        minSeconds = 35;
        maxSeconds = 120;
        break;
    case MusicContext::OverworldNight:
        minSeconds = 25;
        maxSeconds = 80;
        break;
    default:
        break;
    }
    queuedSilenceFrames_ += randomInclusive(minSeconds, maxSeconds) * kOutputSampleRate;
}

int MusicDirector::randomInclusive(const int minValue, const int maxValue)
{
    rngState_ = rngState_ * 1664525u + 1013904223u;
    const std::uint32_t range = static_cast<std::uint32_t>(maxValue - minValue + 1);
    return minValue + static_cast<int>(rngState_ % range);
}
}  // namespace vibecraft::audio
