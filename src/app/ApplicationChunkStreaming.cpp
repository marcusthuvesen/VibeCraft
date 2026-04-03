#include "vibecraft/app/ApplicationChunkStreaming.hpp"

#include <algorithm>

#include <glm/common.hpp>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] glm::vec3 chunkBoundsMin(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>(coord.x * world::Chunk::kSize),
        static_cast<float>(world::kWorldMinY),
        static_cast<float>(coord.z * world::Chunk::kSize),
    };
}

[[nodiscard]] glm::vec3 chunkBoundsMax(const world::ChunkCoord& coord)
{
    return {
        static_cast<float>((coord.x + 1) * world::Chunk::kSize),
        static_cast<float>(world::kWorldMaxY + 1),
        static_cast<float>((coord.z + 1) * world::Chunk::kSize),
    };
}
}  // namespace

std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
}

MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    const int residentChunkRadius,
    const std::size_t meshBuildBudget)
{
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(dirtyCoordSet.size());
    cpuData.removedMeshIds.reserve(dirtyCoordSet.size() + residentChunkMeshIds.size());
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
            const std::uint64_t meshId = chunkMeshId(coord);
            cpuData.desiredResidentIds.insert(meshId);

            const bool isDirty = dirtyCoordSet.contains(coord);
            const auto chunkIt = world.chunks().find(coord);
            if (chunkIt == world.chunks().end())
            {
                continue;
            }

            const bool isResident = residentChunkMeshIds.contains(meshId);
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

        const std::uint64_t meshId = chunkMeshId(coord);
        meshing::ChunkMeshData meshData = chunkMesher.buildMesh(world, coord);
        if (meshData.vertices.empty() || meshData.indices.empty())
        {
            cpuData.removedMeshIds.push_back(meshId);
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

        render::SceneMeshData sceneMesh;
        sceneMesh.id = meshId;
        sceneMesh.indices = std::move(meshData.indices);
        sceneMesh.vertices.reserve(meshData.vertices.size());

        glm::vec3 tightMin{0.0f};
        glm::vec3 tightMax{0.0f};
        bool haveBounds = false;
        for (const meshing::DebugVertex& vertex : meshData.vertices)
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
            sceneMesh.boundsMin = chunkBoundsMin(coord);
            sceneMesh.boundsMax = chunkBoundsMax(coord);
        }

        const world::ChunkMeshStats chunkMeshStats{
            .faceCount = meshData.faceCount,
            .vertexCount = static_cast<std::uint32_t>(meshData.vertices.size()),
            .indexCount = static_cast<std::uint32_t>(meshData.indices.size()),
        };
        cpuData.sceneMeshesToUpload.push_back(std::move(sceneMesh));
        if (isDirty)
        {
            cpuData.dirtyResidentMeshUpdates.push_back(world::ChunkMeshUpdate{
                .coord = coord,
                .stats = chunkMeshStats,
            });
        }
        ++builtThisFrame;
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
    std::unordered_set<std::uint64_t>& residentChunkMeshIds)
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
}
}  // namespace vibecraft::app
