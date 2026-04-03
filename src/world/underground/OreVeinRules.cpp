#include "vibecraft/world/underground/OreVeinRules.hpp"

#include <algorithm>
#include <cmath>

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/TerrainNoise.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world::underground
{
namespace
{
// Minecraft-inspired ore layering:
// - Coal mostly higher in the overworld, still possible near Y=0.
// - Iron centered around the mid-underground.
// - Gold lower, especially below sea level.
// - Diamond concentrated near the deep deepslate floor.
// - Emerald tied to mountain stone and higher terrain bands.
constexpr int kMountainStoneCapStartY = 96;
constexpr int kCoalPeakY = 96;
constexpr int kIronPeakY = 24;
constexpr int kGoldPeakY = -18;
constexpr int kDiamondPeakY = -59;
constexpr int kEmeraldPeakY = 96;

enum class OreKind : std::uint8_t
{
    Coal = 0,
    Iron,
    Gold,
    Diamond,
    Emerald,
};

struct OreHeightProfile
{
    int minY = kUndergroundStartY;
    int peakY = 0;
    int maxY = 0;
};

struct OrePlacementProfile
{
    OreKind kind = OreKind::Coal;
    BlockType oreBlock = BlockType::CoalOre;
    OreHeightProfile heightProfile{};
    double regionScale = 96.0;
    double clusterScale = 24.0;
    int regionOctaves = 3;
    int clusterOctaves = 3;
    std::uint32_t regionSeed = 0;
    std::uint32_t clusterSeed = 0;
    std::uint32_t randomSeed = 0;
    double regionThreshold = 0.60;
    double clusterThreshold = 0.64;
    double peakChance = 0.20;
    bool requireMountainSurface = false;
    bool requireStoneHost = false;
    bool requireDeepslateHost = false;
};

[[nodiscard]] bool hostAllowsOre(const BlockType hostBlockType)
{
    return hostBlockType == BlockType::Stone || hostBlockType == BlockType::Deepslate;
}

[[nodiscard]] bool inVerticalBand(const int y, const int minY, const int maxY)
{
    return y >= minY && y <= maxY;
}

[[nodiscard]] double normalizedTriangularHeightWeight(const int y, const OreHeightProfile& profile)
{
    if (!inVerticalBand(y, profile.minY, profile.maxY))
    {
        return 0.0;
    }

    if (profile.minY == profile.maxY)
    {
        return 1.0;
    }

    if (y == profile.peakY)
    {
        return 1.0;
    }

    if (y < profile.peakY)
    {
        const int denom = std::max(1, profile.peakY - profile.minY);
        return std::clamp(
            static_cast<double>(y - profile.minY) / static_cast<double>(denom),
            0.0,
            1.0);
    }

    const int denom = std::max(1, profile.maxY - profile.peakY);
    return std::clamp(
        static_cast<double>(profile.maxY - y) / static_cast<double>(denom),
        0.0,
        1.0);
}

[[nodiscard]] double oreBiomeMultiplier(const OreKind oreKind, const BiomeOreProfile profile)
{
    switch (oreKind)
    {
    case OreKind::Coal:
        switch (profile)
        {
        case BiomeOreProfile::DustFlats:
            return 1.14;
        case BiomeOreProfile::VerdantGrove:
            return 1.10;
        case BiomeOreProfile::IceShelf:
            return 0.96;
        case BiomeOreProfile::RegolithPlains:
        default:
            return 1.0;
        }
    case OreKind::Iron:
        switch (profile)
        {
        case BiomeOreProfile::RegolithPlains:
            return 1.08;
        case BiomeOreProfile::IceShelf:
            return 1.04;
        case BiomeOreProfile::DustFlats:
            return 0.94;
        case BiomeOreProfile::VerdantGrove:
        default:
            return 1.0;
        }
    case OreKind::Gold:
        switch (profile)
        {
        case BiomeOreProfile::DustFlats:
            return 1.12;
        case BiomeOreProfile::IceShelf:
            return 0.94;
        case BiomeOreProfile::VerdantGrove:
            return 0.96;
        case BiomeOreProfile::RegolithPlains:
        default:
            return 1.0;
        }
    case OreKind::Diamond:
        switch (profile)
        {
        case BiomeOreProfile::IceShelf:
            return 0.98;
        case BiomeOreProfile::VerdantGrove:
            return 0.94;
        case BiomeOreProfile::DustFlats:
        case BiomeOreProfile::RegolithPlains:
        default:
            return 1.0;
        }
    case OreKind::Emerald:
        return 1.0;
    }

    return 1.0;
}

[[nodiscard]] double oreHostMultiplier(
    const OrePlacementProfile& profile,
    const BlockType hostBlockType)
{
    if (profile.requireStoneHost)
    {
        return hostBlockType == BlockType::Stone ? 1.0 : 0.0;
    }
    if (profile.requireDeepslateHost)
    {
        return hostBlockType == BlockType::Deepslate ? 1.0 : 0.0;
    }

    if (hostBlockType == BlockType::Deepslate)
    {
        switch (profile.kind)
        {
        case OreKind::Diamond:
        case OreKind::Gold:
            return 1.08;
        case OreKind::Coal:
            return 0.88;
        case OreKind::Iron:
        case OreKind::Emerald:
        default:
            return 1.0;
        }
    }

    if (hostBlockType == BlockType::Stone)
    {
        return profile.kind == OreKind::Coal ? 1.08 : 1.0;
    }

    return 0.0;
}

[[nodiscard]] double oreRegionNoise(
    const int worldX,
    const int y,
    const int worldZ,
    const OrePlacementProfile& profile)
{
    return noise::fbmNoise2d(
        static_cast<double>(worldX) + static_cast<double>(y) * 0.47,
        static_cast<double>(worldZ) - static_cast<double>(y) * 0.31,
        profile.regionScale,
        profile.regionOctaves,
        profile.regionSeed);
}

[[nodiscard]] double oreClusterNoise(
    const int worldX,
    const int y,
    const int worldZ,
    const OrePlacementProfile& profile)
{
    return noise::fbmNoise2d(
        static_cast<double>(worldX) * 0.85 + static_cast<double>(y) * 0.73,
        static_cast<double>(worldZ) * 0.85 - static_cast<double>(y) * 0.52,
        profile.clusterScale,
        profile.clusterOctaves,
        profile.clusterSeed);
}

[[nodiscard]] bool orePassesPlacement(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType,
    const BiomeOreProfile biomeProfile,
    const OrePlacementProfile& profile)
{
    if (!hostAllowsOre(hostBlockType))
    {
        return false;
    }
    if (profile.requireMountainSurface && surfaceHeight < kMountainStoneCapStartY)
    {
        return false;
    }

    const double heightWeight = normalizedTriangularHeightWeight(y, profile.heightProfile);
    const double hostWeight = oreHostMultiplier(profile, hostBlockType);
    if (heightWeight <= 0.0 || hostWeight <= 0.0)
    {
        return false;
    }

    const double regionNoise = oreRegionNoise(worldX, y, worldZ, profile);
    if (regionNoise < profile.regionThreshold)
    {
        return false;
    }

    const double clusterNoise = oreClusterNoise(worldX, y, worldZ, profile);
    if (clusterNoise < profile.clusterThreshold)
    {
        return false;
    }

    const double oreChance =
        profile.peakChance * heightWeight * hostWeight * oreBiomeMultiplier(profile.kind, biomeProfile);
    const double oreRoll = noise::random01(
        worldX + y * 31,
        worldZ - y * 17,
        profile.randomSeed);
    return oreRoll < std::clamp(oreChance, 0.0, 0.95);
}

[[nodiscard]] OrePlacementProfile coalProfile()
{
    return OrePlacementProfile{
        .kind = OreKind::Coal,
        .oreBlock = BlockType::CoalOre,
        .heightProfile = {.minY = 0, .peakY = kCoalPeakY, .maxY = 192},
        .regionScale = 94.0,
        .clusterScale = 22.0,
        .regionSeed = 0x0ca11ab1U,
        .clusterSeed = 0x0ca11ab2U,
        .randomSeed = 0x0ca11ab3U,
        .regionThreshold = 0.47,
        .clusterThreshold = 0.53,
        .peakChance = 0.58,
    };
}

[[nodiscard]] OrePlacementProfile ironProfile()
{
    return OrePlacementProfile{
        .kind = OreKind::Iron,
        .oreBlock = BlockType::IronOre,
        .heightProfile = {.minY = kUndergroundStartY, .peakY = kIronPeakY, .maxY = 144},
        .regionScale = 104.0,
        .clusterScale = 24.0,
        .regionSeed = 0x1f0a11a1U,
        .clusterSeed = 0x1f0a11a2U,
        .randomSeed = 0x1f0a11a3U,
        .regionThreshold = 0.50,
        .clusterThreshold = 0.56,
        .peakChance = 0.42,
    };
}

[[nodiscard]] OrePlacementProfile goldProfile()
{
    return OrePlacementProfile{
        .kind = OreKind::Gold,
        .oreBlock = BlockType::GoldOre,
        .heightProfile = {.minY = kUndergroundStartY, .peakY = kGoldPeakY, .maxY = 32},
        .regionScale = 84.0,
        .clusterScale = 18.0,
        .regionSeed = 0x60adab11U,
        .clusterSeed = 0x60adab12U,
        .randomSeed = 0x60adab13U,
        .regionThreshold = 0.54,
        .clusterThreshold = 0.60,
        .peakChance = 0.30,
    };
}

[[nodiscard]] OrePlacementProfile diamondProfile()
{
    return OrePlacementProfile{
        .kind = OreKind::Diamond,
        .oreBlock = BlockType::DiamondOre,
        .heightProfile = {.minY = kUndergroundStartY, .peakY = kDiamondPeakY, .maxY = 16},
        .regionScale = 74.0,
        .clusterScale = 16.0,
        .regionSeed = 0xd1a40ad1U,
        .clusterSeed = 0xd1a40ad2U,
        .randomSeed = 0xd1a40ad3U,
        .regionThreshold = 0.58,
        .clusterThreshold = 0.63,
        .peakChance = 0.22,
    };
}

[[nodiscard]] OrePlacementProfile emeraldProfile()
{
    return OrePlacementProfile{
        .kind = OreKind::Emerald,
        .oreBlock = BlockType::EmeraldOre,
        .heightProfile = {.minY = -16, .peakY = kEmeraldPeakY, .maxY = 144},
        .regionScale = 58.0,
        .clusterScale = 12.0,
        .regionOctaves = 2,
        .clusterOctaves = 2,
        .regionSeed = 0xe0e6a1d1U,
        .clusterSeed = 0xe0e6a1d2U,
        .randomSeed = 0xe0e6a1d3U,
        .regionThreshold = 0.68,
        .clusterThreshold = 0.74,
        .peakChance = 0.11,
        .requireMountainSurface = true,
        .requireStoneHost = true,
    };
}
}  // namespace

std::optional<BlockType> selectOreVeinBlock(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType,
    const BiomeOreProfile biomeProfile)
{
    if (!hostAllowsOre(hostBlockType))
    {
        return std::nullopt;
    }

    const OrePlacementProfile emerald = emeraldProfile();
    if (orePassesPlacement(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile, emerald))
    {
        return emerald.oreBlock;
    }

    const OrePlacementProfile diamond = diamondProfile();
    if (orePassesPlacement(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile, diamond))
    {
        return diamond.oreBlock;
    }

    const OrePlacementProfile gold = goldProfile();
    if (orePassesPlacement(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile, gold))
    {
        return gold.oreBlock;
    }

    const OrePlacementProfile iron = ironProfile();
    if (orePassesPlacement(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile, iron))
    {
        return iron.oreBlock;
    }

    const OrePlacementProfile coal = coalProfile();
    if (orePassesPlacement(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile, coal))
    {
        return coal.oreBlock;
    }
    return std::nullopt;
}
}  // namespace vibecraft::world::underground
