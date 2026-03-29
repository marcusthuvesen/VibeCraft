#include "vibecraft/audio/SoundEffects.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_set>

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
constexpr int kSfxImmediateQueueMaxMs = 85;

[[nodiscard]] std::vector<std::string> blockBreakOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
        return {
            "block/grass/break1.ogg",
            "block/grass/break2.ogg",
            "block/grass/break3.ogg",
            "block/grass/break4.ogg",
            "dig/grass1.ogg",
            "dig/grass2.ogg",
            "dig/grass3.ogg",
            "dig/grass4.ogg"};
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return {
            "block/sand/break1.ogg",
            "block/sand/break2.ogg",
            "block/sand/break3.ogg",
            "block/sand/break4.ogg",
            "block/sand/sand1.ogg",
            "block/sand/sand2.ogg",
            "block/sand/sand3.ogg",
            "block/sand/sand4.ogg"};
    case BlockType::TreeTrunk:
    case BlockType::OakPlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return {
            "block/wood/break1.ogg",
            "block/wood/break2.ogg",
            "block/wood/break3.ogg",
            "block/wood/break4.ogg",
            "dig/wood1.ogg",
            "dig/wood2.ogg",
            "dig/wood3.ogg",
            "dig/wood4.ogg"};
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
    case BlockType::Cobblestone:
    case BlockType::Oven:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Gravel:
    case BlockType::Glass:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return {
            "block/stone/break1.ogg",
            "block/stone/break2.ogg",
            "block/stone/break3.ogg",
            "block/stone/break4.ogg",
            "dig/stone1.ogg",
            "dig/stone2.ogg",
            "dig/stone3.ogg",
            "dig/stone4.ogg"};
    }
}

[[nodiscard]] std::vector<std::string> blockHitOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
        return {"block/grass/hit1.ogg", "block/grass/hit2.ogg", "block/grass/hit3.ogg", "block/grass/hit4.ogg"};
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return {"block/sand/hit1.ogg", "block/sand/hit2.ogg", "block/sand/hit3.ogg", "block/sand/hit4.ogg"};
    case BlockType::TreeTrunk:
    case BlockType::OakPlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return {"block/wood/hit1.ogg", "block/wood/hit2.ogg", "block/wood/hit3.ogg", "block/wood/hit4.ogg"};
    case BlockType::Deepslate:
        return {
            "block/deepslate/hit1.ogg",
            "block/deepslate/hit2.ogg",
            "block/deepslate/hit3.ogg",
            "block/deepslate/hit4.ogg"};
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::Oven:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Gravel:
    case BlockType::Glass:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return {"block/stone/hit1.ogg", "block/stone/hit2.ogg", "block/stone/hit3.ogg", "block/stone/hit4.ogg"};
    }
}

