#pragma once

namespace vibecraft::world
{
// Minecraft Java 1.18+ (Caves & Cliffs Part 2): Overworld spans Y=-64..320; the bottom
// bedrock shell occupies Y=-64..-60 inclusive — five layers of noisy bedrock before stone.
// Reference: https://minecraft.wiki/w/Bedrock (Overworld floor pattern).

/// Inclusive bottom world Y used by terrain generation and block edits.
inline constexpr int kWorldMinY = -64;
/// Exclusive top in modern Minecraft is 320, so the highest valid block Y is 319.
inline constexpr int kWorldMaxY = 319;
/// Total vertical block count in a chunk column.
inline constexpr int kWorldHeight = kWorldMaxY - kWorldMinY + 1;

/// Number of bedrock layers at the world bottom in modern Minecraft (Y=-64..-60).
inline constexpr int kMinecraftOverworldBedrockLayerCount = 5;

/// Bottom bedrock shell aligned to Minecraft's Y=-64..-60 band.
inline constexpr int kBedrockFloorMinY = kWorldMinY;
inline constexpr int kBedrockFloorMaxY = kWorldMinY + kMinecraftOverworldBedrockLayerCount - 1;

/// First Y above the bedrock shell where normal underground generation (caves, ores) runs.
inline constexpr int kUndergroundStartY = kBedrockFloorMaxY + 1;
}  // namespace vibecraft::world
