#include "vibecraft/world/WorldSerializer.hpp"

#include "vibecraft/world/World.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

#include <fstream>

namespace vibecraft::world
{
namespace
{
constexpr std::uint32_t kWorldMagic = 0x56494245;
constexpr std::uint32_t kWorldVersion = 1;

void repairUnbreakableBedrockFloor(Chunk& chunk)
{
    auto& storage = chunk.mutableBlockStorage();
    for (int y = kBedrockFloorMinY; y <= kBedrockFloorMaxY; ++y)
    {
        for (int localZ = 0; localZ < Chunk::kSize; ++localZ)
        {
            for (int localX = 0; localX < Chunk::kSize; ++localX)
            {
                storage[static_cast<std::size_t>(y * Chunk::kSize * Chunk::kSize + localZ * Chunk::kSize + localX)] =
                    BlockType::Bedrock;
            }
        }
    }
}
}

bool WorldSerializer::save(const World& world, const std::filesystem::path& outputPath)
{
    std::filesystem::create_directories(outputPath.parent_path());

    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }

    const std::uint32_t chunkCount = static_cast<std::uint32_t>(world.chunks().size());
    output.write(reinterpret_cast<const char*>(&kWorldMagic), sizeof(kWorldMagic));
    output.write(reinterpret_cast<const char*>(&kWorldVersion), sizeof(kWorldVersion));
    output.write(reinterpret_cast<const char*>(&chunkCount), sizeof(chunkCount));

    for (const auto& [coord, chunk] : world.chunks())
    {
        output.write(reinterpret_cast<const char*>(&coord.x), sizeof(coord.x));
        output.write(reinterpret_cast<const char*>(&coord.z), sizeof(coord.z));

        const auto& blocks = chunk.blockStorage();
        output.write(
            reinterpret_cast<const char*>(blocks.data()),
            static_cast<std::streamsize>(blocks.size() * sizeof(BlockType)));
    }

    return output.good();
}

bool WorldSerializer::load(World& world, const std::filesystem::path& inputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input.is_open())
    {
        return false;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::uint32_t chunkCount = 0;

    input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    input.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));

    if (magic != kWorldMagic || version != kWorldVersion)
    {
        return false;
    }

    World::ChunkMap chunks;
    for (std::uint32_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
    {
        ChunkCoord coord{};
        input.read(reinterpret_cast<char*>(&coord.x), sizeof(coord.x));
        input.read(reinterpret_cast<char*>(&coord.z), sizeof(coord.z));

        Chunk chunk(coord);
        auto& storage = chunk.mutableBlockStorage();
        input.read(
            reinterpret_cast<char*>(storage.data()),
            static_cast<std::streamsize>(storage.size() * sizeof(BlockType)));

        if (!input.good())
        {
            return false;
        }

        // Older saves may predate the bedrock floor; restore the unbreakable base on load
        // so mined shafts never open into a visible void at the bottom of the world.
        repairUnbreakableBedrockFloor(chunk);
        chunks.emplace(coord, chunk);
    }

    world.replaceChunks(std::move(chunks));
    return true;
}
}  // namespace vibecraft::world
