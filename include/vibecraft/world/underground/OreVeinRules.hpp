#pragma once

#include <optional>

#include "vibecraft/world/Block.hpp"

namespace vibecraft::world::underground
{
/// Surface biome band used to tune underground ore bands (aligned with `SurfaceBiome` / column biomes).
enum class BiomeOreProfile : std::uint8_t
{
    RegolithPlains = 0,
    DustFlats = 1,
    IceShelf = 2,
    OxygenGrove = 3,
};

/// Host block is plain stone or deepslate (no ore overlay on regolith or surface strata).
/// Returns the ore to place, or std::nullopt to keep the host block.
/// Priority follows rarity: ridge crystals > deep crystals > auric seams > ferrite > carbon.
[[nodiscard]] std::optional<BlockType> selectOreVeinBlock(
    int worldX,
    int y,
    int worldZ,
    int surfaceHeight,
    BlockType hostBlockType,
    BiomeOreProfile biomeProfile = BiomeOreProfile::RegolithPlains);
}  // namespace vibecraft::world::underground
