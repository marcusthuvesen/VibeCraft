#include "vibecraft/audio/SoundEffects.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
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

[[nodiscard]] std::vector<std::string> numberedClipRange(
    const std::string_view prefix,
    const int first,
    const int last)
{
    std::vector<std::string> clips;
    if (first > last)
    {
        return clips;
    }
    clips.reserve(static_cast<std::size_t>(last - first + 1));
    for (int index = first; index <= last; ++index)
    {
        clips.push_back(fmt::format("{}{}.ogg", prefix, index));
    }
    return clips;
}

[[nodiscard]] std::vector<std::string> concatClipLists(std::initializer_list<std::vector<std::string>> groups)
{
    std::size_t clipCount = 0;
    for (const std::vector<std::string>& group : groups)
    {
        clipCount += group.size();
    }

    std::vector<std::string> clips;
    clips.reserve(clipCount);
    for (const std::vector<std::string>& group : groups)
    {
        clips.insert(clips.end(), group.begin(), group.end());
    }
    return clips;
}

[[nodiscard]] std::vector<std::string> grassBreakClips()
{
    return numberedClipRange("dig/grass", 1, 4);
}

[[nodiscard]] std::vector<std::string> grassHitClips()
{
    return numberedClipRange("step/grass", 1, 6);
}

[[nodiscard]] std::vector<std::string> grassPlaceClips()
{
    return grassBreakClips();
}

[[nodiscard]] std::vector<std::string> grassStepClips()
{
    return grassHitClips();
}

[[nodiscard]] std::vector<std::string> mossBreakClips()
{
    return numberedClipRange("block/moss/break", 1, 5);
}

[[nodiscard]] std::vector<std::string> mossHitClips()
{
    return numberedClipRange("block/moss/step", 1, 6);
}

[[nodiscard]] std::vector<std::string> mossPlaceClips()
{
    return numberedClipRange("block/moss/break", 1, 5);
}

[[nodiscard]] std::vector<std::string> mossStepClips()
{
    return numberedClipRange("block/moss/step", 1, 6);
}

[[nodiscard]] std::vector<std::string> sandBreakClips()
{
    return numberedClipRange("dig/sand", 1, 4);
}

[[nodiscard]] std::vector<std::string> sandHitClips()
{
    return numberedClipRange("step/sand", 1, 5);
}

[[nodiscard]] std::vector<std::string> sandPlaceClips()
{
    return sandBreakClips();
}

[[nodiscard]] std::vector<std::string> sandStepClips()
{
    return sandHitClips();
}

[[nodiscard]] std::vector<std::string> woodBreakClips()
{
    return numberedClipRange("dig/wood", 1, 4);
}

[[nodiscard]] std::vector<std::string> woodHitClips()
{
    return numberedClipRange("step/wood", 1, 6);
}

[[nodiscard]] std::vector<std::string> woodPlaceClips()
{
    return woodBreakClips();
}

[[nodiscard]] std::vector<std::string> woodStepClips()
{
    return woodHitClips();
}

[[nodiscard]] std::vector<std::string> stoneBreakClips()
{
    return numberedClipRange("dig/stone", 1, 4);
}

[[nodiscard]] std::vector<std::string> stoneHitClips()
{
    return numberedClipRange("step/stone", 1, 6);
}

[[nodiscard]] std::vector<std::string> stonePlaceClips()
{
    return stoneBreakClips();
}

[[nodiscard]] std::vector<std::string> stoneStepClips()
{
    return stoneHitClips();
}

[[nodiscard]] std::vector<std::string> gravelBreakClips()
{
    return numberedClipRange("dig/gravel", 1, 4);
}

[[nodiscard]] std::vector<std::string> gravelHitClips()
{
    return numberedClipRange("step/gravel", 1, 4);
}

[[nodiscard]] std::vector<std::string> gravelPlaceClips()
{
    return gravelBreakClips();
}

[[nodiscard]] std::vector<std::string> gravelStepClips()
{
    return gravelHitClips();
}

[[nodiscard]] std::vector<std::string> deepslateBreakClips()
{
    return numberedClipRange("block/deepslate/break", 1, 4);
}

