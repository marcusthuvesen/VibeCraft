#include "vibecraft/app/ApplicationChunkStreaming.hpp"

#include <algorithm>
#include <array>

#include <glm/common.hpp>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] glm::vec3 chunkSectionBoundsMin(const world::ChunkCoord& coord, const int sectionIndex)
{
    return {
        static_cast<float>(coord.x * world::Chunk::kSize),
        static_cast<float>(meshing::chunkRenderSectionMinY(sectionIndex)),
        static_cast<float>(coord.z * world::Chunk::kSize),
    };
}

[[nodiscard]] glm::vec3 chunkSectionBoundsMax(const world::ChunkCoord& coord, const int sectionIndex)
{
    return {
        static_cast<float>((coord.x + 1) * world::Chunk::kSize),
        static_cast<float>(meshing::chunkRenderSectionMaxY(sectionIndex) + 1),
        static_cast<float>((coord.z + 1) * world::Chunk::kSize),
    };
}

[[nodiscard]] meshing::ChunkMeshBuildSettings focusedMeshBuildSettings(const int cameraWorldY)
{
    static_cast<void>(cameraWorldY);
    return meshing::ChunkMeshBuildSettings{.prioritizeVerticalWindow = false};
}
}  // namespace

std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
}

std::uint64_t chunkSectionMeshId(const world::ChunkCoord& coord, const int sectionIndex)
{
    constexpr std::uint64_t kCoordMask = (std::uint64_t{1} << 29U) - 1U;
    constexpr std::uint64_t kSectionMask = (std::uint64_t{1} << 6U) - 1U;
    const std::uint64_t packedX = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) & kCoordMask;
    const std::uint64_t packedZ = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.z)) & kCoordMask;
    const std::uint64_t packedSection = static_cast<std::uint64_t>(sectionIndex) & kSectionMask;
    return (packedSection << 58U) | (packedX << 29U) | packedZ;
}

world::ChunkMeshStats chunkMeshStatsFromSections(
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes)
{
    world::ChunkMeshStats stats{};
    for (const meshing::ChunkSectionMeshData& sectionMesh : sectionMeshes)
    {
        stats.faceCount += sectionMesh.mesh.faceCount;
        stats.vertexCount += static_cast<std::uint32_t>(sectionMesh.mesh.vertices.size());
        stats.indexCount += static_cast<std::uint32_t>(sectionMesh.mesh.indices.size());
    }
    return stats;
}

std::vector<render::SceneMeshData> buildSceneMeshesForChunkSections(
    const world::ChunkCoord& coord,
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes)
{
    std::vector<render::SceneMeshData> sceneMeshes;
    sceneMeshes.reserve(sectionMeshes.size());
    for (const meshing::ChunkSectionMeshData& sectionMesh : sectionMeshes)
    {
        if (sectionMesh.mesh.vertices.empty() || sectionMesh.mesh.indices.empty())
        {
            continue;
        }

        render::SceneMeshData sceneMesh;
        sceneMesh.id = chunkSectionMeshId(coord, sectionMesh.sectionIndex);
        sceneMesh.indices = sectionMesh.mesh.indices;
        sceneMesh.vertices.reserve(sectionMesh.mesh.vertices.size());

        glm::vec3 tightMin{0.0f};
        glm::vec3 tightMax{0.0f};
        bool haveBounds = false;
        for (const meshing::DebugVertex& vertex : sectionMesh.mesh.vertices)
        {
            const glm::vec3 p{vertex.x, vertex.y, vertex.z};
            sceneMesh.vertices.push_back(render::SceneMeshData::Vertex{
                .position = p,
                .normal = {vertex.nx, vertex.ny, vertex.nz},
                .uv = {vertex.u, vertex.v},
                .abgr = vertex.abgr,
            });
            if (!haveBounds)
            {
                tightMin = p;
                tightMax = p;
                haveBounds = true;
            }
            else
            {
                tightMin = glm::min(tightMin, p);
                tightMax = glm::max(tightMax, p);
            }
        }

        constexpr float kMeshBoundsPad = 0.02f;
        if (haveBounds)
        {
            sceneMesh.boundsMin = tightMin - kMeshBoundsPad;
            sceneMesh.boundsMax = tightMax + kMeshBoundsPad;
        }
        else
        {
            sceneMesh.boundsMin = chunkSectionBoundsMin(coord, sectionMesh.sectionIndex);
            sceneMesh.boundsMax = chunkSectionBoundsMax(coord, sectionMesh.sectionIndex);
        }
        sceneMeshes.push_back(std::move(sceneMesh));
    }
    return sceneMeshes;
}

std::vector<std::uint64_t> buildRemovedChunkSectionMeshIds(
    const world::ChunkCoord& coord,
    const std::vector<meshing::ChunkSectionMeshData>& sectionMeshes)
{
    std::array<bool, meshing::kChunkRenderSectionCount> builtSections{};
    for (const meshing::ChunkSectionMeshData& sectionMesh : sectionMeshes)
    {
        if (sectionMesh.sectionIndex >= 0 && sectionMesh.sectionIndex < meshing::kChunkRenderSectionCount)
        {
            builtSections[static_cast<std::size_t>(sectionMesh.sectionIndex)] = true;
        }
    }

    std::vector<std::uint64_t> removedMeshIds;
    removedMeshIds.reserve(meshing::kChunkRenderSectionCount);
    for (int sectionIndex = 0; sectionIndex < meshing::kChunkRenderSectionCount; ++sectionIndex)
    {
        if (!builtSections[static_cast<std::size_t>(sectionIndex)])
        {
            removedMeshIds.push_back(chunkSectionMeshId(coord, sectionIndex));
        }
    }
    return removedMeshIds;
}

MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& residentChunkCoords,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    const int cameraWorldY,
    const int residentChunkRadius,
    const std::size_t meshBuildBudget)
{
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(dirtyCoordSet.size());
    cpuData.removedMeshIds.reserve(dirtyCoordSet.size() + residentChunkMeshIds.size());
    cpuData.residentCoordsToAdd.reserve(dirtyCoordSet.size());
    cpuData.removedResidentCoords.reserve(residentChunkCoords.size());
    cpuData.dirtyResidentMeshUpdates.reserve(dirtyCoordSet.size());
    std::vector<std::pair<world::ChunkCoord, bool>> pendingResidentCoords;
    pendingResidentCoords.reserve(static_cast<std::size_t>(
        (residentChunkRadius * 2 + 1) * (residentChunkRadius * 2 + 1)));

    for (int chunkZ = cameraChunk.z - residentChunkRadius;
         chunkZ <= cameraChunk.z + residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - residentChunkRadius;
             chunkX <= cameraChunk.x + residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            cpuData.desiredResidentCoords.insert(coord);
            for (int sectionIndex = 0; sectionIndex < meshing::kChunkRenderSectionCount; ++sectionIndex)
            {
                cpuData.desiredResidentIds.insert(chunkSectionMeshId(coord, sectionIndex));
            }

            const bool isDirty = dirtyCoordSet.contains(coord);
            const auto chunkIt = world.chunks().find(coord);
            if (chunkIt == world.chunks().end())
            {
                continue;
            }

            const bool isResident = residentChunkCoords.contains(coord);
            if (isResident && !isDirty)
            {
                continue;
            }
            pendingResidentCoords.push_back({coord, isDirty});
        }
    }

    std::sort(
        pendingResidentCoords.begin(),
        pendingResidentCoords.end(),
        [&cameraChunk](const auto& lhs, const auto& rhs)
        {
            const int lhsDx = lhs.first.x - cameraChunk.x;
            const int lhsDz = lhs.first.z - cameraChunk.z;
            const int rhsDx = rhs.first.x - cameraChunk.x;
            const int rhsDz = rhs.first.z - cameraChunk.z;
            const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
            const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
            if (lhsDistanceSq != rhsDistanceSq)
            {
                return lhsDistanceSq < rhsDistanceSq;
            }
            if (lhs.first.z != rhs.first.z)
            {
                return lhs.first.z < rhs.first.z;
            }
            return lhs.first.x < rhs.first.x;
        });

    std::size_t builtThisFrame = 0;
    for (const auto& [coord, isDirty] : pendingResidentCoords)
    {
        if (builtThisFrame >= meshBuildBudget)
        {
            break;
        }

        const std::vector<meshing::ChunkSectionMeshData> sectionMeshes =
            chunkMesher.buildSectionMeshes(world, coord, focusedMeshBuildSettings(cameraWorldY));
        const world::ChunkMeshStats chunkMeshStats = chunkMeshStatsFromSections(sectionMeshes);
        const std::vector<render::SceneMeshData> sceneMeshes =
            buildSceneMeshesForChunkSections(coord, sectionMeshes);
        const std::vector<std::uint64_t> removedSectionMeshIds =
            buildRemovedChunkSectionMeshIds(coord, sectionMeshes);
        cpuData.removedMeshIds.insert(
            cpuData.removedMeshIds.end(),
            removedSectionMeshIds.begin(),
            removedSectionMeshIds.end());
        if (sceneMeshes.empty())
        {
            cpuData.removedResidentCoords.push_back(coord);
            if (isDirty)
            {
                cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                    .coord = coord,
                    .stats = world::ChunkMeshStats{},
                });
            }
            ++builtThisFrame;
            continue;
        }

        cpuData.sceneMeshesToUpload.insert(
            cpuData.sceneMeshesToUpload.end(),
            sceneMeshes.begin(),
            sceneMeshes.end());
        cpuData.residentCoordsToAdd.push_back(coord);
        if (isDirty)
        {
            cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                .coord = coord,
                .stats = chunkMeshStats,
            });
        }
        ++builtThisFrame;
    }

    for (const world::ChunkCoord& residentCoord : residentChunkCoords)
    {
        if (!cpuData.desiredResidentCoords.contains(residentCoord))
        {
            cpuData.removedResidentCoords.push_back(residentCoord);
        }
    }

    for (const std::uint64_t residentId : residentChunkMeshIds)
    {
        if (!cpuData.desiredResidentIds.contains(residentId))
        {
            cpuData.removedMeshIds.push_back(residentId);
        }
    }

    return cpuData;
}

void applyMeshSyncGpuData(
    render::Renderer& renderer,
    const MeshSyncCpuData& cpuData,
    std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& residentChunkCoords)
{
    if (!cpuData.sceneMeshesToUpload.empty() || !cpuData.removedMeshIds.empty())
    {
        renderer.updateSceneMeshes(cpuData.sceneMeshesToUpload, cpuData.removedMeshIds);
    }

    for (const std::uint64_t removedId : cpuData.removedMeshIds)
    {
        residentChunkMeshIds.erase(removedId);
    }

    for (const render::SceneMeshData& sceneMesh : cpuData.sceneMeshesToUpload)
    {
        residentChunkMeshIds.insert(sceneMesh.id);
    }

    for (const world::ChunkCoord& removedCoord : cpuData.removedResidentCoords)
    {
        residentChunkCoords.erase(removedCoord);
    }
    for (const world::ChunkCoord& residentCoord : cpuData.residentCoordsToAdd)
    {
        residentChunkCoords.insert(residentCoord);
    }
}
}  // namespace vibecraft::app