[[nodiscard]] std::vector<std::string> footstepOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    std::vector<std::string> step;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
        step.assign(
            {"block/grass/step1.ogg",
             "block/grass/step2.ogg",
             "block/grass/step3.ogg",
             "block/grass/step4.ogg",
             "block/grass/step5.ogg",
             "block/grass/step6.ogg",
             "dig/grass1.ogg",
             "dig/grass2.ogg",
             "dig/grass3.ogg",
             "dig/grass4.ogg"});
        break;
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        step.assign(
            {"block/sand/step1.ogg",
             "block/sand/step2.ogg",
             "block/sand/step3.ogg",
             "block/sand/step4.ogg",
             "block/sand/sand1.ogg",
             "block/sand/sand2.ogg",
             "block/sand/sand3.ogg",
             "block/sand/sand4.ogg"});
        break;
    case BlockType::Water:
        step.assign(
            {"liquid/swim1.ogg",
             "liquid/swim2.ogg",
             "liquid/swim3.ogg",
             "liquid/swim4.ogg",
             "liquid/swim5.ogg",
             "liquid/swim6.ogg",
             "liquid/swim7.ogg",
             "liquid/swim8.ogg",
             "liquid/swim9.ogg",
             "liquid/swim10.ogg",
             "liquid/swim11.ogg",
             "liquid/swim12.ogg",
             "liquid/swim13.ogg",
             "liquid/swim14.ogg",
             "liquid/swim15.ogg",
             "liquid/swim16.ogg",
             "liquid/swim17.ogg",
             "liquid/swim18.ogg",
             "liquid/splash.ogg",
             "liquid/splash2.ogg",
             "liquid/heavy_splash.ogg",
             "liquid/water.ogg"});
        break;
    case BlockType::TreeTrunk:
    case BlockType::OakPlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        step.assign(
            {"block/wood/step1.ogg",
             "block/wood/step2.ogg",
             "block/wood/step3.ogg",
             "block/wood/step4.ogg",
             "block/wood/step5.ogg",
             "block/wood/step6.ogg",
             "dig/wood1.ogg",
             "dig/wood2.ogg",
             "dig/wood3.ogg",
             "dig/wood4.ogg"});
        break;
    case BlockType::Deepslate:
        step.assign(
            {"block/deepslate/step1.ogg",
             "block/deepslate/step2.ogg",
             "block/deepslate/step3.ogg",
             "block/deepslate/step4.ogg",
             "block/deepslate/step5.ogg"});
        break;
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::Oven:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Gravel:
    case BlockType::Glass:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        step.assign(
            {"block/stone/step1.ogg",
             "block/stone/step2.ogg",
             "block/stone/step3.ogg",
             "block/stone/step4.ogg",
             "block/stone/step5.ogg",
             "block/stone/step6.ogg",
             "dig/stone1.ogg",
             "dig/stone2.ogg",
             "dig/stone3.ogg",
             "dig/stone4.ogg"});
        break;
    }

    if (blockType != BlockType::Water)
    {
        const std::vector<std::string> dig = blockBreakOptions(blockType);
        step.insert(step.end(), dig.begin(), dig.end());
    }
    return step;
}

[[nodiscard]] std::vector<std::string> blockPlaceOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    switch (blockType)
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::TreeCrown:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
        return {
            "block/grass/place1.ogg",
            "block/grass/place2.ogg",
            "block/grass/place3.ogg",
            "block/grass/place4.ogg",
            "dig/grass1.ogg",
            "dig/grass2.ogg",
            "dig/grass3.ogg",
            "dig/grass4.ogg"};
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return {
            "block/sand/place1.ogg",
            "block/sand/place2.ogg",
            "block/sand/place3.ogg",
            "block/sand/place4.ogg",
            "block/sand/sand5.ogg",
            "block/sand/sand6.ogg",
            "block/sand/sand7.ogg",
            "block/sand/sand8.ogg"};
    case BlockType::TreeTrunk:
    case BlockType::OakPlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return {
            "block/wood/place1.ogg",
            "block/wood/place2.ogg",
            "block/wood/place3.ogg",
            "block/wood/place4.ogg",
            "dig/wood1.ogg",
            "dig/wood2.ogg",
            "dig/wood3.ogg",
            "dig/wood4.ogg"};
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
    case BlockType::Cobblestone:
    case BlockType::Oven:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Gravel:
    case BlockType::Glass:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return {
            "block/stone/place1.ogg",
            "block/stone/place2.ogg",
            "block/stone/place3.ogg",
            "block/stone/place4.ogg",
            "dig/stone1.ogg",
            "dig/stone2.ogg",
            "dig/stone3.ogg",
            "dig/stone4.ogg"};
    }
}

[[nodiscard]] std::vector<std::string> playerAttackOptions()
{
    return {
        "entity/player/attack/crit1.ogg",
        "entity/player/attack/crit2.ogg",
        "entity/player/attack/crit3.ogg",
        "entity/player/attack/knockback1.ogg",
        "entity/player/attack/knockback2.ogg",
        "entity/player/attack/sweep1.ogg",
        "entity/player/attack/sweep2.ogg",
        "entity/player/attack/sweep3.ogg",
        "entity/player/attack/sweep4.ogg",
        "entity/player/attack/sweep5.ogg",
        "entity/player/attack/sweep6.ogg",
        "entity/player/attack/sweep7.ogg"};
}

