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
[[nodiscard]] float diamondRandomGate(const BiomeOreProfile profile)
{
    switch (profile)
    {
    case BiomeOreProfile::IceShelf:
        return 0.987f;
    case BiomeOreProfile::OxygenGrove:
        return 0.994f;
    case BiomeOreProfile::DustFlats:
    case BiomeOreProfile::RegolithPlains:
    default:
        return 0.992f;
    }
}

[[nodiscard]] float goldRandomGate(const BiomeOreProfile profile)
{
    switch (profile)
    {
    case BiomeOreProfile::IceShelf:
        return 0.990f;
    case BiomeOreProfile::DustFlats:
        return 0.996f;
    case BiomeOreProfile::OxygenGrove:
        return 0.995f;
    case BiomeOreProfile::RegolithPlains:
    default:
        return 0.994f;
    }
}

[[nodiscard]] float ironRandomGate(const BiomeOreProfile profile)
{
    switch (profile)
    {
    case BiomeOreProfile::RegolithPlains:
        return 0.985f;
    case BiomeOreProfile::DustFlats:
        return 0.991f;
    case BiomeOreProfile::IceShelf:
        return 0.987f;
    case BiomeOreProfile::OxygenGrove:
        return 0.993f;
    default:
        return 0.988f;
    }
}

[[nodiscard]] double coalCombinedNoiseThresholdBonus(const BiomeOreProfile profile)
{
    switch (profile)
    {
    case BiomeOreProfile::DustFlats:
        return -0.065;
    case BiomeOreProfile::OxygenGrove:
        return -0.05;
    case BiomeOreProfile::IceShelf:
        return -0.022;
    case BiomeOreProfile::RegolithPlains:
    default:
        return 0.0;
    }
}
// Resource ladder for the pivot: carbon higher up, ferrite mid-depth, auric seams lower,
// ridge crystals near exposed highlands, and deep crystals in the deepest basalt.
constexpr int kCoalMinY = -16;
constexpr int kCoalSurfaceBuffer = 6;
constexpr int kIronMinY = -32;
constexpr int kIronMaxY = 80;
constexpr int kGoldMinY = -48;
constexpr int kGoldMaxY = 32;
constexpr int kDiamondMinY = kUndergroundStartY;
constexpr int kDiamondMaxY = -16;
constexpr int kMountainStoneCapStartY = 96;

[[nodiscard]] bool hostAllowsOre(const BlockType hostBlockType)
{
    return hostBlockType == BlockType::Stone || hostBlockType == BlockType::Deepslate;
}

[[nodiscard]] bool inVerticalBand(const int y, const int minY, const int maxY)
{
    return y >= minY && y <= maxY;
}

[[nodiscard]] bool shouldPlaceEmerald(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType)
{
    if (surfaceHeight < kMountainStoneCapStartY || hostBlockType != BlockType::Stone)
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.19 + static_cast<double>(y) * 0.13) +
        std::cos(static_cast<double>(worldZ) * 0.17 - static_cast<double>(y) * 0.09);
    return veinNoise > 2.35 && noise::random01(worldX, worldZ, 0x3c1a77e2U) > 0.985;
}

[[nodiscard]] bool shouldPlaceDiamond(
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType hostBlockType,
    const BiomeOreProfile profile)
{
    if (!inVerticalBand(y, kDiamondMinY, kDiamondMaxY) || hostBlockType != BlockType::Deepslate)
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.27 + static_cast<double>(y) * 0.21) +
        std::cos(static_cast<double>(worldZ) * 0.23 - static_cast<double>(y) * 0.15) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.11 + static_cast<double>(y) * 0.07);
    return veinNoise > 2.45
        && noise::random01(worldX + y, worldZ - y, 0x91f0a3d1U) > diamondRandomGate(profile);
}

