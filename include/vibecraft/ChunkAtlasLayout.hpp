#pragma once

#include <cstdint>

namespace vibecraft
{
// Must match scripts/build_chunk_atlas.sh (4x4 grid of 16x16 tiles on a square atlas).
inline constexpr std::uint16_t kChunkAtlasWidthPx = 64;
inline constexpr std::uint16_t kChunkAtlasHeightPx = 64;
inline constexpr std::uint16_t kChunkAtlasTileColumns = 4;
inline constexpr std::uint16_t kChunkAtlasTileRows = 4;
}  // namespace vibecraft