[[nodiscard]] std::vector<std::string> playerHurtOptions()
{
    return {
        "entity/player/hurt1.ogg",
        "entity/player/hurt2.ogg",
        "entity/player/hurt3.ogg",
        "entity/player/hurt/fire_hurt1.ogg",
        "entity/player/hurt/fire_hurt2.ogg",
        "entity/player/hurt/fire_hurt3.ogg"};
}

[[nodiscard]] std::vector<std::string> playerJumpOptions()
{
    return {"entity/player/big_fall1.ogg", "entity/player/big_fall2.ogg", "entity/player/small_fall1.ogg"};
}

[[nodiscard]] std::vector<std::string> playerLandOptions(const bool hardLanding)
{
    if (hardLanding)
    {
        return {"entity/player/big_fall1.ogg", "entity/player/big_fall2.ogg"};
    }
    return {"entity/player/small_fall1.ogg", "entity/player/small_fall2.ogg", "entity/player/small_fall3.ogg"};
}

[[nodiscard]] std::vector<std::string> playerDeathOptions()
{
    return {"entity/player/death1.ogg", "entity/player/death2.ogg", "entity/player/death3.ogg"};
}

[[nodiscard]] std::vector<std::string> waterEnterOptions()
{
    // Minecraft 26.1 pack: assets/minecraft/sounds/liquid (see mcasset.cloud liquid folder).
    return {
        "entity/player/splash.high_speed.big1.ogg",
        "entity/player/splash.high_speed.big2.ogg",
        "liquid/splash.ogg",
        "liquid/splash2.ogg",
        "liquid/heavy_splash.ogg",
        "liquid/water.ogg"};
}

[[nodiscard]] std::vector<std::string> waterExitOptions()
{
    return {
        "entity/player/splash.high_speed.small1.ogg",
        "entity/player/splash.high_speed.small2.ogg",
        "liquid/splash.ogg",
        "liquid/splash2.ogg",
        "liquid/heavy_splash.ogg"};
}

[[nodiscard]] std::vector<std::string> mobHitOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::HostileStalker:
        return {
            "entity/zombie/hurt1.ogg",
            "entity/zombie/hurt2.ogg",
            "entity/zombie/hurt3.ogg",
            "entity/zombie/ambient1.ogg",
            "entity/zombie/ambient2.ogg",
            "mob/zombie/hurt1.ogg",
            "mob/zombie/hurt2.ogg",
            "mob/zombie/say1.ogg",
            "mob/zombie/say2.ogg"};
    case MobKind::Cow:
        return {"entity/cow/hurt1.ogg",
                "entity/cow/hurt2.ogg",
                "entity/cow/ambient1.ogg",
                "entity/cow/ambient2.ogg",
                "mob/cow/hurt1.ogg",
                "mob/cow/hurt2.ogg",
                "mob/cow/hurt3.ogg"};
    case MobKind::Pig:
        return {"entity/pig/hurt1.ogg",
                "entity/pig/hurt2.ogg",
                "entity/pig/ambient1.ogg",
                "entity/pig/ambient2.ogg",
                "mob/pig/say1.ogg",
                "mob/pig/say2.ogg",
                "mob/pig/say3.ogg"};
    case MobKind::Sheep:
        return {"entity/sheep/hurt1.ogg",
                "entity/sheep/hurt2.ogg",
                "entity/sheep/ambient1.ogg",
                "entity/sheep/ambient2.ogg",
                "mob/sheep/say1.ogg",
                "mob/sheep/say2.ogg",
                "mob/sheep/say3.ogg"};
    case MobKind::Chicken:
        return {"entity/chicken/hurt1.ogg",
                "entity/chicken/hurt2.ogg",
                "entity/chicken/ambient1.ogg",
                "entity/chicken/ambient2.ogg",
                "mob/chicken/hurt1.ogg",
                "mob/chicken/hurt2.ogg",
                "mob/chicken/say1.ogg",
                "mob/chicken/say2.ogg"};
    }
    return {};
}

