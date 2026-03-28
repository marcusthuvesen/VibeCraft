#pragma once

#include <filesystem>

namespace vibecraft::world
{
class World;

class WorldSerializer
{
  public:
    static bool save(const World& world, const std::filesystem::path& outputPath);
    static bool load(World& world, const std::filesystem::path& inputPath);
};
}  // namespace vibecraft::world
