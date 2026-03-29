#pragma once

#include <cstdint>

namespace vibecraft
{
// Must match scripts/build_chunk_atlas.sh (6x6 grid of 16x16 tiles).
inline constexpr std::uint16_t kChunkAtlasWidthPx = 96;
inline constexpr std::uint16_t kChunkAtlasHeightPx = 96;
inline constexpr std::uint16_t kChunkAtlasTileColumns = 6;
inline constexpr std::uint16_t kChunkAtlasTileRows = 6;
}  // namespace vibecraft
