#pragma once

#include <cstdint>

namespace vibecraft
{
// Must match scripts/build_chunk_atlas.sh (8x13 grid of 16x16 tiles).
inline constexpr std::uint16_t kChunkAtlasWidthPx = 128;
inline constexpr std::uint16_t kChunkAtlasHeightPx = 208;
inline constexpr std::uint16_t kChunkAtlasTileColumns = 8;
inline constexpr std::uint16_t kChunkAtlasTileRows = 13;
}  // namespace vibecraft