[[nodiscard]] std::vector<std::string> mobDefeatOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::HostileStalker:
        return {"entity/zombie/death.ogg", "mob/zombie/death.ogg"};
    case MobKind::Cow:
        return {"entity/cow/death.ogg", "mob/cow/hurt1.ogg", "mob/cow/hurt2.ogg"};
    case MobKind::Pig:
        return {"entity/pig/death.ogg", "mob/pig/death.ogg"};
    case MobKind::Sheep:
        return {"entity/sheep/death.ogg", "mob/sheep/say1.ogg", "mob/sheep/say2.ogg"};
    case MobKind::Chicken:
        return {"entity/chicken/death.ogg", "mob/chicken/hurt1.ogg", "mob/chicken/hurt2.ogg"};
    }
    return {};
}

[[nodiscard]] std::vector<std::string> warmupClipPaths()
{
    std::vector<std::string> clips;
    const auto append = [&clips](const std::vector<std::string>& src)
    {
        clips.insert(clips.end(), src.begin(), src.end());
    };

    append(blockBreakOptions(vibecraft::world::BlockType::Stone));
    append(blockBreakOptions(vibecraft::world::BlockType::Grass));
    append(blockBreakOptions(vibecraft::world::BlockType::Sand));
    append(blockBreakOptions(vibecraft::world::BlockType::TreeTrunk));
    append(blockBreakOptions(vibecraft::world::BlockType::Deepslate));

    append(blockPlaceOptions(vibecraft::world::BlockType::Stone));
    append(blockPlaceOptions(vibecraft::world::BlockType::Grass));
    append(blockPlaceOptions(vibecraft::world::BlockType::Sand));
    append(blockPlaceOptions(vibecraft::world::BlockType::TreeTrunk));
    append(blockPlaceOptions(vibecraft::world::BlockType::Deepslate));

    append(footstepOptions(vibecraft::world::BlockType::Stone));
    append(footstepOptions(vibecraft::world::BlockType::Grass));
    append(footstepOptions(vibecraft::world::BlockType::Sand));
    append(footstepOptions(vibecraft::world::BlockType::TreeTrunk));
    append(footstepOptions(vibecraft::world::BlockType::Deepslate));
    append(footstepOptions(vibecraft::world::BlockType::Water));
    append(waterEnterOptions());
    append(waterExitOptions());

    append(playerAttackOptions());
    append(playerHurtOptions());
    append(playerJumpOptions());
    append(playerLandOptions(false));
    append(playerLandOptions(true));
    append(playerDeathOptions());

    using MK = vibecraft::game::MobKind;
    append(mobHitOptions(MK::HostileStalker));
    append(mobHitOptions(MK::Cow));
    append(mobHitOptions(MK::Pig));
    append(mobHitOptions(MK::Sheep));
    append(mobHitOptions(MK::Chicken));
    append(mobDefeatOptions(MK::HostileStalker));
    append(mobDefeatOptions(MK::Cow));
    append(mobDefeatOptions(MK::Pig));
    append(mobDefeatOptions(MK::Sheep));
    append(mobDefeatOptions(MK::Chicken));

    std::unordered_set<std::string> seen;
    std::vector<std::string> unique;
    unique.reserve(clips.size());
    for (const std::string& clip : clips)
    {
        if (seen.emplace(clip).second)
        {
            unique.push_back(clip);
        }
    }
    return unique;
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
    for (const std::string& clipPath : warmupClipPaths())
    {
        static_cast<void>(getOrLoadClip(clipPath));
    }
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

void SoundEffects::playBlockDigTick(const vibecraft::world::BlockType blockType)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> hit = blockHitOptions(blockType);
    if (!hit.empty() && getOrLoadClip(hit.front()) != nullptr)
    {
        playRandomClipWithGain(hit, 0.58f);
        return;
    }
    playRandomClipWithGain(blockBreakOptions(blockType), 0.58f);
}

void SoundEffects::playFootstep(const vibecraft::world::BlockType surfaceBlockType)
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(footstepOptions(surfaceBlockType), 0.32f);
}