[[nodiscard]] std::vector<std::string> deepslateHitClips()
{
    return numberedClipRange("block/deepslate/step", 1, 6);
}

[[nodiscard]] std::vector<std::string> deepslatePlaceClips()
{
    return numberedClipRange("block/deepslate/place", 1, 6);
}

[[nodiscard]] std::vector<std::string> deepslateStepClips()
{
    return numberedClipRange("block/deepslate/step", 1, 6);
}

[[nodiscard]] std::vector<std::string> glassBreakClips()
{
    return {"random/glass1.ogg", "random/glass2.ogg", "random/glass3.ogg"};
}

[[nodiscard]] std::vector<std::string> blockBreakOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    if (vibecraft::world::isWoodDoorBlock(blockType))
    {
        return woodBreakClips();
    }
    if (vibecraft::world::isMetalDoorBlock(blockType))
    {
        return stoneBreakClips();
    }

    switch (vibecraft::world::normalizePlaceVariantBlockType(blockType))
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::OakLeaves:
    case BlockType::JungleLeaves:
    case BlockType::SpruceLeaves:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
    case BlockType::DeadBush:
    case BlockType::GrassTuft:
    case BlockType::FlowerTuft:
    case BlockType::DryTuft:
    case BlockType::LushTuft:
    case BlockType::FrostTuft:
    case BlockType::SparseTuft:
    case BlockType::CloverTuft:
    case BlockType::SproutTuft:
    case BlockType::Vines:
    case BlockType::CocoaPod:
    case BlockType::Melon:
    case BlockType::Bamboo:
        return grassBreakClips();
    case BlockType::MossBlock:
        return mossBreakClips();
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return sandBreakClips();
    case BlockType::OakLog:
    case BlockType::JungleLog:
    case BlockType::SpruceLog:
    case BlockType::OakPlanks:
    case BlockType::JunglePlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return woodBreakClips();
    case BlockType::Deepslate:
        return deepslateBreakClips();
    case BlockType::Gravel:
        return gravelBreakClips();
    case BlockType::Glass:
        return glassBreakClips();
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::MossyCobblestone:
    case BlockType::Furnace:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return stoneBreakClips();
    }
}

[[nodiscard]] std::vector<std::string> blockHitOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    if (vibecraft::world::isWoodDoorBlock(blockType))
    {
        return woodHitClips();
    }
    if (vibecraft::world::isMetalDoorBlock(blockType))
    {
        return stoneHitClips();
    }

    switch (vibecraft::world::normalizePlaceVariantBlockType(blockType))
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::OakLeaves:
    case BlockType::JungleLeaves:
    case BlockType::SpruceLeaves:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
    case BlockType::DeadBush:
    case BlockType::GrassTuft:
    case BlockType::FlowerTuft:
    case BlockType::DryTuft:
    case BlockType::LushTuft:
    case BlockType::FrostTuft:
    case BlockType::SparseTuft:
    case BlockType::CloverTuft:
    case BlockType::SproutTuft:
    case BlockType::Vines:
    case BlockType::CocoaPod:
    case BlockType::Melon:
    case BlockType::Bamboo:
        return grassHitClips();
    case BlockType::MossBlock:
        return mossHitClips();
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return sandHitClips();
    case BlockType::OakLog:
    case BlockType::JungleLog:
    case BlockType::SpruceLog:
    case BlockType::OakPlanks:
    case BlockType::JunglePlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return woodHitClips();
    case BlockType::Deepslate:
        return deepslateHitClips();
    case BlockType::Gravel:
        return gravelHitClips();
    case BlockType::Glass:
        return stoneHitClips();
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::MossyCobblestone:
    case BlockType::Furnace:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return stoneHitClips();
    }
}

