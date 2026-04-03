#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "vibecraft/meshing/ChunkMesher.hpp"
#include "vibecraft/render/Renderer.hpp"
#include "vibecraft/world/World.hpp"

namespace vibecraft::app
{
struct MeshSyncCpuData
{
    std::unordered_set<std::uint64_t> desiredResidentIds;
    std::vector<render::SceneMeshData> sceneMeshesToUpload;
    std::vector<std::uint64_t> removedMeshIds;
    std::vector<world::ChunkMeshUpdate> dirtyResidentMeshUpdates;
};

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord);

[[nodiscard]] MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    int residentChunkRadius,
    std::size_t meshBuildBudget);

void applyMeshSyncGpuData(
    render::Renderer& renderer,
    const MeshSyncCpuData& cpuData,
    std::unordered_set<std::uint64_t>& residentChunkMeshIds);
}  // namespace vibecraft::app
