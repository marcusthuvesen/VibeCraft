#include "vibecraft/world/World.hpp"

#include "vibecraft/world/Block.hpp"
#include "vibecraft/world/WorldVerticalScale.hpp"

namespace vibecraft::world
{
std::size_t World::GravityCellHash::operator()(const GravityCell& cell) const noexcept
{
    std::size_t seed = static_cast<std::size_t>(static_cast<std::uint32_t>(cell.x));
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.y + 0x9e3779b9U)) + (seed << 6U) + (seed >> 2U);
    seed ^= static_cast<std::size_t>(static_cast<std::uint32_t>(cell.z + 0x85ebca6bU)) + (seed << 6U) + (seed >> 2U);
    return seed;
}

void World::scheduleGravityBlock(const int worldX, const int y, const int worldZ)
{
    if (y < kWorldMinY || y > kWorldMaxY)
    {
        return;
    }
    if (isGravityBlock(blockAt(worldX, y, worldZ)))
    {
        activeGravityBlocks_.insert(GravityCell{worldX, y, worldZ});
    }
}

void World::processGravityCell(const GravityCell& cell)
{
    if (cell.y <= kWorldMinY || cell.y > kWorldMaxY)
    {
        return;
    }

    const BlockType blockType = blockAt(cell.x, cell.y, cell.z);
    if (!isGravityBlock(blockType))
    {
        return;
    }

    const BlockType below = blockAt(cell.x, cell.y - 1, cell.z);
    if (below != BlockType::Air && !isFluid(below))
    {
        return; // supported — nothing to do
    }

    // Move the gravity block one step downward.
    setBlockUnchecked(cell.x, cell.y, cell.z, BlockType::Air);
    setBlockUnchecked(cell.x, cell.y - 1, cell.z, blockType);

    // Evict any fluid state at both positions so water/lava tracking stays consistent.
    fluidSources_.erase(FluidCell{cell.x, cell.y, cell.z});
    flowingFluids_.erase(FluidCell{cell.x, cell.y, cell.z});
    fluidSources_.erase(FluidCell{cell.x, cell.y - 1, cell.z});
    flowingFluids_.erase(FluidCell{cell.x, cell.y - 1, cell.z});
    scheduleFluidNeighborhood(cell.x, cell.y, cell.z);
    scheduleFluidNeighborhood(cell.x, cell.y - 1, cell.z);

    // Continue falling from the new position.
    activeGravityBlocks_.insert(GravityCell{cell.x, cell.y - 1, cell.z});

    // The block that was sitting above (now sitting on air) might also need to fall.
    scheduleGravityBlock(cell.x, cell.y + 1, cell.z);
}

void World::tickGravityBlocks(const std::size_t maxUpdates)
{
    if (maxUpdates == 0 || activeGravityBlocks_.empty())
    {
        return;
    }

    std::vector<GravityCell> pending;
    pending.reserve(std::min(maxUpdates, activeGravityBlocks_.size()));
    auto it = activeGravityBlocks_.begin();
    while (it != activeGravityBlocks_.end() && pending.size() < maxUpdates)
    {
        pending.push_back(*it);
        it = activeGravityBlocks_.erase(it);
    }

    for (const GravityCell& cell : pending)
    {
        processGravityCell(cell);
    }
}
}  // namespace vibecraft::world
