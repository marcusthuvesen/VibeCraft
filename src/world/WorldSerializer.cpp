#include "vibecraft/world/WorldSerializer.hpp"

#include "vibecraft/world/World.hpp"

#include <fstream>

namespace vibecraft::world
{
namespace
{
constexpr std::uint32_t kWorldMagic = 0x56494245;
constexpr std::uint32_t kWorldVersion = 2;
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
    const std::uint32_t generationSeed = world.generationSeed();
    output.write(reinterpret_cast<const char*>(&generationSeed), sizeof(generationSeed));
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
    std::uint32_t generationSeed = 0;
    std::uint32_t chunkCount = 0;

    input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    input.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != kWorldMagic || (version != 1 && version != kWorldVersion))
    {
        return false;
    }

    if (version >= 2)
    {
        input.read(reinterpret_cast<char*>(&generationSeed), sizeof(generationSeed));
    }
    input.read(reinterpret_cast<char*>(&chunkCount), sizeof(chunkCount));
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

        chunks.emplace(coord, chunk);
    }

    world.setGenerationSeed(generationSeed);
    world.replaceChunks(std::move(chunks));
    return true;
}
}  // namespace vibecraft::world
