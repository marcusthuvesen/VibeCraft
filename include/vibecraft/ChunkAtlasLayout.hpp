#pragma once

#include <cstdint>

namespace vibecraft
{
// Must match scripts/build_chunk_atlas.sh (5x4 grid of 16x16 tiles).
inline constexpr std::uint16_t kChunkAtlasWidthPx = 80;
inline constexpr std::uint16_t kChunkAtlasHeightPx = 64;
inline constexpr std::uint16_t kChunkAtlasTileColumns = 5;
inline constexpr std::uint16_t kChunkAtlasTileRows = 4;
}  // namespace vibecraft
