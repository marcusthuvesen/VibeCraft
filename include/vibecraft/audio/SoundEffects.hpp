#pragma once

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::audio
{
class SoundEffects
{
  public:
    bool initialize(SDL_AudioStream* stream, const std::filesystem::path& audioRoot);
    void shutdown();

    void setMasterGain(float gain);
    [[nodiscard]] float masterGain() const;

    void playBlockBreak(vibecraft::world::BlockType blockType);
    void playBlockPlace(vibecraft::world::BlockType blockType);

  private:
    struct DecodedClip
    {
        std::vector<float> pcmF32Stereo;
    };

    [[nodiscard]] bool decodeClip(const std::string& relativePath, DecodedClip& outClip);
    [[nodiscard]] const DecodedClip* getOrLoadClip(const std::string& relativePath);
    void playRandomClip(const std::vector<std::string>& options);
    [[nodiscard]] int randomInclusive(int minValue, int maxValue);

    std::filesystem::path audioRoot_;
    SDL_AudioStream* stream_ = nullptr;
    float streamGain_ = 1.0f;
    std::unordered_map<std::string, DecodedClip> clipCache_;
    std::uint32_t rngState_ = 0x7151a42du;
};
}  // namespace vibecraft::audio