[[nodiscard]] std::vector<std::string> footstepOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    std::vector<std::string> step;
    if (vibecraft::world::isWoodDoorBlock(blockType))
    {
        return woodStepClips();
    }
    if (vibecraft::world::isMetalDoorBlock(blockType))
    {
        return stoneStepClips();
    }

    switch (vibecraft::world::normalizePlaceVariantBlockType(blockType))
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::OakLeaves:
    case BlockType::JungleLeaves:
    case BlockType::SpruceLeaves:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
    case BlockType::DeadBush:
    case BlockType::GrassTuft:
    case BlockType::FlowerTuft:
    case BlockType::DryTuft:
    case BlockType::LushTuft:
    case BlockType::FrostTuft:
    case BlockType::SparseTuft:
    case BlockType::CloverTuft:
    case BlockType::SproutTuft:
    case BlockType::Vines:
    case BlockType::CocoaPod:
    case BlockType::Melon:
    case BlockType::Bamboo:
        step = grassStepClips();
        break;
    case BlockType::MossBlock:
        step = mossStepClips();
        break;
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        step = sandStepClips();
        break;
    case BlockType::Water:
        step = concatClipLists({numberedClipRange("liquid/swim", 1, 18), {"liquid/water.ogg"}});
        break;
    case BlockType::OakLog:
    case BlockType::JungleLog:
    case BlockType::SpruceLog:
    case BlockType::OakPlanks:
    case BlockType::JunglePlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        step = woodStepClips();
        break;
    case BlockType::Deepslate:
        step = deepslateStepClips();
        break;
    case BlockType::Gravel:
        step = gravelStepClips();
        break;
    case BlockType::Glass:
        step = stoneStepClips();
        break;
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::MossyCobblestone:
    case BlockType::Furnace:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        step = stoneStepClips();
        break;
    }
    return step;
}

[[nodiscard]] std::vector<std::string> blockPlaceOptions(const vibecraft::world::BlockType blockType)
{
    using vibecraft::world::BlockType;
    if (vibecraft::world::isWoodDoorBlock(blockType))
    {
        return woodPlaceClips();
    }
    if (vibecraft::world::isMetalDoorBlock(blockType))
    {
        return stonePlaceClips();
    }

    switch (vibecraft::world::normalizePlaceVariantBlockType(blockType))
    {
    case BlockType::Grass:
    case BlockType::SnowGrass:
    case BlockType::JungleGrass:
    case BlockType::Dirt:
    case BlockType::OakLeaves:
    case BlockType::JungleLeaves:
    case BlockType::SpruceLeaves:
    case BlockType::Dandelion:
    case BlockType::Poppy:
    case BlockType::BlueOrchid:
    case BlockType::Allium:
    case BlockType::OxeyeDaisy:
    case BlockType::BrownMushroom:
    case BlockType::RedMushroom:
    case BlockType::Cactus:
    case BlockType::DeadBush:
    case BlockType::GrassTuft:
    case BlockType::FlowerTuft:
    case BlockType::DryTuft:
    case BlockType::LushTuft:
    case BlockType::FrostTuft:
    case BlockType::SparseTuft:
    case BlockType::CloverTuft:
    case BlockType::SproutTuft:
    case BlockType::Vines:
    case BlockType::CocoaPod:
    case BlockType::Melon:
    case BlockType::Bamboo:
        return grassPlaceClips();
    case BlockType::MossBlock:
        return mossPlaceClips();
    case BlockType::Sand:
    case BlockType::Sandstone:
    case BlockType::TNT:
        return sandPlaceClips();
    case BlockType::OakLog:
    case BlockType::JungleLog:
    case BlockType::SpruceLog:
    case BlockType::OakPlanks:
    case BlockType::JunglePlanks:
    case BlockType::CraftingTable:
    case BlockType::Chest:
    case BlockType::Torch:
    case BlockType::Bookshelf:
        return woodPlaceClips();
    case BlockType::Deepslate:
        return deepslatePlaceClips();
    case BlockType::Gravel:
        return gravelPlaceClips();
    case BlockType::Glass:
        return stonePlaceClips();
    case BlockType::CoalOre:
    case BlockType::IronOre:
    case BlockType::GoldOre:
    case BlockType::DiamondOre:
    case BlockType::EmeraldOre:
    case BlockType::Cobblestone:
    case BlockType::MossyCobblestone:
    case BlockType::Furnace:
    case BlockType::Bricks:
    case BlockType::Glowstone:
    case BlockType::Obsidian:
    case BlockType::Bedrock:
    case BlockType::Stone:
    default:
        return stonePlaceClips();
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
    return concatClipLists(
        {numberedClipRange("damage/hit", 1, 3), numberedClipRange("entity/player/hurt/fire_hurt", 1, 3)});
}

[[nodiscard]] std::vector<std::string> playerJumpOptions()
{
    return {"random/breath.ogg"};
}

[[nodiscard]] std::vector<std::string> playerLandOptions(const bool hardLanding)
{
    if (hardLanding)
    {
        return {"damage/fallbig.ogg"};
    }
    return {"damage/fallsmall.ogg"};
}

[[nodiscard]] std::vector<std::string> playerDeathOptions()
{
    return numberedClipRange("damage/hit", 1, 3);
}

[[nodiscard]] std::vector<std::string> uiClickOptions()
{
    return {"random/click.ogg", "random/click_stereo.ogg", "random/wood_click.ogg"};
}

[[nodiscard]] std::vector<std::string> chestOpenOptions()
{
    return {"random/chestopen.ogg"};
}

[[nodiscard]] std::vector<std::string> chestCloseOptions()
{
    return {"random/chestclosed.ogg"};
}

[[nodiscard]] std::vector<std::string> itemConsumeOptions()
{
    return {"random/eat1.ogg", "random/eat2.ogg", "random/eat3.ogg", "random/drink.ogg"};
}

[[nodiscard]] std::vector<std::string> waterEnterOptions()
{
    return {"liquid/heavy_splash.ogg", "liquid/splash.ogg", "liquid/splash2.ogg", "liquid/water.ogg"};
}

[[nodiscard]] std::vector<std::string> waterExitOptions()
{
    return {"liquid/splash.ogg", "liquid/splash2.ogg", "liquid/water.ogg"};
}

[[nodiscard]] std::vector<std::string> mobHitOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::Zombie:
    case MobKind::Skeleton:
    case MobKind::Creeper:
    case MobKind::Spider:
        return concatClipLists({numberedClipRange("mob/zombie/hurt", 1, 2), numberedClipRange("mob/zombie/say", 1, 3)});
    case MobKind::Player:
        return playerHurtOptions();
    case MobKind::Cow:
        return concatClipLists({numberedClipRange("mob/cow/hurt", 1, 3), numberedClipRange("mob/cow/say", 1, 4)});
    case MobKind::Pig:
        return numberedClipRange("mob/pig/say", 1, 3);
    case MobKind::Sheep:
        return numberedClipRange("mob/sheep/say", 1, 3);
    case MobKind::Chicken:
        return concatClipLists({numberedClipRange("mob/chicken/hurt", 1, 2), numberedClipRange("mob/chicken/say", 1, 3)});
    }
    return {};
}