void SoundEffects::playPlayerAttack()
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = playerAttackOptions();
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.52f);
        return;
    }
    queueProceduralSweep(760.0f, 310.0f, 0.055f, 0.19f, 0.22f);
}

void SoundEffects::playPlayerHurt()
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = playerHurtOptions();
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.65f);
        return;
    }
    queueProceduralSweep(280.0f, 130.0f, 0.095f, 0.33f, 0.26f);
}

void SoundEffects::playPlayerJump()
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = playerJumpOptions();
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.30f);
        return;
    }
    queueProceduralSweep(420.0f, 560.0f, 0.045f, 0.14f, 0.12f);
}

void SoundEffects::playPlayerLand(const bool hardLanding)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = playerLandOptions(hardLanding);
    if (!options.empty())
    {
        playRandomClipWithGain(options, hardLanding ? 0.55f : 0.34f);
        return;
    }
    queueProceduralSweep(240.0f, 95.0f, hardLanding ? 0.085f : 0.055f, hardLanding ? 0.28f : 0.16f, 0.32f);
}

void SoundEffects::playPlayerDeath()
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = playerDeathOptions();
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.74f);
        return;
    }
    queueProceduralSweep(190.0f, 52.0f, 0.18f, 0.45f, 0.30f);
}

void SoundEffects::playWaterEnter()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(waterEnterOptions(), 0.46f);
}

void SoundEffects::playWaterExit()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(waterExitOptions(), 0.42f);
}

void SoundEffects::playMobHit(const vibecraft::game::MobKind mobKind)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = mobHitOptions(mobKind);
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.85f);
        return;
    }
    queueProceduralSweep(210.0f, 110.0f, 0.085f, 0.34f, 0.34f);
}

void SoundEffects::playMobDefeat(const vibecraft::game::MobKind mobKind)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = mobDefeatOptions(mobKind);
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.92f);
        return;
    }
    queueProceduralSweep(180.0f, 70.0f, 0.14f, 0.38f, 0.28f);
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

void SoundEffects::queueProceduralFallbackClip()
{
    queueProceduralFallbackScaled(1.0f);
}

void SoundEffects::queueProceduralFallbackScaled(const float gain)
{
    if (stream_ == nullptr)
    {
        return;
    }

    constexpr int kSampleRate = 44100;
    constexpr int kFrames = kSampleRate * 4 / 100;
    std::array<float, static_cast<std::size_t>(kFrames) * 2> chunk{};
    constexpr float kTwoPi = 6.2831853f;
    const float g = std::clamp(gain, 0.0f, 4.0f);
    for (int i = 0; i < kFrames; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float env = 1.0f - static_cast<float>(i) / static_cast<float>((kFrames > 1) ? (kFrames - 1) : 1);
        const float s = 0.42f * g * env * std::sin(kTwoPi * 1400.0f * t);
        chunk[static_cast<std::size_t>(i) * 2] = s;
        chunk[static_cast<std::size_t>(i) * 2 + 1] = s;
    }

    if (!SDL_PutAudioStreamData(stream_, chunk.data(), static_cast<int>(chunk.size() * sizeof(float))))
    {
        core::logWarning(fmt::format("Failed queuing procedural SFX: {}", SDL_GetError()));
    }
    else
    {
        static_cast<void>(SDL_FlushAudioStream(stream_));
    }
}

