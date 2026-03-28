#include "vibecraft/audio/SoundEffects.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
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

[[nodiscard]] std::vector<std::string> blockBreakOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
        return {"dig/grass1.ogg", "dig/grass2.ogg", "dig/grass3.ogg", "dig/grass4.ogg"};
    case BlockType::Sand:
        return {"block/sand/sand1.ogg", "block/sand/sand2.ogg", "block/sand/sand3.ogg", "block/sand/sand4.ogg"};
    case BlockType::TreeTrunk:
        return {"dig/wood1.ogg", "dig/wood2.ogg", "dig/wood3.ogg", "dig/wood4.ogg"};
    case BlockType::Deepslate:
        return {
            "block/deepslate/break1.ogg",
            "block/deepslate/break2.ogg",
            "block/deepslate/break3.ogg",
            "block/deepslate/break4.ogg"};
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return {"dig/stone1.ogg", "dig/stone2.ogg", "dig/stone3.ogg", "dig/stone4.ogg"};
    }
}

[[nodiscard]] std::vector<std::string> blockPlaceOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
        return {"dig/grass1.ogg", "dig/grass2.ogg", "dig/grass3.ogg", "dig/grass4.ogg"};
    case BlockType::Sand:
        return {"block/sand/sand5.ogg", "block/sand/sand6.ogg", "block/sand/sand7.ogg", "block/sand/sand8.ogg"};
    case BlockType::TreeTrunk:
        return {"dig/wood1.ogg", "dig/wood2.ogg", "dig/wood3.ogg", "dig/wood4.ogg"};
    case BlockType::Deepslate:
        return {
            "block/deepslate/place1.ogg",
            "block/deepslate/place2.ogg",
            "block/deepslate/place3.ogg",
            "block/deepslate/place4.ogg"};
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return {"dig/stone1.ogg", "dig/stone2.ogg", "dig/stone3.ogg", "dig/stone4.ogg"};
    }
}
}  // namespace

bool SoundEffects::initialize(SDL_AudioStream* const stream, const std::filesystem::path& audioRoot)
{
    shutdown();
    if (stream == nullptr)
    {
        core::logWarning("SFX disabled: null audio stream.");
        return false;
    }
    stream_ = stream;
    audioRoot_ = audioRoot;

    setMasterGain(streamGain_);
    clipCache_.clear();
    return true;
}

void SoundEffects::shutdown()
{
    if (stream_ != nullptr)
    {
        static_cast<void>(SDL_ClearAudioStream(stream_));
        stream_ = nullptr;
    }
    clipCache_.clear();
}

void SoundEffects::setMasterGain(const float gain)
{
    streamGain_ = std::clamp(gain, 0.0f, 1.0f);
    if (stream_ != nullptr && !SDL_SetAudioStreamGain(stream_, streamGain_))
    {
        core::logWarning(fmt::format("SFX gain change failed: {}", SDL_GetError()));
    }
}

float SoundEffects::masterGain() const
{
    return streamGain_;
}

void SoundEffects::playBlockBreak(const vibecraft::world::BlockType blockType)
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClip(blockBreakOptions(blockType));
}

void SoundEffects::playBlockPlace(const vibecraft::world::BlockType blockType)
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClip(blockPlaceOptions(blockType));
}

bool SoundEffects::decodeClip(const std::string& relativePath, DecodedClip& outClip)
{
    const std::filesystem::path clipPath = audioRoot_ / relativePath;
    const std::string clipPathUtf8 = clipPath.generic_string();

    int sourceChannels = 0;
    int sourceSampleRate = 0;
    short* sourcePcm16 = nullptr;
    const int decodedFrames =
        stb_vorbis_decode_filename(clipPathUtf8.c_str(), &sourceChannels, &sourceSampleRate, &sourcePcm16);
    if (decodedFrames <= 0 || sourcePcm16 == nullptr || sourceChannels <= 0 || sourceSampleRate <= 0)
    {
        core::logWarning(fmt::format("Missing/invalid SFX clip '{}'.", relativePath));
        return false;
    }

    std::vector<float> sourceStereo;
    sourceStereo.resize(static_cast<std::size_t>(decodedFrames) * kOutputChannelCount);
    constexpr float kI16ToFloat = 1.0f / 32768.0f;
    for (int frame = 0; frame < decodedFrames; ++frame)
    {
        const int sourceBase = frame * sourceChannels;
        const short leftSample = sourcePcm16[sourceBase];
        const short rightSample = sourcePcm16[sourceBase + (sourceChannels > 1 ? 1 : 0)];
        sourceStereo[static_cast<std::size_t>(frame) * 2] = static_cast<float>(leftSample) * kI16ToFloat;
        sourceStereo[static_cast<std::size_t>(frame) * 2 + 1] = static_cast<float>(rightSample) * kI16ToFloat;
    }

    std::free(sourcePcm16);
    sourcePcm16 = nullptr;

    if (sourceSampleRate == kOutputSampleRate)
    {
        outClip.pcmF32Stereo = std::move(sourceStereo);
        return true;
    }

    const std::size_t resampledFrameCount = static_cast<std::size_t>(std::max(
        1.0,
        std::ceil(
            static_cast<double>(decodedFrames) * static_cast<double>(kOutputSampleRate)
            / static_cast<double>(sourceSampleRate))));
    outClip.pcmF32Stereo.resize(resampledFrameCount * kOutputChannelCount);
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
            outClip.pcmF32Stereo[dstFrame * 2 + static_cast<std::size_t>(channel)] = s0 + (s1 - s0) * t;
        }
    }
    return true;
}

const SoundEffects::DecodedClip* SoundEffects::getOrLoadClip(const std::string& relativePath)
{
    const auto cachedIt = clipCache_.find(relativePath);
    if (cachedIt != clipCache_.end())
    {
        return &cachedIt->second;
    }

    DecodedClip decodedClip;
    if (!decodeClip(relativePath, decodedClip))
    {
        return nullptr;
    }
    const auto [insertedIt, inserted] = clipCache_.emplace(relativePath, std::move(decodedClip));
    if (!inserted)
    {
        return nullptr;
    }
    return &insertedIt->second;
}

void SoundEffects::playRandomClip(const std::vector<std::string>& options)
{
    if (stream_ == nullptr || options.empty())
    {
        return;
    }

    const int clipIndex = randomInclusive(0, static_cast<int>(options.size()) - 1);
    const DecodedClip* const clip = getOrLoadClip(options[static_cast<std::size_t>(clipIndex)]);
    if (clip == nullptr || clip->pcmF32Stereo.empty())
    {
        return;
    }

    if (!SDL_PutAudioStreamData(
            stream_,
            clip->pcmF32Stereo.data(),
            static_cast<int>(clip->pcmF32Stereo.size() * sizeof(float))))
    {
        core::logWarning(fmt::format("Failed queuing SFX clip: {}", SDL_GetError()));
    }
}

int SoundEffects::randomInclusive(const int minValue, const int maxValue)
{
    rngState_ = rngState_ * 1664525u + 1013904223u;
    const std::uint32_t range = static_cast<std::uint32_t>(maxValue - minValue + 1);
    return minValue + static_cast<int>(rngState_ % range);
}
}  // namespace vibecraft::audio