[[nodiscard]] std::vector<std::string> mobAmbientOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::Zombie:
    case MobKind::Skeleton:
    case MobKind::Creeper:
    case MobKind::Spider:
        return numberedClipRange("mob/zombie/say", 1, 3);
    case MobKind::Player:
        return {};
    case MobKind::Cow:
        return numberedClipRange("mob/cow/say", 1, 4);
    case MobKind::Pig:
        return numberedClipRange("mob/pig/say", 1, 3);
    case MobKind::Sheep:
        return numberedClipRange("mob/sheep/say", 1, 3);
    case MobKind::Chicken:
        return numberedClipRange("mob/chicken/say", 1, 3);
    }
    return {};
}

[[nodiscard]] std::vector<std::string> mobStepOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::Zombie:
    case MobKind::Skeleton:
    case MobKind::Creeper:
    case MobKind::Spider:
        return numberedClipRange("mob/zombie/step", 1, 5);
    case MobKind::Player:
        return {};
    case MobKind::Cow:
        return numberedClipRange("mob/cow/step", 1, 4);
    case MobKind::Pig:
        return numberedClipRange("mob/pig/step", 1, 5);
    case MobKind::Sheep:
        return numberedClipRange("mob/sheep/step", 1, 5);
    case MobKind::Chicken:
        return numberedClipRange("mob/chicken/step", 1, 2);
    }
    return {};
}

