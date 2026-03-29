#pragma once

#include <SDL3/SDL_audio.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "vibecraft/game/MobTypes.hpp"
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
    /// Short dig/hit tick while mining (quieter than full break).
    void playBlockDigTick(vibecraft::world::BlockType blockType);
    /// Footstep on the given surface block (grass, stone, sand, …).
    void playFootstep(vibecraft::world::BlockType surfaceBlockType);
    /// Enter/exit water transition cues from Minecraft ambient underwater pack.
    void playWaterEnter();
    void playWaterExit();
    /// Short melee whoosh on a successful player attack.
    void playPlayerAttack();
    /// Player hurt cue for fall/lava/drowning/combat damage.
    void playPlayerHurt();
    /// Jump takeoff cue from player movement set.
    void playPlayerJump();
    /// Landing cue (uses stronger sample selection for hard landings).
    void playPlayerLand(bool hardLanding);
    /// One-shot death cue before respawn.
    void playPlayerDeath();
    /// Mob damage impact cue.
    void playMobHit(vibecraft::game::MobKind mobKind);
    /// Slightly heavier cue when a mob dies.
    void playMobDefeat(vibecraft::game::MobKind mobKind);

  private:
    struct DecodedClip
    {
        std::vector<float> pcmF32Stereo;
    };

    [[nodiscard]] bool decodeClip(const std::string& relativePath, DecodedClip& outClip);
    [[nodiscard]] const DecodedClip* getOrLoadClip(const std::string& relativePath);
    void playRandomClip(const std::vector<std::string>& options);
    void playRandomClipWithGain(const std::vector<std::string>& options, float gain);
    void queueProceduralFallbackClip();
    void queueProceduralFallbackScaled(float gain);
    void queueProceduralSweep(float startHz, float endHz, float durationSeconds, float gain, float noiseMix);
    [[nodiscard]] int randomInclusive(int minValue, int maxValue);

    std::filesystem::path audioRoot_;
    SDL_AudioStream* stream_ = nullptr;
    float streamGain_ = 1.0f;
    std::unordered_map<std::string, DecodedClip> clipCache_;
    std::uint32_t rngState_ = 0x7151a42du;
};
}  // namespace vibecraft::audio
