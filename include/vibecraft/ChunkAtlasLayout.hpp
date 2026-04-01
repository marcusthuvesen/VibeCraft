#pragma once

#include <cstdint>

namespace vibecraft
{
// Must match scripts/build_chunk_atlas.sh (8x10 grid of 16x16 tiles).
inline constexpr std::uint16_t kChunkAtlasWidthPx = 128;
inline constexpr std::uint16_t kChunkAtlasHeightPx = 160;
inline constexpr std::uint16_t kChunkAtlasTileColumns = 8;
inline constexpr std::uint16_t kChunkAtlasTileRows = 10;
}  // namespace vibecraft
