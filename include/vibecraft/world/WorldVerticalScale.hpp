#pragma once

#include "vibecraft/world/Chunk.hpp"

namespace vibecraft::world
{
// Minecraft Java 1.18+ (Caves & Cliffs Part 2): Overworld spans Y=-64..320; the bottom
// bedrock shell occupies Y=-64..-60 inclusive — five layers of noisy bedrock before stone.
// Reference: https://minecraft.wiki/w/Bedrock (Overworld floor pattern).

/// Number of bedrock layers at the world bottom in modern Minecraft (Y=-64..-60).
inline constexpr int kMinecraftOverworldBedrockLayerCount = 5;

/// VibeCraft uses a 64-block-tall column (Y in [0, Chunk::kHeight)). The bottom five
/// world layers Y=0..4 correspond to that same bedrock band count — solid, unbreakable
/// floor with no void below (see World::blockAt for Y<0).
inline constexpr int kBedrockFloorMinY = 0;
inline constexpr int kBedrockFloorMaxY = kMinecraftOverworldBedrockLayerCount - 1;

static_assert(kBedrockFloorMaxY < Chunk::kHeight, "Bedrock band must fit in chunk height");

/// First Y above the bedrock shell where normal underground generation (caves, ores) runs.
inline constexpr int kUndergroundStartY = kBedrockFloorMaxY + 1;
}  // namespace vibecraft::world
