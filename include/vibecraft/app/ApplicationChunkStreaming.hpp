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
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> desiredResidentCoords;
    std::vector<render::SceneMeshData> sceneMeshesToUpload;
    std::vector<std::uint64_t> removedMeshIds;
    std::vector<world::ChunkCoord> residentCoordsToAdd;
    std::vector<world::ChunkCoord> removedResidentCoords;
    std::vector<world::ChunkMeshUpdate> dirtyResidentMeshUpdates;
};

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord);
[[nodiscard]] std::uint64_t chunkSectionMeshId(const world::ChunkCoord& coord, int sectionIndex);
[[nodiscard]] world::ChunkMeshStats chunkMeshStatsFromSections(
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes);
[[nodiscard]] std::vector<render::SceneMeshData> buildSceneMeshesForChunkSections(
    const world::ChunkCoord& coord,
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes);
[[nodiscard]] std::vector<std::uint64_t> buildRemovedChunkSectionMeshIds(
    const world::ChunkCoord& coord,
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes);

[[nodiscard]] MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& residentChunkCoords,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    int cameraWorldY,
    int residentChunkRadius,
    std::size_t meshBuildBudget);

void applyMeshSyncGpuData(
    render::Renderer& renderer,
    const MeshSyncCpuData& cpuData,
    std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& residentChunkCoords);
}  // namespace vibecraft::app