[[nodiscard]] std::vector<std::string> mobDefeatOptions(const vibecraft::game::MobKind mobKind)
{
    using vibecraft::game::MobKind;
    switch (mobKind)
    {
    case MobKind::Zombie:
    case MobKind::Skeleton:
    case MobKind::Creeper:
    case MobKind::Spider:
        return {"mob/zombie/death.ogg"};
    case MobKind::Player:
        return playerDeathOptions();
    case MobKind::Cow:
        return numberedClipRange("mob/cow/hurt", 1, 3);
    case MobKind::Pig:
        return {"mob/pig/death.ogg"};
    case MobKind::Sheep:
        return numberedClipRange("mob/sheep/say", 1, 3);
    case MobKind::Chicken:
        return numberedClipRange("mob/chicken/hurt", 1, 2);
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
    append(blockBreakOptions(vibecraft::world::BlockType::OakLog));
    append(blockBreakOptions(vibecraft::world::BlockType::Deepslate));

    append(blockPlaceOptions(vibecraft::world::BlockType::Stone));
    append(blockPlaceOptions(vibecraft::world::BlockType::Grass));
    append(blockPlaceOptions(vibecraft::world::BlockType::Sand));
    append(blockPlaceOptions(vibecraft::world::BlockType::OakLog));
    append(blockPlaceOptions(vibecraft::world::BlockType::Deepslate));

    append(footstepOptions(vibecraft::world::BlockType::Stone));
    append(footstepOptions(vibecraft::world::BlockType::Grass));
    append(footstepOptions(vibecraft::world::BlockType::Sand));
    append(footstepOptions(vibecraft::world::BlockType::OakLog));
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
    append(uiClickOptions());
    append(chestOpenOptions());
    append(chestCloseOptions());
    append(itemConsumeOptions());

    using MK = vibecraft::game::MobKind;
    append(mobAmbientOptions(MK::Zombie));
    append(mobAmbientOptions(MK::Skeleton));
    append(mobAmbientOptions(MK::Creeper));
    append(mobAmbientOptions(MK::Spider));
    append(mobAmbientOptions(MK::Cow));
    append(mobAmbientOptions(MK::Pig));
    append(mobAmbientOptions(MK::Sheep));
    append(mobAmbientOptions(MK::Chicken));
    append(mobStepOptions(MK::Zombie));
    append(mobStepOptions(MK::Skeleton));
    append(mobStepOptions(MK::Creeper));
    append(mobStepOptions(MK::Spider));
    append(mobStepOptions(MK::Cow));
    append(mobStepOptions(MK::Pig));
    append(mobStepOptions(MK::Sheep));
    append(mobStepOptions(MK::Chicken));
    append(mobHitOptions(MK::Zombie));
    append(mobHitOptions(MK::Skeleton));
    append(mobHitOptions(MK::Creeper));
    append(mobHitOptions(MK::Spider));
    append(mobHitOptions(MK::Cow));
    append(mobHitOptions(MK::Pig));
    append(mobHitOptions(MK::Sheep));
    append(mobHitOptions(MK::Chicken));
    append(mobDefeatOptions(MK::Zombie));
    append(mobDefeatOptions(MK::Skeleton));
    append(mobDefeatOptions(MK::Creeper));
    append(mobDefeatOptions(MK::Spider));
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

void SoundEffects::playUiClick()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(uiClickOptions(), 0.40f);
}

void SoundEffects::playChestOpen()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(chestOpenOptions(), 0.55f);
}

void SoundEffects::playChestClose()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(chestCloseOptions(), 0.52f);
}

void SoundEffects::playItemConsume()
{
    if (stream_ == nullptr)
    {
        return;
    }
    playRandomClipWithGain(itemConsumeOptions(), 0.44f);
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

void SoundEffects::playMobAmbient(const vibecraft::game::MobKind mobKind)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = mobAmbientOptions(mobKind);
    if (!options.empty())
    {
        playRandomClipWithGain(options, 0.38f);
    }
}

void SoundEffects::playMobStep(const vibecraft::game::MobKind mobKind)
{
    if (stream_ == nullptr)
    {
        return;
    }
    const std::vector<std::string> options = mobStepOptions(mobKind);
    if (!options.empty())
    {
        playRandomClipWithGain(options, mobKind == vibecraft::game::MobKind::Chicken ? 0.26f : 0.30f);
    }
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