void SoundEffects::queueProceduralSweep(
    const float startHz,
    const float endHz,
    const float durationSeconds,
    const float gain,
    const float noiseMix)
{
    if (stream_ == nullptr)
    {
        return;
    }

    const int frames = std::max(1, static_cast<int>(std::round(durationSeconds * static_cast<float>(kOutputSampleRate))));
    std::vector<float> chunk(static_cast<std::size_t>(frames) * 2U, 0.0f);
    constexpr float kTwoPi = 6.2831853f;
    float phase = 0.0f;
    const float clampedGain = std::clamp(gain, 0.0f, 1.5f);
    const float clampedNoiseMix = std::clamp(noiseMix, 0.0f, 1.0f);

    for (int i = 0; i < frames; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(std::max(1, frames - 1));
        const float env = std::pow(1.0f - t, 1.7f);
        const float hz = startHz + (endHz - startHz) * t;
        phase += kTwoPi * hz / static_cast<float>(kOutputSampleRate);
        const float tonal = std::sin(phase);
        const float noise = static_cast<float>(randomInclusive(-1000, 1000)) / 1000.0f;
        const float sample = (tonal * (1.0f - clampedNoiseMix) + noise * clampedNoiseMix * 0.75f) * env * clampedGain;
        chunk[static_cast<std::size_t>(i) * 2U] = sample;
        chunk[static_cast<std::size_t>(i) * 2U + 1U] = sample;
    }

    if (!SDL_PutAudioStreamData(stream_, chunk.data(), static_cast<int>(chunk.size() * sizeof(float))))
    {
        core::logWarning(fmt::format("Failed queuing procedural combat SFX: {}", SDL_GetError()));
    }
    else
    {
        static_cast<void>(SDL_FlushAudioStream(stream_));
    }
}

void SoundEffects::playRandomClip(const std::vector<std::string>& options)
{
    playRandomClipWithGain(options, 1.0f);
}

void SoundEffects::playRandomClipWithGain(const std::vector<std::string>& options, const float gain)
{
    if (stream_ == nullptr || options.empty())
    {
        return;
    }

    const int queuedBytes = SDL_GetAudioStreamQueued(stream_);
    if (queuedBytes < 0)
    {
        core::logWarning(fmt::format("Failed querying SFX queue size: {}", SDL_GetError()));
    }
    else
    {
        const int maxQueuedBytes = (kOutputSampleRate * kOutputChannelCount * static_cast<int>(sizeof(float))
            * kSfxImmediateQueueMaxMs) / 1000;
        if (queuedBytes > maxQueuedBytes && !SDL_ClearAudioStream(stream_))
        {
            core::logWarning(fmt::format("Failed clearing delayed SFX queue: {}", SDL_GetError()));
        }
    }

    const float g = std::clamp(gain, 0.0f, 4.0f);
    const int startClipIndex = randomInclusive(0, static_cast<int>(options.size()) - 1);
    const DecodedClip* clip = nullptr;
    for (std::size_t i = 0; i < options.size(); ++i)
    {
        const std::size_t clipIndex =
            (static_cast<std::size_t>(startClipIndex) + i) % options.size();
        const DecodedClip* const candidate = getOrLoadClip(options[clipIndex]);
        if (candidate != nullptr && !candidate->pcmF32Stereo.empty())
        {
            clip = candidate;
            break;
        }
    }
    if (clip == nullptr)
    {
        queueProceduralFallbackScaled(g);
        return;
    }

    if (g >= 0.999f && g <= 1.001f)
    {
        if (!SDL_PutAudioStreamData(
                stream_,
                clip->pcmF32Stereo.data(),
                static_cast<int>(clip->pcmF32Stereo.size() * sizeof(float))))
        {
            core::logWarning(fmt::format("Failed queuing SFX clip: {}", SDL_GetError()));
        }
        else
        {
            static_cast<void>(SDL_FlushAudioStream(stream_));
        }
        return;
    }

    thread_local std::vector<float> scaled;
    scaled.resize(clip->pcmF32Stereo.size());
    for (std::size_t i = 0; i < clip->pcmF32Stereo.size(); ++i)
    {
        scaled[i] = clip->pcmF32Stereo[i] * g;
    }

    if (!SDL_PutAudioStreamData(
            stream_,
            scaled.data(),
            static_cast<int>(scaled.size() * sizeof(float))))
    {
        core::logWarning(fmt::format("Failed queuing SFX clip: {}", SDL_GetError()));
    }
    else
    {
        static_cast<void>(SDL_FlushAudioStream(stream_));
    }
}

int SoundEffects::randomInclusive(const int minValue, const int maxValue)
{
    rngState_ = rngState_ * 1664525u + 1013904223u;
    const std::uint32_t range = static_cast<std::uint32_t>(maxValue - minValue + 1);
    return minValue + static_cast<int>(rngState_ % range);
}
}  // namespace vibecraft::audio