[[nodiscard]] bool shouldPlaceGold(
    const int worldX,
    const int y,
    const int worldZ,
    const BlockType hostBlockType,
    const BiomeOreProfile profile)
{
    if (!inVerticalBand(y, kGoldMinY, kGoldMaxY) || !hostAllowsOre(hostBlockType))
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.31) +
        std::cos(static_cast<double>(worldZ) * 0.29) +
        std::sin(static_cast<double>(worldX + worldZ + y) * 0.07);
    return veinNoise > 2.5
        && noise::random01(worldX, y + worldZ * 3, 0xb00dfaceU) > goldRandomGate(profile);
}

[[nodiscard]] bool shouldPlaceIron(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType,
    const BiomeOreProfile profile)
{
    if (!inVerticalBand(y, kIronMinY, kIronMaxY) || !hostAllowsOre(hostBlockType))
    {
        return false;
    }

    if (y > surfaceHeight - kCoalSurfaceBuffer)
    {
        return false;
    }

    const double veinRegionNoise = std::sin(static_cast<double>(worldX) * 0.051) +
        std::cos(static_cast<double>(worldZ) * 0.047) +
        std::sin(static_cast<double>(worldX + worldZ) * 0.031);

    if (veinRegionNoise <= 2.0)
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.21 + static_cast<double>(y) * 0.09) +
        std::cos(static_cast<double>(worldZ) * 0.18 - static_cast<double>(y) * 0.07);
    return veinNoise > 2.25
        && noise::random01(worldX - y, worldZ + y, 0xfe11c0a1U) > ironRandomGate(profile);
}

[[nodiscard]] bool shouldPlaceCoal(
    const int worldX,
    const int y,
    const int worldZ,
    const int surfaceHeight,
    const BlockType hostBlockType,
    const BiomeOreProfile profile)
{
    if (y < kCoalMinY || y > surfaceHeight - kCoalSurfaceBuffer || !hostAllowsOre(hostBlockType))
    {
        return false;
    }

    const double veinRegionNoise = std::sin(static_cast<double>(worldX) * 0.041) +
        std::cos(static_cast<double>(worldZ) * 0.037) +
        std::sin(static_cast<double>(worldX + worldZ) * 0.024) +
        std::cos(static_cast<double>(worldX - worldZ) * 0.029);

    if (veinRegionNoise <= 2.15)
    {
        return false;
    }

    const double veinNoise = std::sin(static_cast<double>(worldX) * 0.23 + static_cast<double>(y) * 0.11) +
        std::cos(static_cast<double>(worldZ) * 0.19 - static_cast<double>(y) * 0.08) +
        std::sin(static_cast<double>(worldX - worldZ) * 0.17 + static_cast<double>(y) * 0.05);
    const double pocketNoise = std::sin(static_cast<double>(worldX) * 0.53) +
        std::cos(static_cast<double>(y) * 0.47) +
        std::sin(static_cast<double>(worldZ) * 0.43);
    const double combinedOreNoise = veinNoise + pocketNoise * 0.35;
    constexpr int kSeaLevel = 63;
    const double altitudeBias =
        std::clamp((static_cast<double>(y) - static_cast<double>(kSeaLevel)) / 24.0, -0.35, 0.35);
    const double hostBias = hostBlockType == BlockType::Deepslate ? 0.18 : 0.0;
    const double threshold =
        2.3 - altitudeBias + hostBias + coalCombinedNoiseThresholdBonus(profile);
    return combinedOreNoise > threshold;
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

    if (shouldPlaceEmerald(worldX, y, worldZ, surfaceHeight, hostBlockType))
    {
        return BlockType::EmeraldOre;
    }
    if (shouldPlaceDiamond(worldX, y, worldZ, hostBlockType, biomeProfile))
    {
        return BlockType::DiamondOre;
    }
    if (shouldPlaceGold(worldX, y, worldZ, hostBlockType, biomeProfile))
    {
        return BlockType::GoldOre;
    }
    if (shouldPlaceIron(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile))
    {
        return BlockType::IronOre;
    }
    if (shouldPlaceCoal(worldX, y, worldZ, surfaceHeight, hostBlockType, biomeProfile))
    {
        return BlockType::CoalOre;
    }
    return std::nullopt;
}
}  // namespace vibecraft::world::underground
