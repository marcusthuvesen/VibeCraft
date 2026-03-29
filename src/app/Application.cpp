#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <unordered_set>
#include <vector>

#include "vibecraft/audio/RuntimeAudioRoot.hpp"
#include "vibecraft/core/Logger.hpp"
#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/multiplayer/UdpTransport.hpp"
#include "vibecraft/platform/LocalNetworkAddress.hpp"
#include "vibecraft/render/RendererDetail.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/WorldEditCommand.hpp"

#if !defined(NDEBUG) || defined(VIBECRAFT_DEBUG_MULTIPLAYER_JOIN)
template<typename... Args>
void logMultiplayerJoinDiag(fmt::format_string<Args...> fmtStr, Args&&... args)
{
    vibecraft::core::logInfo(fmt::format("[mp-join] {}", fmt::format(fmtStr, std::forward<Args>(args)...)));
}
#else
template<typename... Args>
void logMultiplayerJoinDiag(fmt::format_string<Args...>, Args&&...)
{
}
#endif

namespace vibecraft::app
{
namespace
{
struct WindowSettings
{
    std::uint32_t width = 1600;
    std::uint32_t height = 900;
};

struct StreamingSettings
{
    int bootstrapChunkRadius = 2;
    int residentChunkRadius = 3;
    int generationChunkRadius = 5;
    std::size_t generationChunkBudgetPerFrame = 12;
    std::size_t prefetchGenerationBudgetPerFrame = 6;
    /// Lower cap smooths frame time when many chunks need rebuilds (slightly slower catch-up).
    std::size_t meshBuildBudgetPerFrame = 5;
    std::size_t offResidentDirtyRebuildBudget = 8;
    int forwardPrefetchChunks = 2;
};

struct InputTuning
{
    float moveSpeed = 4.317f;
    float sneakSpeedMultiplier = 0.3f;
    float sprintSpeedMultiplier = 1.3f;
    float mouseSensitivity = 0.09f;
    float reachDistance = 6.0f;
};

struct PlayerMovementSettings
{
    float colliderHalfWidth = 0.3f;
    float standingColliderHeight = 1.8f;
    float sneakingColliderHeight = 1.5f;
    float standingEyeHeight = 1.62f;
    float sneakingEyeHeight = 1.27f;
    float gravity = 32.0f;
    float jumpVelocity = 8.4f;
    float terminalFallVelocity = 45.0f;
    float waterMoveSpeedMultiplier = 0.30f;
    /// Downward component while in water (before buoyancy). Tuned with `waterBuoyancyAcceleration` for neutral float.
    float waterGravity = 10.0f;
    /// Upward component while in water — match `waterGravity` for Minecraft-like neutral buoyancy (no idle sink).
    float waterBuoyancyAcceleration = 10.0f;
    float waterTerminalFallVelocity = 4.2f;
    float waterTerminalRiseVelocity = 4.0f;
    /// Space / jump while swimming (Minecraft-style fast rise).
    float waterSwimUpAcceleration = 15.0f;
    float waterSinkAcceleration = 6.0f;
    float waterVerticalDrag = 4.0f;
    float maxStepHeight = 0.6f;
    float collisionSweepStep = 0.2f;
    float groundProbeDistance = 0.05f;
    /// Minecraft-style liana climb in jungle (BlockType::Vines); blocks/sec along Y.
    float vineClimbSpeed = 2.45f;
    float vineDescendSpeed = 3.1f;
    /// Gravity multiplier while sliding down vines without input.
    float vineIdleFallGravityMultiplier = 0.14f;
};

constexpr WindowSettings kWindowSettings{};
constexpr StreamingSettings kStreamingSettings{};
constexpr InputTuning kInputTuning{};
constexpr PlayerMovementSettings kPlayerMovementSettings{};
constexpr float kFloatEpsilon = 0.0001f;
constexpr float kNetworkTickSeconds = 1.0f / 20.0f;

[[nodiscard]] std::uint32_t generateRandomWorldSeed()
{
    std::random_device randomDevice;
    const std::uint32_t deviceSeed = randomDevice();
    const std::uint64_t ticks = SDL_GetTicksNS();
    std::uint32_t seed = deviceSeed ^ static_cast<std::uint32_t>(ticks) ^ static_cast<std::uint32_t>(ticks >> 32U);
    if (seed == 0)
    {
        seed = 0x6d2b79f5U;
    }
    return seed;
}

[[nodiscard]] float normalizeDegrees(float degrees)
{
    while (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }
    while (degrees < -180.0f)
    {
        degrees += 360.0f;
    }
    return degrees;
}

[[nodiscard]] float lerpDegrees(const float fromDegrees, const float toDegrees, const float t)
{
    return normalizeDegrees(fromDegrees + normalizeDegrees(toDegrees - fromDegrees) * t);
}

[[nodiscard]] std::int64_t chestStorageKey(const glm::ivec3& blockPosition)
{
    const std::int64_t x = static_cast<std::int64_t>(blockPosition.x) & 0x1fffffLL;
    const std::int64_t y = static_cast<std::int64_t>(blockPosition.y) & 0x1fffffLL;
    const std::int64_t z = static_cast<std::int64_t>(blockPosition.z) & 0x1fffffLL;
    return (x << 42) | (y << 21) | z;
}

[[nodiscard]] bool isEarthyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::Grass || blockType == world::BlockType::Dirt
        || blockType == world::BlockType::Sand || blockType == world::BlockType::SnowGrass
        || blockType == world::BlockType::JungleGrass || blockType == world::BlockType::Gravel
        || blockType == world::BlockType::MossBlock
        || blockType == world::BlockType::Cactus || blockType == world::BlockType::Dandelion
        || blockType == world::BlockType::Poppy || blockType == world::BlockType::BlueOrchid
        || blockType == world::BlockType::Allium || blockType == world::BlockType::OxeyeDaisy
        || blockType == world::BlockType::BrownMushroom || blockType == world::BlockType::RedMushroom
        || blockType == world::BlockType::DeadBush || blockType == world::BlockType::Vines
        || blockType == world::BlockType::CocoaPod || blockType == world::BlockType::Melon
        || blockType == world::BlockType::Bamboo;
}

[[nodiscard]] bool isStoneFamilyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::Stone || blockType == world::BlockType::Deepslate
        || blockType == world::BlockType::CoalOre || blockType == world::BlockType::IronOre
        || blockType == world::BlockType::GoldOre || blockType == world::BlockType::DiamondOre
        || blockType == world::BlockType::EmeraldOre || blockType == world::BlockType::Bricks
        || blockType == world::BlockType::Glowstone || blockType == world::BlockType::Obsidian
        || blockType == world::BlockType::MossyCobblestone
        || blockType == world::BlockType::Glass;
}

[[nodiscard]] bool isWoodFamilyBlockType(const world::BlockType blockType)
{
    return blockType == world::BlockType::TreeTrunk || blockType == world::BlockType::TreeCrown
        || blockType == world::BlockType::JungleTreeTrunk
        || blockType == world::BlockType::JungleTreeCrown
        || blockType == world::BlockType::SnowTreeTrunk
        || blockType == world::BlockType::SnowTreeCrown
        || blockType == world::BlockType::Bookshelf || blockType == world::BlockType::JunglePlanks;
}

[[nodiscard]] bool isPickaxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isStoneFamilyBlockType(targetBlockType) || targetBlockType == world::BlockType::Cobblestone
        || targetBlockType == world::BlockType::Sandstone || targetBlockType == world::BlockType::Oven
        || targetBlockType == world::BlockType::Glass;
}

[[nodiscard]] bool isAxeEffectiveTarget(const world::BlockType targetBlockType)
{
    return isWoodFamilyBlockType(targetBlockType) || targetBlockType == world::BlockType::OakPlanks
        || targetBlockType == world::BlockType::JunglePlanks
        || targetBlockType == world::BlockType::CraftingTable || targetBlockType == world::BlockType::Chest
        || targetBlockType == world::BlockType::Bookshelf;
}

[[nodiscard]] bool isPickaxeItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isAxeItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
    case EquippedItem::DiamondAxe:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool isSwordItem(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::DiamondSword:
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return true;
    default:
        return false;
    }
}

/// 1 = wood ... 5 = diamond; 0 = not a tiered tool.
[[nodiscard]] int toolMaterialTier(const EquippedItem equippedItem)
{
    switch (equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::WoodPickaxe:
    case EquippedItem::WoodAxe:
        return 1;
    case EquippedItem::StoneSword:
    case EquippedItem::StonePickaxe:
    case EquippedItem::StoneAxe:
        return 2;
    case EquippedItem::IronSword:
    case EquippedItem::IronPickaxe:
    case EquippedItem::IronAxe:
        return 3;
    case EquippedItem::GoldSword:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::GoldAxe:
        return 4;
    case EquippedItem::DiamondSword:
    case EquippedItem::DiamondPickaxe:
    case EquippedItem::DiamondAxe:
        return 5;
    default:
        return 0;
    }
}

[[nodiscard]] float toolMiningSpeedMultiplier(
    const EquippedItem equippedItem,
    const world::BlockType targetBlockType)
{
    if (equippedItem == EquippedItem::None)
    {
        return 1.0f;
    }

    const int tier = toolMaterialTier(equippedItem);
    if (tier <= 0)
    {
        return 1.0f;
    }

    const float t = static_cast<float>(tier);

    if (isPickaxeItem(equippedItem))
    {
        if (!isPickaxeEffectiveTarget(targetBlockType))
        {
            return 1.06f;
        }
        return 1.12f + t * 0.64f;
    }

    if (isAxeItem(equippedItem))
    {
        if (!isAxeEffectiveTarget(targetBlockType))
        {
            return 1.05f;
        }
        return 1.22f + t * 0.8f;
    }

    if (isSwordItem(equippedItem))
    {
        if (isPickaxeEffectiveTarget(targetBlockType) || isAxeEffectiveTarget(targetBlockType))
        {
            return 0.58f + t * 0.06f;
        }
        return 1.04f + t * 0.05f;
    }

    return 1.0f;
}

[[nodiscard]] float miningSpeedMultiplier(
    const world::BlockType equippedBlockType,
    const world::BlockType targetBlockType)
{
    if (equippedBlockType == world::BlockType::Air)
    {
        return 1.0f;
    }
    if (equippedBlockType == targetBlockType)
    {
        return 2.0f;
    }
    if (isStoneFamilyBlockType(targetBlockType) && isStoneFamilyBlockType(equippedBlockType))
    {
        return 2.5f;
    }
    if (isEarthyBlockType(targetBlockType) && isEarthyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    if (isWoodFamilyBlockType(targetBlockType) && isWoodFamilyBlockType(equippedBlockType))
    {
        return 2.2f;
    }
    return 1.15f;
}

[[nodiscard]] float miningDurationSeconds(
    const world::BlockType targetBlockType,
    const world::BlockType equippedBlockType,
    const EquippedItem equippedItem)
{
    constexpr float kHardnessToSeconds = 0.65f;
    constexpr float kMinimumBreakDurationSeconds = 0.06f;
    const world::BlockMetadata metadata = world::blockMetadata(targetBlockType);
    if (!metadata.breakable)
    {
        return std::numeric_limits<float>::max();
    }

    const float blockMultiplier = miningSpeedMultiplier(equippedBlockType, targetBlockType);
    const float toolMultiplier = toolMiningSpeedMultiplier(equippedItem, targetBlockType);
    const float speedMultiplier = std::max(blockMultiplier, toolMultiplier);
    const float rawDurationSeconds = metadata.hardness * kHardnessToSeconds / speedMultiplier;
    return std::max(kMinimumBreakDurationSeconds, rawDurationSeconds);
}

[[nodiscard]] const char* timeOfDayLabel(const game::TimeOfDayPeriod period)
{
    switch (period)
    {
    case game::TimeOfDayPeriod::Dawn:
        return "dawn";
    case game::TimeOfDayPeriod::Day:
        return "day";
    case game::TimeOfDayPeriod::Dusk:
        return "dusk";
    case game::TimeOfDayPeriod::Night:
    default:
        return "night";
    }
}

[[nodiscard]] const char* weatherLabel(const game::WeatherType weatherType)
{
    switch (weatherType)
    {
    case game::WeatherType::Cloudy:
        return "cloudy";
    case game::WeatherType::Rain:
        return "rain";
    case game::WeatherType::Clear:
    default:
        return "clear";
    }
}

[[nodiscard]] std::uint64_t chunkMeshId(const world::ChunkCoord& coord)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(coord.x)) << 32U)
        | static_cast<std::uint32_t>(coord.z);
}

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

struct MeshSyncCpuData
{
    std::unordered_set<std::uint64_t> desiredResidentIds;
    std::vector<render::SceneMeshData> sceneMeshesToUpload;
    std::vector<std::uint64_t> removedMeshIds;
    std::vector<world::ChunkMeshUpdate> dirtyResidentMeshUpdates;
};

[[nodiscard]] MeshSyncCpuData buildMeshSyncCpuData(
    const world::World& world,
    const meshing::ChunkMesher& chunkMesher,
    const std::unordered_set<std::uint64_t>& residentChunkMeshIds,
    const std::unordered_set<world::ChunkCoord, world::ChunkCoordHash>& dirtyCoordSet,
    const world::ChunkCoord& cameraChunk,
    const std::size_t meshBuildBudget)
{
    MeshSyncCpuData cpuData;
    cpuData.sceneMeshesToUpload.reserve(dirtyCoordSet.size());
    cpuData.removedMeshIds.reserve(dirtyCoordSet.size() + residentChunkMeshIds.size());
    cpuData.dirtyResidentMeshUpdates.reserve(dirtyCoordSet.size());
    std::vector<std::pair<world::ChunkCoord, bool>> pendingResidentCoords;
    pendingResidentCoords.reserve(static_cast<std::size_t>(
        (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1)));

    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
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

[[nodiscard]] game::Aabb playerAabbAt(const glm::vec3& feetPosition, const float colliderHeight)
{
    return game::aabbAtFeet(feetPosition, kPlayerMovementSettings.colliderHalfWidth, colliderHeight);
}

[[nodiscard]] glm::vec3 findInitialSpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& preferredCameraPosition,
    const float colliderHeight)
{
    const int spawnWorldX = static_cast<int>(std::floor(preferredCameraPosition.x));
    const int spawnWorldZ = static_cast<int>(std::floor(preferredCameraPosition.z));
    glm::vec3 spawnFeetPosition(
        preferredCameraPosition.x,
        static_cast<float>(terrainGenerator.surfaceHeightAt(spawnWorldX, spawnWorldZ) + 1),
        preferredCameraPosition.z);

    // The expanded modern world height moved the terrain surface far above the legacy dev-camera
    // spawn. Start on top of the actual surface, then keep stepping upward until the player
    // capsule has full headroom (e.g. if a tree generated at the spawn column).
    constexpr int kSpawnClearanceSearchLimit = 64;
    for (int attempt = 0; attempt <= kSpawnClearanceSearchLimit; ++attempt)
    {
        if (!game::collidesWithSolidBlock(worldState, playerAabbAt(spawnFeetPosition, colliderHeight)))
        {
            return spawnFeetPosition;
        }
        spawnFeetPosition.y += 1.0f;
    }

    return spawnFeetPosition;
}

[[nodiscard]] const char* spawnPresetLabel(const SpawnPreset preset)
{
    switch (preset)
    {
    case SpawnPreset::North:
        return "Northlands";
    case SpawnPreset::South:
        return "Southlands";
    case SpawnPreset::East:
        return "Eastlands";
    case SpawnPreset::West:
        return "Westlands";
    case SpawnPreset::Origin:
    default:
        return "Origin";
    }
}

[[nodiscard]] const char* spawnBiomeTargetLabel(const SpawnBiomeTarget target)
{
    switch (target)
    {
    case SpawnBiomeTarget::Temperate:
        return "Temperate";
    case SpawnBiomeTarget::Sandy:
        return "Sandy";
    case SpawnBiomeTarget::Snowy:
        return "Snowy";
    case SpawnBiomeTarget::Jungle:
        return "Jungle";
    case SpawnBiomeTarget::Any:
    default:
        return "Any";
    }
}

[[nodiscard]] SpawnBiomeTarget nextSpawnBiomeTarget(const SpawnBiomeTarget target)
{
    switch (target)
    {
    case SpawnBiomeTarget::Any:
        return SpawnBiomeTarget::Temperate;
    case SpawnBiomeTarget::Temperate:
        return SpawnBiomeTarget::Sandy;
    case SpawnBiomeTarget::Sandy:
        return SpawnBiomeTarget::Snowy;
    case SpawnBiomeTarget::Snowy:
        return SpawnBiomeTarget::Jungle;
    case SpawnBiomeTarget::Jungle:
    default:
        return SpawnBiomeTarget::Any;
    }
}

[[nodiscard]] bool matchesSpawnBiomeTarget(
    const world::SurfaceBiome biome,
    const SpawnBiomeTarget target)
{
    using SB = world::SurfaceBiome;
    switch (target)
    {
    case SpawnBiomeTarget::Any:
        return true;
    case SpawnBiomeTarget::Temperate:
        return biome == SB::TemperateGrassland;
    case SpawnBiomeTarget::Sandy:
        return biome == SB::Sandy;
    case SpawnBiomeTarget::Snowy:
        return biome == SB::Snowy;
    case SpawnBiomeTarget::Jungle:
        return biome == SB::Jungle;
    }
    return true;
}

[[nodiscard]] SpawnPreset nextSpawnPreset(const SpawnPreset preset)
{
    switch (preset)
    {
    case SpawnPreset::Origin:
        return SpawnPreset::North;
    case SpawnPreset::North:
        return SpawnPreset::South;
    case SpawnPreset::South:
        return SpawnPreset::East;
    case SpawnPreset::East:
        return SpawnPreset::West;
    case SpawnPreset::West:
    default:
        return SpawnPreset::Origin;
    }
}

[[nodiscard]] glm::vec3 preferredSpawnProbePosition(
    const SpawnPreset preset,
    const glm::vec3& fallbackCameraPosition)
{
    constexpr float kSpawnOffset = 420.0f;
    switch (preset)
    {
    case SpawnPreset::North:
        return glm::vec3(0.0f, fallbackCameraPosition.y, -kSpawnOffset);
    case SpawnPreset::South:
        return glm::vec3(0.0f, fallbackCameraPosition.y, kSpawnOffset);
    case SpawnPreset::East:
        return glm::vec3(kSpawnOffset, fallbackCameraPosition.y, 0.0f);
    case SpawnPreset::West:
        return glm::vec3(-kSpawnOffset, fallbackCameraPosition.y, 0.0f);
    case SpawnPreset::Origin:
    default:
        return fallbackCameraPosition;
    }
}

[[nodiscard]] glm::vec3 resolveSpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const SpawnPreset spawnPreset,
    const SpawnBiomeTarget spawnBiomeTarget,
    const glm::vec3& fallbackCameraPosition,
    const float colliderHeight)
{
    const glm::vec3 spawnProbePosition = preferredSpawnProbePosition(spawnPreset, fallbackCameraPosition);
    if (spawnBiomeTarget == SpawnBiomeTarget::Any)
    {
        return findInitialSpawnFeetPosition(worldState, terrainGenerator, spawnProbePosition, colliderHeight);
    }

    // Search around the preferred spawn area for the requested biome.
    constexpr int kSearchStep = 24;
    constexpr int kSearchRadius = 1200;
    for (int radius = 0; radius <= kSearchRadius; radius += kSearchStep)
    {
        for (int dz = -radius; dz <= radius; dz += kSearchStep)
        {
            for (int dx = -radius; dx <= radius; dx += kSearchStep)
            {
                if (radius > 0 && std::abs(dx) != radius && std::abs(dz) != radius)
                {
                    continue;
                }
                const int sampleX = static_cast<int>(std::floor(spawnProbePosition.x)) + dx;
                const int sampleZ = static_cast<int>(std::floor(spawnProbePosition.z)) + dz;
                if (!matchesSpawnBiomeTarget(terrainGenerator.surfaceBiomeAt(sampleX, sampleZ), spawnBiomeTarget))
                {
                    continue;
                }
                const glm::vec3 biomeProbe{
                    static_cast<float>(sampleX),
                    spawnProbePosition.y,
                    static_cast<float>(sampleZ),
                };
                return findInitialSpawnFeetPosition(worldState, terrainGenerator, biomeProbe, colliderHeight);
            }
        }
    }

    return findInitialSpawnFeetPosition(worldState, terrainGenerator, spawnProbePosition, colliderHeight);
}

struct AxisMoveResult
{
    bool blocked = false;
    float appliedDisplacement = 0.0f;
};

[[nodiscard]] AxisMoveResult movePlayerAxisWithCollision(
    const world::World& worldState,
    glm::vec3& feetPosition,
    const int axisIndex,
    const float displacement,
    const float colliderHeight)
{
    if (std::abs(displacement) <= kFloatEpsilon)
    {
        return {};
    }

    float remaining = displacement;
    bool blocked = false;
    const float startPosition = feetPosition[axisIndex];

    while (std::abs(remaining) > kFloatEpsilon)
    {
        const float step = std::clamp(
            remaining,
            -kPlayerMovementSettings.collisionSweepStep,
            kPlayerMovementSettings.collisionSweepStep);
        const glm::vec3 basePosition = feetPosition;
        glm::vec3 candidatePosition = basePosition;
        candidatePosition[axisIndex] += step;

        if (!game::collidesWithSolidBlock(worldState, playerAabbAt(candidatePosition, colliderHeight)))
        {
            feetPosition = candidatePosition;
            remaining -= step;
            continue;
        }

        float low = 0.0f;
        float high = 1.0f;
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            const float mid = (low + high) * 0.5f;
            glm::vec3 sweepPosition = basePosition;
            sweepPosition[axisIndex] += step * mid;
            if (game::collidesWithSolidBlock(worldState, playerAabbAt(sweepPosition, colliderHeight)))
            {
                high = mid;
            }
            else
            {
                low = mid;
            }
        }

        feetPosition[axisIndex] += step * low;
        blocked = true;
        break;
    }

    return AxisMoveResult{
        .blocked = blocked,
        .appliedDisplacement = feetPosition[axisIndex] - startPosition,
    };
}

[[nodiscard]] bool tryStepUpAfterHorizontalBlock(
    const world::World& worldState,
    glm::vec3& feetPosition,
    const int axisIndex,
    const float remainingDisplacement,
    const float colliderHeight)
{
    if (axisIndex == 1 || std::abs(remainingDisplacement) <= kFloatEpsilon)
    {
        return false;
    }

    glm::vec3 steppedPosition = feetPosition;
    const AxisMoveResult stepUpResult = movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        1,
        kPlayerMovementSettings.maxStepHeight,
        colliderHeight);
    if (stepUpResult.blocked || stepUpResult.appliedDisplacement < kPlayerMovementSettings.maxStepHeight * 0.5f)
    {
        return false;
    }

    const AxisMoveResult horizontalResult = movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        axisIndex,
        remainingDisplacement,
        colliderHeight);
    if (horizontalResult.blocked)
    {
        return false;
    }

    static_cast<void>(movePlayerAxisWithCollision(
        worldState,
        steppedPosition,
        1,
        -(kPlayerMovementSettings.maxStepHeight + kPlayerMovementSettings.groundProbeDistance + kFloatEpsilon),
        colliderHeight));
    glm::vec3 groundedProbe = steppedPosition;
    groundedProbe.y -= kPlayerMovementSettings.groundProbeDistance;
    if (!game::collidesWithSolidBlock(worldState, playerAabbAt(groundedProbe, colliderHeight)))
    {
        return false;
    }
    if (steppedPosition.y <= feetPosition.y + kFloatEpsilon)
    {
        return false;
    }

    feetPosition = steppedPosition;
    return true;
}

[[nodiscard]] bool isGroundedAtFeetPosition(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight)
{
    glm::vec3 groundedProbe = feetPosition;
    groundedProbe.y -= kPlayerMovementSettings.groundProbeDistance;
    return game::collidesWithSolidBlock(worldState, playerAabbAt(groundedProbe, colliderHeight));
}

[[nodiscard]] bool aabbTouchesBlockType(
    const world::World& worldState,
    const game::Aabb& aabb,
    const world::BlockType blockType)
{
    const int minX = static_cast<int>(std::floor(aabb.min.x));
    const int minY = static_cast<int>(std::floor(aabb.min.y));
    const int minZ = static_cast<int>(std::floor(aabb.min.z));
    const int maxX = static_cast<int>(std::floor(aabb.max.x - game::kAabbEpsilon));
    const int maxY = static_cast<int>(std::floor(aabb.max.y - game::kAabbEpsilon));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z - game::kAabbEpsilon));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                if (worldState.blockAt(x, y, z) == blockType)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

[[nodiscard]] game::EnvironmentalHazards samplePlayerHazards(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight,
    const float eyeHeight)
{
    const game::Aabb playerBody = playerAabbAt(feetPosition, colliderHeight);
    const glm::vec3 eyeSamplePosition = feetPosition + glm::vec3(0.0f, eyeHeight - 0.1f, 0.0f);
    const world::BlockType eyeBlock = worldState.blockAt(
        static_cast<int>(std::floor(eyeSamplePosition.x)),
        static_cast<int>(std::floor(eyeSamplePosition.y)),
        static_cast<int>(std::floor(eyeSamplePosition.z)));

    return game::EnvironmentalHazards{
        .bodyInWater = aabbTouchesBlockType(worldState, playerBody, world::BlockType::Water),
        .bodyInLava = aabbTouchesBlockType(worldState, playerBody, world::BlockType::Lava),
        .headSubmergedInWater = eyeBlock == world::BlockType::Water,
    };
}

[[nodiscard]] const char* hazardLabel(const game::EnvironmentalHazards& hazards)
{
    if (hazards.bodyInLava)
    {
        return "lava";
    }
    if (hazards.headSubmergedInWater)
    {
        return "underwater";
    }
    if (hazards.bodyInWater)
    {
        return "water";
    }
    return "clear";
}

[[nodiscard]] std::string resolveJoinAddressFromEnvironment()
{
    if (const char* const address = std::getenv("VIBECRAFT_JOIN_ADDRESS"); address != nullptr
        && address[0] != '\0')
    {
        return std::string(address);
    }
    return "127.0.0.1";
}

void applyDefaultHotbarLoadout(HotbarSlots& hotbarSlots, std::size_t& selectedHotbarIndex)
{
    hotbarSlots.fill({});
    hotbarSlots[0].equippedItem = EquippedItem::DiamondSword;
    hotbarSlots[0].count = 1;
    hotbarSlots[0].blockType = world::BlockType::Air;
    selectedHotbarIndex = 0;
}

[[nodiscard]] float meleeDamageForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
        return 3.0f;
    case EquippedItem::StoneSword:
        return 4.0f;
    case EquippedItem::IronSword:
        return 5.0f;
    case EquippedItem::GoldSword:
        return 3.5f;
    case EquippedItem::DiamondSword:
        return 7.0f;
    case EquippedItem::WoodAxe:
        return 4.0f;
    case EquippedItem::StoneAxe:
        return 5.5f;
    case EquippedItem::IronAxe:
        return 6.5f;
    case EquippedItem::GoldAxe:
        return 4.25f;
    case EquippedItem::DiamondAxe:
        return 9.0f;
    case EquippedItem::WoodPickaxe:
        return 2.0f;
    case EquippedItem::StonePickaxe:
        return 2.75f;
    case EquippedItem::IronPickaxe:
        return 3.5f;
    case EquippedItem::GoldPickaxe:
        return 2.25f;
    case EquippedItem::DiamondPickaxe:
        return 5.0f;
    case EquippedItem::None:
    default:
        return 1.0f;
    }
}

[[nodiscard]] float meleeReachForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return 3.05f;
    case EquippedItem::DiamondSword:
        return 3.45f;
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
    case EquippedItem::DiamondAxe:
        return 3.1f;
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return 2.85f;
    case EquippedItem::None:
    default:
        return 2.75f;
    }
}

[[nodiscard]] float knockbackDistanceForSlot(const InventorySlot& slot)
{
    switch (slot.equippedItem)
    {
    case EquippedItem::WoodSword:
    case EquippedItem::StoneSword:
        return 0.55f;
    case EquippedItem::IronSword:
    case EquippedItem::GoldSword:
        return 0.65f;
    case EquippedItem::DiamondSword:
        return 0.9f;
    case EquippedItem::WoodAxe:
    case EquippedItem::StoneAxe:
        return 0.62f;
    case EquippedItem::IronAxe:
    case EquippedItem::GoldAxe:
        return 0.72f;
    case EquippedItem::DiamondAxe:
        return 0.95f;
    case EquippedItem::WoodPickaxe:
    case EquippedItem::StonePickaxe:
        return 0.48f;
    case EquippedItem::IronPickaxe:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::DiamondPickaxe:
        return 0.52f;
    case EquippedItem::None:
    default:
        return 0.45f;
    }
}

[[nodiscard]] EquippedItem mobDropItemForKind(const game::MobKind mobKind)
{
    using MK = game::MobKind;
    switch (mobKind)
    {
    case MK::HostileStalker:
        return EquippedItem::RottenFlesh;
    case MK::Player:
        return EquippedItem::None;
    case MK::Cow:
        return EquippedItem::Leather;
    case MK::Pig:
        return EquippedItem::RawPorkchop;
    case MK::Sheep:
        return EquippedItem::Mutton;
    case MK::Chicken:
        return EquippedItem::Feather;
    }
    return EquippedItem::RottenFlesh;
}

[[nodiscard]] render::HudItemKind hudItemKindForEquippedItem(const EquippedItem equippedItem)
{
    return static_cast<render::HudItemKind>(equippedItem);
}

[[nodiscard]] world::BlockType networkFallbackBlockTypeForEquippedItem(const EquippedItem equippedItem)
{
    using BK = world::BlockType;
    switch (equippedItem)
    {
    case EquippedItem::Stick:
        return BK::TreeTrunk;
    case EquippedItem::RottenFlesh:
        return BK::CoalOre;
    case EquippedItem::Leather:
        return BK::TreeTrunk;
    case EquippedItem::RawPorkchop:
        return BK::Dirt;
    case EquippedItem::Mutton:
        return BK::TreeCrown;
    case EquippedItem::Feather:
        return BK::Sand;
    case EquippedItem::Coal:
        return BK::CoalOre;
    case EquippedItem::WoodSword:
    case EquippedItem::WoodPickaxe:
    case EquippedItem::WoodAxe:
        return BK::OakPlanks;
    case EquippedItem::StoneSword:
    case EquippedItem::StonePickaxe:
    case EquippedItem::StoneAxe:
        return BK::Cobblestone;
    case EquippedItem::IronSword:
    case EquippedItem::IronPickaxe:
    case EquippedItem::IronAxe:
        return BK::IronOre;
    case EquippedItem::GoldSword:
    case EquippedItem::GoldPickaxe:
    case EquippedItem::GoldAxe:
        return BK::GoldOre;
    case EquippedItem::DiamondSword:
    case EquippedItem::DiamondPickaxe:
    case EquippedItem::DiamondAxe:
        return BK::DiamondOre;
    case EquippedItem::None:
    default:
        return BK::Air;
    }
}

template <std::size_t SlotCount>
[[nodiscard]] bool addEquippedItemToSlots(
    std::array<InventorySlot, SlotCount>& slots,
    const EquippedItem equippedItem,
    std::size_t* const selectedHotbarIndex)
{
    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.equippedItem == equippedItem && slot.count < kMaxStackSize)
        {
            ++slot.count;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        InventorySlot& slot = slots[slotIndex];
        if (slot.count == 0)
        {
            slot.blockType = world::BlockType::Air;
            slot.equippedItem = equippedItem;
            slot.count = 1;
            if (selectedHotbarIndex != nullptr)
            {
                *selectedHotbarIndex = slotIndex;
            }
            return true;
        }
    }

    return false;
}

[[nodiscard]] bool addEquippedItemToInventory(
    HotbarSlots& hotbarSlots,
    BagSlots& bagSlots,
    const EquippedItem equippedItem,
    std::size_t& selectedHotbarIndex)
{
    if (equippedItem == EquippedItem::None)
    {
        return false;
    }
    if (addEquippedItemToSlots(hotbarSlots, equippedItem, &selectedHotbarIndex))
    {
        return true;
    }
    return addEquippedItemToSlots(bagSlots, equippedItem, nullptr);
}

[[nodiscard]] render::FrameDebugData::HotbarSlotHud makeHudSlot(const InventorySlot& slot)
{
    return render::FrameDebugData::HotbarSlotHud{
        .blockType = slot.blockType,
        .count = slot.count,
        .itemKind = hudItemKindForEquippedItem(slot.equippedItem),
        .heldItemUsesSwordPose = isSwordItem(slot.equippedItem),
    };
}

[[nodiscard]] bool canPlaceIntoCraftingGrid(const InventorySlot& slot)
{
    return slot.count > 0
        && (slot.equippedItem != EquippedItem::None || slot.blockType != world::BlockType::Air);
}

[[nodiscard]] bool canReceiveCraftingOutput(
    const InventorySlot& carriedSlot,
    const InventorySlot& outputSlot)
{
    return isInventorySlotEmpty(carriedSlot)
        || (canMergeInventorySlots(carriedSlot, outputSlot)
            && carriedSlot.count + outputSlot.count <= kMaxStackSize);
}

void mergeOrSwapInventorySlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const bool allowPlacedEquippedItem)
{
    if (!allowPlacedEquippedItem && carriedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }
    if (!allowPlacedEquippedItem && targetSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    if (isInventorySlotEmpty(carriedSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }
    if (isInventorySlotEmpty(targetSlot))
    {
        std::swap(carriedSlot, targetSlot);
        return;
    }

    if (canMergeInventorySlots(carriedSlot, targetSlot) && targetSlot.count < kMaxStackSize)
    {
        const std::uint32_t space = kMaxStackSize - targetSlot.count;
        const std::uint32_t transfer = std::min(space, carriedSlot.count);
        targetSlot.count += transfer;
        carriedSlot.count -= transfer;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
        return;
    }

    std::swap(carriedSlot, targetSlot);
}

void rightClickInventorySlot(
    InventorySlot& carriedSlot,
    InventorySlot& targetSlot,
    const bool allowPlacedEquippedItem)
{
    if (!allowPlacedEquippedItem && !isInventorySlotEmpty(carriedSlot)
        && carriedSlot.equippedItem != EquippedItem::None)
    {
        return;
    }
    if (!allowPlacedEquippedItem && !isInventorySlotEmpty(targetSlot)
        && targetSlot.equippedItem != EquippedItem::None)
    {
        return;
    }

    if (isInventorySlotEmpty(carriedSlot))
    {
        if (isInventorySlotEmpty(targetSlot))
        {
            return;
        }

        carriedSlot = targetSlot;
        carriedSlot.count = (targetSlot.count + 1U) / 2U;
        targetSlot.count -= carriedSlot.count;
        if (targetSlot.count == 0)
        {
            clearInventorySlot(targetSlot);
        }
        return;
    }

    if (isInventorySlotEmpty(targetSlot))
    {
        targetSlot.blockType = carriedSlot.blockType;
        targetSlot.equippedItem = carriedSlot.equippedItem;
        targetSlot.count = 1;
        --carriedSlot.count;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
        return;
    }

    if (canMergeInventorySlots(carriedSlot, targetSlot) && targetSlot.count < kMaxStackSize)
    {
        ++targetSlot.count;
        --carriedSlot.count;
        if (carriedSlot.count == 0)
        {
            clearInventorySlot(carriedSlot);
        }
    }
}

void trimInPlace(std::string& value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.pop_back();
    }
}

[[nodiscard]] std::string trimCopy(std::string value)
{
    trimInPlace(value);
    return value;
}

}

bool Application::initialize()
{
    core::initializeLogger();

    if (!window_.create("VibeCraft", kWindowSettings.width, kWindowSettings.height))
    {
        return false;
    }

    // Keep startup fast by deferring world creation until the player chooses Singleplayer.
    {
        vibecraft::world::World::ChunkMap emptyChunks;
        world_.replaceChunks(std::move(emptyChunks));
    }

    multiplayerAddress_ = resolveJoinAddressFromEnvironment();
    loadMultiplayerPrefs();
    loadAudioPrefs();

    if (!renderer_.initialize(window_.nativeWindowHandle(), window_.width(), window_.height()))
    {
        core::logError("Failed to initialize bgfx.");
        return false;
    }

    const std::filesystem::path minecraftAudioRoot = audio::resolveMinecraftAudioRoot();
    core::logInfo(fmt::format("Minecraft audio assets: {}", minecraftAudioRoot.generic_string()));
    audio::logMinecraftAudioPackDiagnostics(minecraftAudioRoot);
    if (!sharedAudioOutput_.initialize())
    {
        core::logWarning("Shared audio output failed to open; music and SFX are disabled.");
    }
    else
    {
        if (!musicDirector_.initialize(sharedAudioOutput_.musicStream(), minecraftAudioRoot))
        {
            core::logWarning("Music system failed to initialize; continuing without music.");
        }
        if (!soundEffects_.initialize(sharedAudioOutput_.sfxStream(), minecraftAudioRoot))
        {
            core::logWarning("SFX system failed to initialize; continuing without sound effects.");
        }
    }
    musicDirector_.setMasterGain(musicVolume_);
    soundEffects_.setMasterGain(sfxVolume_);

    camera_.addYawPitch(90.0f, 0.0f);
    dayNightCycle_.setElapsedSeconds(70.0f);
    weatherSystem_.setElapsedSeconds(0.0f);

    gameScreen_ = GameScreen::MainMenu;
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    mainMenuNotice_ = "Multiplayer: open Multiplayer, then Host game or Join game.";
    applyDefaultHotbarLoadout(hotbarSlots_, selectedHotbarIndex_);
    return true;
}

int Application::run()
{
    std::uint64_t lastCounter = SDL_GetPerformanceCounter();
    const std::uint64_t frequency = SDL_GetPerformanceFrequency();

    while (!inputState_.quitRequested)
    {
        const std::uint64_t currentCounter = SDL_GetPerformanceCounter();
        const float deltaTimeSeconds =
            static_cast<float>(currentCounter - lastCounter) / static_cast<float>(frequency);
        lastCounter = currentCounter;

        window_.pollEvents(inputState_);
        processInput(deltaTimeSeconds);
        update(deltaTimeSeconds);
    }

    saveAudioPrefs();
    stopMultiplayerSessions();
    musicDirector_.shutdown();
    soundEffects_.shutdown();
    sharedAudioOutput_.shutdown();
    renderer_.shutdown();
    return 0;
}

void Application::update(const float deltaTimeSeconds)
{
    dayNightCycle_.advanceSeconds(deltaTimeSeconds);
    weatherSystem_.advanceSeconds(deltaTimeSeconds);
    const game::DayNightSample dayNightSample = dayNightCycle_.sample();
    const game::WeatherSample weatherSample = weatherSystem_.sample();
    const glm::vec3 clearSkyTint = dayNightSample.skyTint * weatherSample.skyTintMultiplier;
    const glm::vec3 clearHorizonTint = dayNightSample.horizonTint * weatherSample.horizonTintMultiplier;
    const glm::vec3 skyTint =
        glm::mix(clearSkyTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.28f);
    const glm::vec3 horizonTint =
        glm::mix(clearHorizonTint, weatherSample.cloudTint, weatherSample.cloudCoverage * 0.18f);
    const float sunLightScale = 1.0f - weatherSample.sunOcclusion * 0.55f;
    const float moonLightScale = 1.0f - weatherSample.cloudCoverage * 0.20f;
    const float visibleSunScale = 1.0f - weatherSample.sunOcclusion * 0.35f;
    const float visibleMoonScale = 1.0f - weatherSample.cloudCoverage * 0.12f;

    const float frameTimeMs = deltaTimeSeconds * 1000.0f;
    if (!frameTimeInitialized_)
    {
        smoothedFrameTimeMs_ = frameTimeMs;
        frameTimeInitialized_ = true;
    }
    else
    {
        constexpr float kFrameTimeSmoothingAlpha = 0.1f;
        smoothedFrameTimeMs_ =
            smoothedFrameTimeMs_ + (frameTimeMs - smoothedFrameTimeMs_) * kFrameTimeSmoothingAlpha;
    }

    if (singleplayerLoadState_.active)
    {
        updateSingleplayerLoad();
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        mainMenuTimeSeconds_ += deltaTimeSeconds;
    }

    updateMultiplayer(deltaTimeSeconds);

    if (inputState_.windowSizeChanged && window_.width() != 0 && window_.height() != 0)
    {
        renderer_.resize(window_.width(), window_.height());
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        syncWorldData();
        const float currentEyeHeight = std::max(0.0f, camera_.position().y - playerFeetPosition_.y);
        updateDroppedItems(deltaTimeSeconds, currentEyeHeight);
    }

    // Multiplayer clients do not receive mob state from the host yet; skip local simulation.
    if (gameScreen_ == GameScreen::Playing && multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        const float healthBeforeMobTick = playerVitals_.health();
        mobSpawnSystem_.tick(
            world_,
            terrainGenerator_,
            playerFeetPosition_,
            kPlayerMovementSettings.colliderHalfWidth,
            deltaTimeSeconds,
            dayNightSample.period,
            mobSpawningEnabled_,
            playerVitals_);
        if (!creativeModeEnabled_ && playerVitals_.health() + 0.001f < healthBeforeMobTick)
        {
            soundEffects_.playPlayerHurt();
        }
    }

    std::optional<world::RaycastHit> raycastHit;
    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        raycastHit = world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    }

    render::FrameDebugData frameDebugData;
    frameDebugData.chunkCount = static_cast<std::uint32_t>(world_.chunks().size());
    frameDebugData.dirtyChunkCount = static_cast<std::uint32_t>(world_.dirtyChunkCount());
    frameDebugData.totalFaces = world_.totalVisibleFaces();
    frameDebugData.residentChunkCount = static_cast<std::uint32_t>(residentChunkMeshIds_.size());
    frameDebugData.cameraPosition = camera_.position();
    frameDebugData.uiCursorX = inputState_.mouseWindowX;
    frameDebugData.uiCursorY = inputState_.mouseWindowY;
    frameDebugData.showWorldOriginGuides = showWorldOriginGuides_;
    frameDebugData.health = playerVitals_.health();
    frameDebugData.maxHealth = playerVitals_.maxHealth();
    const float safeFrameTimeMs = std::max(smoothedFrameTimeMs_, 0.001f);
    const float smoothedFps = 1000.0f / safeFrameTimeMs;
    const int cycleSeconds = static_cast<int>(std::floor(dayNightSample.elapsedSeconds));
    const int cycleMinutesComponent = cycleSeconds / 60;
    const int cycleSecondsComponent = cycleSeconds % 60;
    frameDebugData.statusLine = fmt::format(
        "HP: {:.0f}/{:.0f}  Air: {:.0f}/{:.0f}  Hazard: {}  Mouse: {}  Grounded: {}  Time: {} {:02d}:{:02d}  Weather: {}  Save: {}  Net: {}  Peers: {}  Frame: {:.2f} ms ({:.1f} fps){}",
        playerVitals_.health(),
        playerVitals_.maxHealth(),
        playerVitals_.air(),
        playerVitals_.maxAir(),
        hazardLabel(playerHazards_),
        mouseCaptured_ ? "captured" : "released",
        isGrounded_ ? "yes" : "no",
        timeOfDayLabel(dayNightSample.period),
        cycleMinutesComponent,
        cycleSecondsComponent,
        weatherLabel(weatherSample.type),
        savePath_.generic_string(),
        multiplayerStatusLine_.empty() ? "offline" : multiplayerStatusLine_,
        remotePlayers_.size(),
        safeFrameTimeMs,
        smoothedFps,
        respawnNotice_.empty() ? "" : fmt::format("  {}", respawnNotice_));
    for (std::size_t slotIndex = 0; slotIndex < frameDebugData.hotbarSlots.size(); ++slotIndex)
    {
        frameDebugData.hotbarSlots[slotIndex] = makeHudSlot(hotbarSlots_[slotIndex]);
    }
    frameDebugData.hotbarSelectedIndex = selectedHotbarIndex_;
    frameDebugData.heldItemSwing = heldItemSwing_;
    for (std::size_t i = 0; i < frameDebugData.bagSlots.size(); ++i)
    {
        frameDebugData.bagSlots[i] = makeHudSlot(bagSlots_[i]);
    }
    if (craftingMenuState_.active)
    {
        frameDebugData.craftingMenuActive = true;
        frameDebugData.craftingUsesWorkbench = craftingMenuState_.usesWorkbench;
        if (craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage)
        {
            frameDebugData.craftingTitle = "Chest";
        }
        else
        {
            frameDebugData.craftingTitle =
                craftingMenuState_.usesWorkbench ? "Crafting Table" : "Inventory Crafting";
        }
        frameDebugData.craftingBagStartRow =
            static_cast<std::uint8_t>(std::min<std::size_t>(craftingMenuState_.bagStartRow, 255));
        const std::size_t visibleStart = craftingMenuState_.bagStartRow * 9;
        const std::size_t visibleEndExclusive =
            std::min<std::size_t>(visibleStart + 27, bagSlots_.size());
        frameDebugData.craftingHint = fmt::format(
            "{}  |  Bag slots: {}-{} / {} (mouse wheel to scroll)",
            craftingMenuState_.hint,
            visibleStart + 1,
            visibleEndExclusive,
            bagSlots_.size());
        frameDebugData.craftingCursorSlot = makeHudSlot(craftingMenuState_.carriedSlot);
        if (craftingMenuState_.mode != CraftingMenuState::Mode::ChestStorage)
        {
            if (const std::optional<CraftingMatch> craftingMatch = evaluateCraftingGrid(
                    craftingMenuState_.gridSlots,
                    craftingMenuState_.usesWorkbench ? CraftingMode::Workbench3x3 : CraftingMode::Inventory2x2);
                craftingMatch.has_value())
            {
                frameDebugData.craftingResultSlot = makeHudSlot(craftingMatch->output);
            }
        }
        for (std::size_t slotIndex = 0; slotIndex < frameDebugData.craftingGridSlots.size(); ++slotIndex)
        {
            frameDebugData.craftingGridSlots[slotIndex] = makeHudSlot(craftingMenuState_.gridSlots[slotIndex]);
        }
    }
    frameDebugData.worldPickups.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        const float bobOffset = 0.14f + std::sin(droppedItem.ageSeconds * 6.0f) * 0.08f;
        frameDebugData.worldPickups.push_back(render::FrameDebugData::WorldPickupHud{
            .blockType = droppedItem.blockType,
            .itemKind = hudItemKindForEquippedItem(droppedItem.equippedItem),
            .worldPosition = droppedItem.worldPosition + glm::vec3(0.0f, bobOffset, 0.0f),
            .spinRadians = droppedItem.spinRadians,
        });
    }

    if (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused)
    {
        frameDebugData.worldMobs.reserve(mobSpawnSystem_.mobs().size() + remotePlayers_.size());
        for (const game::MobInstance& mob : mobSpawnSystem_.mobs())
        {
            frameDebugData.worldMobs.push_back(render::FrameDebugData::WorldMobHud{
                .feetPosition = {mob.feetX, mob.feetY, mob.feetZ},
                .yawRadians = mob.yawRadians,
                .pitchRadians = 0.0f,
                .halfWidth = mob.halfWidth,
                .height = mob.height,
                .mobKind = mob.kind,
            });
        }
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        for (const RemotePlayerState& remotePlayer : remotePlayers_)
        {
            frameDebugData.worldMobs.push_back(render::FrameDebugData::WorldMobHud{
                .feetPosition = remotePlayer.position,
                .yawRadians = remotePlayer.yawDegrees * kDegreesToRadians,
                .pitchRadians = remotePlayer.pitchDegrees * kDegreesToRadians,
                .halfWidth = 0.30f,
                .height = 2.0f,
                .mobKind = game::MobKind::Player,
                .heldBlockType = remotePlayer.selectedBlockType,
                .heldItemKind = hudItemKindForEquippedItem(remotePlayer.selectedEquippedItem),
                .heldItemUsesSwordPose = isSwordItem(remotePlayer.selectedEquippedItem),
            });
        }
    }

    if (raycastHit.has_value())
    {
        frameDebugData.hasTarget = true;
        frameDebugData.targetBlock = raycastHit->solidBlock;
        if (activeMiningState_.active
            && activeMiningState_.targetBlockPosition == raycastHit->solidBlock
            && activeMiningState_.requiredSeconds > 0.0f)
        {
            frameDebugData.miningTargetActive = true;
            frameDebugData.miningTargetProgress = std::clamp(
                activeMiningState_.elapsedSeconds / activeMiningState_.requiredSeconds,
                0.0f,
                1.0f);
        }
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        frameDebugData.mainMenuActive = true;
        frameDebugData.mainMenuTimeSeconds = mainMenuTimeSeconds_;
        frameDebugData.mainMenuNotice = mainMenuNotice_;
        frameDebugData.mainMenuCreativeModeEnabled = creativeModeEnabled_;
        frameDebugData.mainMenuSpawnPresetLabel = spawnPresetLabel(spawnPreset_);
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight =
            stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;
        if (mainMenuSoundSettingsOpen_)
        {
            frameDebugData.mainMenuSoundSettingsActive = true;
            frameDebugData.mainMenuSoundMusicVolume = musicVolume_;
            frameDebugData.mainMenuSoundSfxVolume = sfxVolume_;
            frameDebugData.mainMenuSoundSettingsHoveredControl = render::Renderer::hitTestPauseSoundMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                textWidth,
                textHeight);
        }
        else if (singleplayerLoadState_.active)
        {
            frameDebugData.mainMenuHoveredButton = -1;
            frameDebugData.mainMenuLoadingActive = true;
            frameDebugData.mainMenuLoadingProgress = singleplayerLoadState_.progress;
            frameDebugData.mainMenuLoadingLabel = singleplayerLoadState_.label;
        }
        else if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None)
        {
            frameDebugData.mainMenuMultiplayerPanel = mainMenuMultiplayerPanel_;
            frameDebugData.mainMenuMultiplayerLanAddress = detectedLanAddress_;
            frameDebugData.mainMenuJoinAddressField = joinAddressInput_;
            frameDebugData.mainMenuJoinPortField = joinPortInput_;
            frameDebugData.mainMenuMultiplayerPortDisplay = multiplayerPort_;
            frameDebugData.mainMenuJoinFocusedField = joinFocusedField_;
            frameDebugData.mainMenuJoinPresetButtonLabels.clear();
            for (const JoinPresetEntry& preset : joinPresets_)
            {
                frameDebugData.mainMenuJoinPresetButtonLabels.push_back(
                    fmt::format("{} — {}:{}", preset.label, preset.host, preset.port));
            }
            const int joinSlotsForMpLayout = static_cast<int>(std::min(joinPresets_.size(), std::size_t(3)));
            const int mainMenuContentTopBias = vibecraft::render::detail::mainMenuLogoReservedDbgRows(
                window_.width(),
                window_.height(),
                textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
            const int multiplayerRowShift = render::Renderer::multiplayerMenuRowShift(
                textHeight,
                mainMenuMultiplayerPanel_,
                mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join ? joinSlotsForMpLayout : 0,
                mainMenuContentTopBias);
            switch (mainMenuMultiplayerPanel_)
            {
            case MainMenuMultiplayerPanel::Hub:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerHub(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::Host:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerHost(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::Join:
                frameDebugData.mainMenuMultiplayerHoveredControl = render::Renderer::hitTestMainMenuMultiplayerJoin(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight,
                    joinSlotsForMpLayout,
                    multiplayerRowShift,
                    mainMenuContentTopBias);
                break;
            case MainMenuMultiplayerPanel::None:
            default:
                frameDebugData.mainMenuMultiplayerHoveredControl = -1;
                break;
            }
        }
        else
        {
            frameDebugData.mainMenuHoveredButton = render::Renderer::hitTestMainMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                textWidth,
                textHeight,
                renderer_.menuLogoWidthPx(),
                renderer_.menuLogoHeightPx());
        }
        if (singleplayerLoadState_.active)
        {
            frameDebugData.mainMenuLoadingActive = true;
            frameDebugData.mainMenuLoadingProgress = singleplayerLoadState_.progress;
            if (pendingHostStartAfterWorldLoad_)
            {
                frameDebugData.mainMenuLoadingTitle = "STARTING MULTIPLAYER HOST";
            }
            else if (pendingClientJoinAfterWorldLoad_)
            {
                frameDebugData.mainMenuLoadingTitle = "JOINING MULTIPLAYER";
            }
            else
            {
                frameDebugData.mainMenuLoadingTitle = "LOADING SINGLEPLAYER";
            }
            frameDebugData.mainMenuLoadingLabel = singleplayerLoadState_.label;
        }
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        frameDebugData.pauseMenuActive = true;
        frameDebugData.pauseMenuNotice = pauseMenuNotice_;
        const bgfx::Stats* const pauseStats = bgfx::getStats();
        const std::uint16_t pauseTextWidth =
            pauseStats != nullptr && pauseStats->textWidth > 0 ? pauseStats->textWidth : 100;
        const std::uint16_t pauseTextHeight =
            pauseStats != nullptr && pauseStats->textHeight > 0 ? pauseStats->textHeight : 30;
        if (pauseGameSettingsOpen_)
        {
            frameDebugData.pauseGameSettingsActive = true;
            frameDebugData.mobSpawningEnabled = mobSpawningEnabled_;
            frameDebugData.pauseSpawnBiomeLabel = spawnBiomeTargetLabel(spawnBiomeTarget_);
            frameDebugData.pauseGameSettingsHoveredControl = render::Renderer::hitTestPauseGameSettingsMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                pauseTextWidth,
                pauseTextHeight);
        }
        else if (pauseSoundSettingsOpen_)
        {
            frameDebugData.pauseSoundSettingsActive = true;
            frameDebugData.pauseSoundMusicVolume = musicVolume_;
            frameDebugData.pauseSoundSfxVolume = sfxVolume_;
            int hoveredControl = render::Renderer::hitTestPauseSoundMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                pauseTextWidth,
                pauseTextHeight);
            if (render::Renderer::pauseSoundSliderValueFromMouse(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    pauseTextWidth,
                    pauseTextHeight,
                    true)
                .has_value())
            {
                hoveredControl = 1;
            }
            else if (render::Renderer::pauseSoundSliderValueFromMouse(
                         inputState_.mouseWindowX,
                         inputState_.mouseWindowY,
                         window_.width(),
                         window_.height(),
                         pauseTextWidth,
                         pauseTextHeight,
                         false)
                         .has_value())
            {
                hoveredControl = 3;
            }
            frameDebugData.pauseSoundSettingsHoveredControl = hoveredControl;
        }
        else
        {
            frameDebugData.pauseMenuHoveredButton = render::Renderer::hitTestPauseMenu(
                inputState_.mouseWindowX,
                inputState_.mouseWindowY,
                window_.width(),
                window_.height(),
                pauseTextWidth,
                pauseTextHeight);
        }
    }

    audio::MusicContext musicContext = audio::MusicContext::OverworldDay;
    if (gameScreen_ == GameScreen::MainMenu)
    {
        musicContext = audio::MusicContext::Menu;
    }
    else if (playerHazards_.headSubmergedInWater)
    {
        musicContext = audio::MusicContext::Underwater;
    }
    else if (dayNightSample.period == game::TimeOfDayPeriod::Dusk
             || dayNightSample.period == game::TimeOfDayPeriod::Night)
    {
        musicContext = audio::MusicContext::OverworldNight;
    }
    musicDirector_.update(deltaTimeSeconds, musicContext);

    renderer_.renderFrame(
        frameDebugData,
        render::CameraFrameData{
            .position = camera_.position(),
            .forward = camera_.forward(),
            .up = camera_.up(),
            .skyTint = skyTint,
            .horizonTint = horizonTint,
            .sunDirection = dayNightSample.sunDirection,
            .moonDirection = dayNightSample.moonDirection,
            .sunLightTint = dayNightSample.sunLightTint * sunLightScale,
            .moonLightTint = dayNightSample.moonLightTint * moonLightScale,
            .cloudTint = weatherSample.cloudTint,
            .weatherWindDirectionXZ = weatherSample.windDirectionXZ,
            .sunVisibility = dayNightSample.sunVisibility * visibleSunScale,
            .moonVisibility = dayNightSample.moonVisibility * visibleMoonScale,
            .cloudCoverage = weatherSample.cloudCoverage,
            .rainIntensity = weatherSample.rainIntensity,
            .weatherTimeSeconds = weatherSample.elapsedSeconds,
            .weatherWindSpeed = weatherSample.windSpeed,
        });
}

void Application::processInput(const float deltaTimeSeconds)
{
    const bool allowMainMenuPointerInputWhileUnfocused =
        gameScreen_ == GameScreen::MainMenu
        && (inputState_.leftMousePressed || inputState_.leftMouseClicked);
    if (!inputState_.windowFocused && !allowMainMenuPointerInputWhileUnfocused)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        return;
    }

    const bool f3Down = inputState_.isKeyDown(SDL_SCANCODE_F3);
    if (f3Down && !debugF3KeyWasDown_
        && (gameScreen_ == GameScreen::Playing || gameScreen_ == GameScreen::Paused))
    {
        showWorldOriginGuides_ = !showWorldOriginGuides_;
    }
    debugF3KeyWasDown_ = f3Down;

    const bool craftingKeyDown = inputState_.isKeyDown(SDL_SCANCODE_E);
    const bool craftingKeyPressed = craftingKeyDown && !craftingKeyWasDown_;
    craftingKeyWasDown_ = craftingKeyDown;

    if (craftingMenuState_.active && (inputState_.escapePressed || craftingKeyPressed))
    {
        closeCraftingMenu();
        return;
    }

    if (inputState_.escapePressed)
    {
        if (gameScreen_ == GameScreen::Playing)
        {
            gameScreen_ = GameScreen::Paused;
            pauseSoundSettingsOpen_ = false;
            pauseGameSettingsOpen_ = false;
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
            pauseMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::Paused)
        {
            if (pauseGameSettingsOpen_)
            {
                pauseGameSettingsOpen_ = false;
                pauseMenuNotice_.clear();
            }
            else if (pauseSoundSettingsOpen_)
            {
                pauseSoundSettingsOpen_ = false;
                pauseMenuNotice_ = "Sound settings saved.";
                saveAudioPrefs();
            }
            else
            {
                gameScreen_ = GameScreen::Playing;
                mouseCaptured_ = true;
                window_.setRelativeMouseMode(true);
                pauseMenuNotice_.clear();
                inputState_.clearMouseMotion();
            }
        }
        else if (gameScreen_ == GameScreen::MainMenu && mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None
                 && !mainMenuSoundSettingsOpen_)
        {
            if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Hub)
            {
                mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
            }
            else
            {
                mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
            }
            window_.setTextInputActive(false);
            mainMenuNotice_.clear();
        }
        else if (gameScreen_ == GameScreen::MainMenu && mainMenuSoundSettingsOpen_)
        {
            mainMenuSoundSettingsOpen_ = false;
            mainMenuNotice_ = "Sound settings saved.";
            saveAudioPrefs();
        }
    }

    if (inputState_.releaseMouseRequested)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
    }

    if (inputState_.captureMouseRequested && gameScreen_ == GameScreen::Playing)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }
    if (inputState_.tabPressed && gameScreen_ == GameScreen::Playing)
    {
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }

    if (gameScreen_ == GameScreen::MainMenu)
    {
        if (mouseCaptured_)
        {
            mouseCaptured_ = false;
            window_.setRelativeMouseMode(false);
        }
        processJoinMenuTextInput();
        if (singleplayerLoadState_.active)
        {
            return;
        }
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight =
            stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;
        const bool creativeToggleKeyDown = inputState_.isKeyDown(SDL_SCANCODE_C);
        const bool spawnPresetToggleKeyDown = inputState_.isKeyDown(SDL_SCANCODE_V);
        if (creativeToggleKeyDown && !creativeToggleKeyWasDown_)
        {
            creativeModeEnabled_ = !creativeModeEnabled_;
            mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
        }
        if (spawnPresetToggleKeyDown && !spawnPresetToggleKeyWasDown_)
        {
            spawnPreset_ = nextSpawnPreset(spawnPreset_);
            mainMenuNotice_ = fmt::format("Spawn preset: {}", spawnPresetLabel(spawnPreset_));
        }
        creativeToggleKeyWasDown_ = creativeToggleKeyDown;
        spawnPresetToggleKeyWasDown_ = spawnPresetToggleKeyDown;

        // Hit tests use the same dbg-text dimensions as rendering (including bgfx stats fallbacks).
        if (inputState_.leftMouseClicked)
        {
            if (mainMenuSoundSettingsOpen_)
            {
                const int hit = render::Renderer::hitTestPauseSoundMenu(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight);
                switch (hit)
                {
                case 0:
                    mainMenuSoundSettingsOpen_ = false;
                    mainMenuNotice_ = "Sound settings saved.";
                    break;
                case 1:
                    musicVolume_ = std::max(0.0f, musicVolume_ - 0.05f);
                    musicDirector_.setMasterGain(musicVolume_);
                    break;
                case 2:
                    musicVolume_ = std::min(1.0f, musicVolume_ + 0.05f);
                    musicDirector_.setMasterGain(musicVolume_);
                    break;
                case 3:
                    sfxVolume_ = std::max(0.0f, sfxVolume_ - 0.05f);
                    soundEffects_.setMasterGain(sfxVolume_);
                    break;
                case 4:
                    sfxVolume_ = std::min(1.0f, sfxVolume_ + 0.05f);
                    soundEffects_.setMasterGain(sfxVolume_);
                    break;
                default:
                    break;
                }
                saveAudioPrefs();
            }
            else if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::None)
            {
                const int joinSlotsForMpLayout = static_cast<int>(std::min(joinPresets_.size(), std::size_t(3)));
                const int mainMenuContentTopBias = vibecraft::render::detail::mainMenuLogoReservedDbgRows(
                    window_.width(),
                    window_.height(),
                    textHeight,
                    renderer_.menuLogoWidthPx(),
                    renderer_.menuLogoHeightPx());
                const int multiplayerRowShift = render::Renderer::multiplayerMenuRowShift(
                    textHeight,
                    mainMenuMultiplayerPanel_,
                    mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join ? joinSlotsForMpLayout : 0,
                    mainMenuContentTopBias);
                if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Hub)
                {
                    const int hit = render::Renderer::hitTestMainMenuMultiplayerHub(
                        inputState_.mouseWindowX,
                        inputState_.mouseWindowY,
                        window_.width(),
                        window_.height(),
                        textWidth,
                        textHeight,
                        multiplayerRowShift,
                        mainMenuContentTopBias);
                    switch (hit)
                    {
                    case 0:
                        refreshDetectedLanAddress();
                        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Host;
                        mainMenuNotice_.clear();
                        break;
                    case 1:
                        loadJoinPresets();
                        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Join;
                        joinFocusedField_ = 0;
                        window_.setTextInputActive(true);
                        mainMenuNotice_.clear();
                        break;
                    case 2:
                        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
                        mainMenuNotice_.clear();
                        break;
                    default:
                        break;
                    }
                }
                else if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Host)
                {
                    const int hit = render::Renderer::hitTestMainMenuMultiplayerHost(
                        inputState_.mouseWindowX,
                        inputState_.mouseWindowY,
                        window_.width(),
                        window_.height(),
                        textWidth,
                        textHeight,
                        multiplayerRowShift,
                        mainMenuContentTopBias);
                    switch (hit)
                    {
                    case 0:
                        tryStartHostFromMenu();
                        break;
                    case 1:
                        mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                        mainMenuNotice_.clear();
                        break;
                    default:
                        break;
                    }
                }
                else if (mainMenuMultiplayerPanel_ == MainMenuMultiplayerPanel::Join)
                {
                    const int presetSlots = joinSlotsForMpLayout;
                    const int hit = render::Renderer::hitTestMainMenuMultiplayerJoin(
                        inputState_.mouseWindowX,
                        inputState_.mouseWindowY,
                        window_.width(),
                        window_.height(),
                        textWidth,
                        textHeight,
                        presetSlots,
                        multiplayerRowShift,
                        mainMenuContentTopBias);
                    if (hit >= 0 && hit < presetSlots)
                    {
                        applyJoinPreset(joinPresets_[static_cast<std::size_t>(hit)]);
                        tryConnectFromJoinMenu();
                    }
                    else
                    {
                        const int manual = hit - presetSlots;
                        switch (manual)
                        {
                        case 0:
                            joinFocusedField_ = 0;
                            window_.setTextInputActive(true);
                            break;
                        case 1:
                            joinFocusedField_ = 1;
                            window_.setTextInputActive(true);
                            break;
                        case 2:
                            tryConnectFromJoinMenu();
                            break;
                        case 3:
                            mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                            window_.setTextInputActive(false);
                            mainMenuNotice_.clear();
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            else
            {
                const int hit = render::Renderer::hitTestMainMenu(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight,
                    renderer_.menuLogoWidthPx(),
                    renderer_.menuLogoHeightPx());
                switch (hit)
                {
                case 0:
                    pendingHostStartAfterWorldLoad_ = false;
                    beginSingleplayerLoad();
                    break;
                case 1:
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::Hub;
                    mainMenuNotice_.clear();
                    break;
                case 2:
                    creativeModeEnabled_ = !creativeModeEnabled_;
                    mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
                    break;
                case 3:
                    mainMenuSoundSettingsOpen_ = true;
                    mainMenuNotice_.clear();
                    break;
                case 4:
                    inputState_.quitRequested = true;
                    break;
                case 5:
                    creativeModeEnabled_ = !creativeModeEnabled_;
                    mainMenuNotice_ = creativeModeEnabled_ ? "Creative mode enabled." : "Creative mode disabled.";
                    break;
                case 6:
                    spawnPreset_ = nextSpawnPreset(spawnPreset_);
                    mainMenuNotice_ = fmt::format("Spawn preset: {}", spawnPresetLabel(spawnPreset_));
                    break;
                default:
                    break;
                }
            }
        }
        return;
    }

    if (gameScreen_ == GameScreen::Paused)
    {
        const bgfx::Stats* const stats = bgfx::getStats();
        const std::uint16_t textWidth =
            stats != nullptr && stats->textWidth > 0 ? stats->textWidth : 100;
        const std::uint16_t textHeight =
            stats != nullptr && stats->textHeight > 0 ? stats->textHeight : 30;

        if (inputState_.leftMouseClicked)
        {
            if (pauseGameSettingsOpen_)
            {
                const int hit = render::Renderer::hitTestPauseGameSettingsMenu(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight);
                switch (hit)
                {
                case 0:
                    pauseGameSettingsOpen_ = false;
                    pauseMenuNotice_.clear();
                    break;
                case 1:
                    mobSpawningEnabled_ = !mobSpawningEnabled_;
                    if (!mobSpawningEnabled_)
                    {
                        mobSpawnSystem_.clearAllMobs();
                    }
                    pauseMenuNotice_ = mobSpawningEnabled_ ? "Mob spawning enabled." : "Mob spawning disabled.";
                    break;
                case 2:
                {
                    spawnBiomeTarget_ = nextSpawnBiomeTarget(spawnBiomeTarget_);
                    spawnFeetPosition_ = resolveSpawnFeetPosition(
                        world_,
                        terrainGenerator_,
                        spawnPreset_,
                        spawnBiomeTarget_,
                        camera_.position(),
                        kPlayerMovementSettings.standingColliderHeight);
                    playerFeetPosition_ = spawnFeetPosition_;
                    verticalVelocity_ = 0.0f;
                    accumulatedFallDistance_ = 0.0f;
                    jumpWasHeld_ = false;
                    isGrounded_ = isGroundedAtFeetPosition(
                        world_,
                        playerFeetPosition_,
                        kPlayerMovementSettings.standingColliderHeight);
                    camera_.setPosition(
                        playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
                    playerHazards_ = samplePlayerHazards(
                        world_,
                        playerFeetPosition_,
                        kPlayerMovementSettings.standingColliderHeight,
                        kPlayerMovementSettings.standingEyeHeight);
                    pauseMenuNotice_ = fmt::format("Spawn biome: {}", spawnBiomeTargetLabel(spawnBiomeTarget_));
                    break;
                }
                default:
                    break;
                }
            }
            else if (pauseSoundSettingsOpen_)
            {
                const int hit = render::Renderer::hitTestPauseSoundMenu(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight);
                switch (hit)
                {
                case 0:
                    pauseSoundSettingsOpen_ = false;
                    pauseMenuNotice_ = "Sound settings saved.";
                    break;
                default:
                {
                    if (const std::optional<float> sliderValue = render::Renderer::pauseSoundSliderValueFromMouse(
                            inputState_.mouseWindowX,
                            inputState_.mouseWindowY,
                            window_.width(),
                            window_.height(),
                            textWidth,
                            textHeight,
                            true);
                        sliderValue.has_value())
                    {
                        musicVolume_ = std::clamp(*sliderValue, 0.0f, 1.0f);
                        musicDirector_.setMasterGain(musicVolume_);
                    }
                    else if (const std::optional<float> sliderValue = render::Renderer::pauseSoundSliderValueFromMouse(
                                 inputState_.mouseWindowX,
                                 inputState_.mouseWindowY,
                                 window_.width(),
                                 window_.height(),
                                 textWidth,
                                 textHeight,
                                 false);
                             sliderValue.has_value())
                    {
                        sfxVolume_ = std::clamp(*sliderValue, 0.0f, 1.0f);
                        soundEffects_.setMasterGain(sfxVolume_);
                    }
                    break;
                }
                }
                saveAudioPrefs();
            }
            else
            {
                const int hit = render::Renderer::hitTestPauseMenu(
                    inputState_.mouseWindowX,
                    inputState_.mouseWindowY,
                    window_.width(),
                    window_.height(),
                    textWidth,
                    textHeight);
                switch (hit)
                {
                case 0:
                    gameScreen_ = GameScreen::Playing;
                    pauseSoundSettingsOpen_ = false;
                    pauseGameSettingsOpen_ = false;
                    mouseCaptured_ = true;
                    window_.setRelativeMouseMode(true);
                    pauseMenuNotice_.clear();
                    inputState_.clearMouseMotion();
                    break;
                case 1:
                    pauseSoundSettingsOpen_ = true;
                    pauseMenuNotice_.clear();
                    break;
                case 2:
                    stopMultiplayerSessions();
                    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
                    window_.setTextInputActive(false);
                    gameScreen_ = GameScreen::MainMenu;
                    pauseSoundSettingsOpen_ = false;
                    pauseGameSettingsOpen_ = false;
                    mouseCaptured_ = false;
                    window_.setRelativeMouseMode(false);
                    pauseMenuNotice_.clear();
                    singleplayerLoadState_ = {};
                    break;
                case 3:
                    inputState_.quitRequested = true;
                    break;
                case 4:
                    pauseGameSettingsOpen_ = true;
                    pauseMenuNotice_.clear();
                    break;
                default:
                    break;
                }
            }
        }
        return;
    }

    if (gameScreen_ != GameScreen::Playing)
    {
        return;
    }

    if (craftingMenuState_.active)
    {
        mouseCaptured_ = false;
        window_.setRelativeMouseMode(false);
        constexpr std::size_t kBagColumns = 9;
        constexpr std::size_t kVisibleBagRows = 3;
        const std::size_t totalBagRows = bagSlots_.size() / kBagColumns;
        const std::size_t maxBagStartRow =
            totalBagRows > kVisibleBagRows ? totalBagRows - kVisibleBagRows : 0;
        if (inputState_.mouseWheelDeltaY != 0)
        {
            const int scrollDelta = inputState_.mouseWheelDeltaY;
            if (scrollDelta > 0)
            {
                const std::size_t step = static_cast<std::size_t>(scrollDelta);
                craftingMenuState_.bagStartRow = craftingMenuState_.bagStartRow > step
                    ? craftingMenuState_.bagStartRow - step
                    : 0;
            }
            else
            {
                craftingMenuState_.bagStartRow = std::min<std::size_t>(
                    maxBagStartRow,
                    craftingMenuState_.bagStartRow + static_cast<std::size_t>(-scrollDelta));
            }
        }
        if (inputState_.leftMouseClicked)
        {
            handleCraftingMenuClick();
        }
        if (inputState_.rightMousePressed)
        {
            handleCraftingMenuRightClick();
        }
        return;
    }

    heldItemSwing_ = std::max(0.0f, heldItemSwing_ - deltaTimeSeconds * 5.2f);
    if (inputState_.leftMousePressed)
    {
        heldItemSwing_ = 1.0f;
    }

    if (mouseCaptured_)
    {
        camera_.addYawPitch(
            -inputState_.mouseDeltaX * kInputTuning.mouseSensitivity,
            -inputState_.mouseDeltaY * kInputTuning.mouseSensitivity);
    }

    if (inputState_.isKeyDown(SDL_SCANCODE_1))
    {
        selectedHotbarIndex_ = 0;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_2))
    {
        selectedHotbarIndex_ = 1;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_3))
    {
        selectedHotbarIndex_ = 2;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_4))
    {
        selectedHotbarIndex_ = 3;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_5))
    {
        selectedHotbarIndex_ = 4;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_6))
    {
        selectedHotbarIndex_ = 5;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_7))
    {
        selectedHotbarIndex_ = 6;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_8))
    {
        selectedHotbarIndex_ = 7;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_9))
    {
        selectedHotbarIndex_ = 8;
    }
    if (inputState_.mouseWheelDeltaY != 0)
    {
        const int slotCount = static_cast<int>(hotbarSlots_.size());
        int selected = static_cast<int>(selectedHotbarIndex_);
        if (inputState_.mouseWheelDeltaY > 0)
        {
            selected -= inputState_.mouseWheelDeltaY;
        }
        else
        {
            selected += -inputState_.mouseWheelDeltaY;
        }
        selected = ((selected % slotCount) + slotCount) % slotCount;
        selectedHotbarIndex_ = static_cast<std::size_t>(selected);
    }
    if (craftingKeyPressed)
    {
        openCraftingMenu(false);
        return;
    }

    const bool sneaking = inputState_.isKeyDown(SDL_SCANCODE_LSHIFT);
    const bool sprinting = inputState_.isKeyDown(SDL_SCANCODE_LCTRL) && !sneaking;
    const float colliderHeight = sneaking ? kPlayerMovementSettings.sneakingColliderHeight
                                          : kPlayerMovementSettings.standingColliderHeight;
    const float eyeHeight = sneaking ? kPlayerMovementSettings.sneakingEyeHeight
                                     : kPlayerMovementSettings.standingEyeHeight;
    respawnNotice_.clear();

    float currentMoveSpeed = kInputTuning.moveSpeed;
    if (sneaking)
    {
        currentMoveSpeed *= kInputTuning.sneakSpeedMultiplier;
    }
    else if (sprinting)
    {
        currentMoveSpeed *= kInputTuning.sprintSpeedMultiplier;
    }
    const game::EnvironmentalHazards movementHazardsBeforeStep =
        samplePlayerHazards(world_, playerFeetPosition_, colliderHeight, eyeHeight);
    const bool inWaterForMovement = movementHazardsBeforeStep.bodyInWater;
    if (inWaterForMovement)
    {
        currentMoveSpeed *= kPlayerMovementSettings.waterMoveSpeedMultiplier;
    }

    glm::vec3 localMotion(0.0f);
    if (inputState_.isKeyDown(SDL_SCANCODE_W))
    {
        localMotion.z += currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_S))
    {
        localMotion.z -= currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_D))
    {
        localMotion.x -= currentMoveSpeed * deltaTimeSeconds;
    }
    if (inputState_.isKeyDown(SDL_SCANCODE_A))
    {
        localMotion.x += currentMoveSpeed * deltaTimeSeconds;
    }

    glm::vec3 horizontalForward = camera_.forward();
    horizontalForward.y = 0.0f;
    if (glm::dot(horizontalForward, horizontalForward) > 0.0f)
    {
        horizontalForward = glm::normalize(horizontalForward);
    }
    else
    {
        horizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 horizontalRight = camera_.right();
    horizontalRight.y = 0.0f;
    if (glm::dot(horizontalRight, horizontalRight) > 0.0f)
    {
        horizontalRight = glm::normalize(horizontalRight);
    }
    else
    {
        horizontalRight = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // On land: move on the horizontal plane. In water: Minecraft-style swim along look direction (W/S dive and rise).
    glm::vec3 horizontalDisplacement(0.0f);
    float swimVerticalFromLook = 0.0f;
    if (inWaterForMovement)
    {
        glm::vec3 swimForward = camera_.forward();
        if (glm::dot(swimForward, swimForward) > kFloatEpsilon)
        {
            swimForward = glm::normalize(swimForward);
        }
        else
        {
            swimForward = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        glm::vec3 swimRight = camera_.right();
        if (glm::dot(swimRight, swimRight) > kFloatEpsilon)
        {
            swimRight = glm::normalize(swimRight);
        }
        else
        {
            swimRight = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        const glm::vec3 swimWish = swimForward * localMotion.z + swimRight * localMotion.x;
        swimVerticalFromLook = swimWish.y;
        horizontalDisplacement = glm::vec3(swimWish.x, 0.0f, swimWish.z);
    }
    else
    {
        horizontalDisplacement =
            horizontalForward * localMotion.z + horizontalRight * localMotion.x;
    }

    const float horizontalMoveDistance =
        glm::length(glm::vec2(horizontalDisplacement.x, horizontalDisplacement.z));

    if (glm::dot(horizontalDisplacement, horizontalDisplacement) > 0.0f)
    {
        const bool allowStepAssist = isGrounded_ && !inWaterForMovement;
        const AxisMoveResult moveXResult = movePlayerAxisWithCollision(
            world_,
            playerFeetPosition_,
            0,
            horizontalDisplacement.x,
            colliderHeight);
        if (allowStepAssist && moveXResult.blocked)
        {
            const float remainingX = horizontalDisplacement.x - moveXResult.appliedDisplacement;
            static_cast<void>(tryStepUpAfterHorizontalBlock(
                world_,
                playerFeetPosition_,
                0,
                remainingX,
                colliderHeight));
        }

        const AxisMoveResult moveZResult = movePlayerAxisWithCollision(
            world_,
            playerFeetPosition_,
            2,
            horizontalDisplacement.z,
            colliderHeight);
        if (allowStepAssist && moveZResult.blocked)
        {
            const float remainingZ = horizontalDisplacement.z - moveZResult.appliedDisplacement;
            static_cast<void>(tryStepUpAfterHorizontalBlock(
                world_,
                playerFeetPosition_,
                2,
                remainingZ,
                colliderHeight));
        }
    }

    const bool wasGrounded = isGrounded_;
    isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, colliderHeight);

    const bool jumpHeld = inputState_.isKeyDown(SDL_SCANCODE_SPACE);
    if (!inWaterForMovement && jumpHeld && !jumpWasHeld_ && isGrounded_)
    {
        verticalVelocity_ = kPlayerMovementSettings.jumpVelocity;
        isGrounded_ = false;
        soundEffects_.playPlayerJump();
    }
    jumpWasHeld_ = jumpHeld;

    const game::Aabb playerBodyForVines = playerAabbAt(playerFeetPosition_, colliderHeight);
    const bool touchingClimbableVines =
        !inWaterForMovement
        && aabbTouchesBlockType(world_, playerBodyForVines, world::BlockType::Vines);

    if (inWaterForMovement)
    {
        // Buoyancy vs gravity: equal by default → no constant sink (Minecraft-style neutral swim).
        verticalVelocity_ += kPlayerMovementSettings.waterBuoyancyAcceleration * deltaTimeSeconds;
        if (jumpHeld)
        {
            verticalVelocity_ += kPlayerMovementSettings.waterSwimUpAcceleration * deltaTimeSeconds;
        }
        if (sneaking)
        {
            verticalVelocity_ -= kPlayerMovementSettings.waterSinkAcceleration * deltaTimeSeconds;
        }
        verticalVelocity_ -= kPlayerMovementSettings.waterGravity * deltaTimeSeconds;
        verticalVelocity_ = std::clamp(
            verticalVelocity_,
            -kPlayerMovementSettings.waterTerminalFallVelocity,
            kPlayerMovementSettings.waterTerminalRiseVelocity);
        verticalVelocity_ *= std::exp(-kPlayerMovementSettings.waterVerticalDrag * deltaTimeSeconds);
    }
    else if (touchingClimbableVines)
    {
        const float kVine = kPlayerMovementSettings.vineClimbSpeed;
        const float kDesc = kPlayerMovementSettings.vineDescendSpeed;
        if (sneaking)
        {
            verticalVelocity_ = -kDesc;
        }
        else
        {
            // Do not cancel a strong upward impulse from jumping off solid ground.
            const bool canLatchClimbSpeed = verticalVelocity_ <= kVine + 0.55f;
            const bool forwardClimb =
                inputState_.isKeyDown(SDL_SCANCODE_W) && canLatchClimbSpeed;
            const bool jumpClimb =
                jumpHeld && !isGrounded_ && canLatchClimbSpeed;
            if (forwardClimb || jumpClimb)
            {
                verticalVelocity_ = kVine;
            }
            else
            {
                verticalVelocity_ -= kPlayerMovementSettings.gravity
                    * kPlayerMovementSettings.vineIdleFallGravityMultiplier * deltaTimeSeconds;
                verticalVelocity_ = std::max(verticalVelocity_, -kDesc * 1.25f);
            }
        }
    }
    else
    {
        verticalVelocity_ -= kPlayerMovementSettings.gravity * deltaTimeSeconds;
        verticalVelocity_ = std::max(verticalVelocity_, -kPlayerMovementSettings.terminalFallVelocity);
    }

    const glm::vec3 verticalStartPosition = playerFeetPosition_;
    float verticalDisplacement = verticalVelocity_ * deltaTimeSeconds;
    if (inWaterForMovement)
    {
        verticalDisplacement += swimVerticalFromLook;
    }
    const AxisMoveResult verticalMoveResult =
        movePlayerAxisWithCollision(world_, playerFeetPosition_, 1, verticalDisplacement, colliderHeight);
    const bool verticalBlocked = verticalMoveResult.blocked;
    if (verticalBlocked)
    {
        if (verticalVelocity_ < 0.0f)
        {
            isGrounded_ = true;
        }
        verticalVelocity_ = 0.0f;
    }
    else
    {
        isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, colliderHeight);
        if (isGrounded_ && verticalVelocity_ < 0.0f)
        {
            verticalVelocity_ = 0.0f;
        }
    }

    const game::EnvironmentalHazards previousHazards = playerHazards_;
    playerHazards_ = samplePlayerHazards(world_, playerFeetPosition_, colliderHeight, eyeHeight);
    const bool bodyInClimbableVines =
        !playerHazards_.bodyInWater
        && aabbTouchesBlockType(
            world_,
            playerAabbAt(playerFeetPosition_, colliderHeight),
            world::BlockType::Vines);
    if (!previousHazards.bodyInWater && playerHazards_.bodyInWater)
    {
        soundEffects_.playWaterEnter();
    }
    else if (previousHazards.bodyInWater && !playerHazards_.bodyInWater)
    {
        soundEffects_.playWaterExit();
    }
    if (verticalDisplacement < 0.0f && !playerHazards_.bodyInWater && !bodyInClimbableVines)
    {
        accumulatedFallDistance_ += std::max(0.0f, verticalStartPosition.y - playerFeetPosition_.y);
    }

    const bool landedThisFrame = !wasGrounded && isGrounded_;
    bool playerTookDamageThisFrame = false;
    if (landedThisFrame)
    {
        const float landingDistance = accumulatedFallDistance_;
        if (!creativeModeEnabled_)
        {
            const float healthBeforeLanding = playerVitals_.health();
            static_cast<void>(playerVitals_.applyLandingImpact(accumulatedFallDistance_, playerHazards_.bodyInWater));
            if (playerVitals_.health() + 0.001f < healthBeforeLanding)
            {
                playerTookDamageThisFrame = true;
            }
        }
        if (!playerHazards_.bodyInWater)
        {
            const bool hardLanding = landingDistance > 4.2f || playerTookDamageThisFrame;
            soundEffects_.playPlayerLand(hardLanding);
        }
        accumulatedFallDistance_ = 0.0f;
        footstepDistanceAccumulator_ = 0.0f;
    }
    else if (playerHazards_.bodyInWater)
    {
        accumulatedFallDistance_ = 0.0f;
    }
    else if (bodyInClimbableVines)
    {
        accumulatedFallDistance_ = 0.0f;
    }

    if (isGrounded_ && !playerHazards_.bodyInWater && horizontalMoveDistance > 0.0001f)
    {
        const float stepIntervalMeters = sprinting ? 0.31f : 0.42f;
        footstepDistanceAccumulator_ += horizontalMoveDistance;
        while (footstepDistanceAccumulator_ >= stepIntervalMeters)
        {
            footstepDistanceAccumulator_ -= stepIntervalMeters;
            const int bx = static_cast<int>(std::floor(playerFeetPosition_.x));
            const int by = static_cast<int>(std::floor(playerFeetPosition_.y)) - 1;
            const int bz = static_cast<int>(std::floor(playerFeetPosition_.z));
            const world::BlockType groundBlock = world_.blockAt(bx, by, bz);
            if (world::isSolid(groundBlock))
            {
                soundEffects_.playFootstep(groundBlock);
            }
        }
    }
    else if (playerHazards_.bodyInWater)
    {
        const float verticalMoveDistance = std::abs(playerFeetPosition_.y - verticalStartPosition.y);
        const float waterMoveDistance = horizontalMoveDistance + verticalMoveDistance;
        if (waterMoveDistance > 0.0001f)
        {
            constexpr float kSwimStrokeIntervalMeters = 0.9f;
            footstepDistanceAccumulator_ += waterMoveDistance;
            while (footstepDistanceAccumulator_ >= kSwimStrokeIntervalMeters)
            {
                footstepDistanceAccumulator_ -= kSwimStrokeIntervalMeters;
                soundEffects_.playFootstep(world::BlockType::Water);
            }
        }
        else
        {
            footstepDistanceAccumulator_ = 0.0f;
        }
    }
    else if (!isGrounded_)
    {
        footstepDistanceAccumulator_ = 0.0f;
    }

    if (!creativeModeEnabled_)
    {
        const float healthBeforeEnvironmentTick = playerVitals_.health();
        playerVitals_.tickEnvironment(deltaTimeSeconds, playerHazards_);
        if (playerVitals_.health() + 0.001f < healthBeforeEnvironmentTick)
        {
            playerTookDamageThisFrame = true;
        }
    }
    else
    {
        // Creative mode keeps vitals full and ignores environmental damage/air depletion.
        playerVitals_.reset();
    }
    if (playerTookDamageThisFrame)
    {
        soundEffects_.playPlayerHurt();
    }
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, eyeHeight, 0.0f));

    if (!creativeModeEnabled_ && playerVitals_.isDead())
    {
        soundEffects_.playPlayerDeath();
        respawnNotice_ = fmt::format("Respawned after {} damage.", game::damageCauseName(playerVitals_.lastDamageCause()));
        respawnPlayer();
    }

    bool attackedMobThisFrame = false;
    if (inputState_.leftMousePressed)
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (const std::optional<game::MobDamageResult> mobDamage = mobSpawnSystem_.damageClosestAlongRay(
                world_,
                camera_.position(),
                camera_.forward(),
                meleeReachForSlot(selectedSlot),
                meleeDamageForSlot(selectedSlot),
                playerFeetPosition_,
                knockbackDistanceForSlot(selectedSlot));
            mobDamage.has_value())
        {
            attackedMobThisFrame = true;
            activeMiningState_.active = false;
            activeMiningState_.elapsedSeconds = 0.0f;
            soundEffects_.playPlayerAttack();
            if (mobDamage->killed)
            {
                soundEffects_.playMobDefeat(mobDamage->mobKind);
                spawnDroppedItemAtPosition(
                    mobDropItemForKind(mobDamage->mobKind),
                    mobDamage->feetPosition + glm::vec3(0.0f, 0.35f, 0.0f));
            }
            else
            {
                soundEffects_.playMobHit(mobDamage->mobKind);
            }
        }
    }

    const auto raycastHit =
        world_.raycast(camera_.position(), camera_.forward(), kInputTuning.reachDistance);
    if (attackedMobThisFrame)
    {
        return;
    }
    if (!raycastHit.has_value())
    {
        activeMiningState_.active = false;
        activeMiningState_.elapsedSeconds = 0.0f;
        return;
    }

    const std::uint32_t mouseButtons = SDL_GetMouseState(nullptr, nullptr);
    const bool leftMouseHeld = (mouseButtons & SDL_BUTTON_LMASK) != 0U;
    if (!leftMouseHeld)
    {
        activeMiningState_.active = false;
        activeMiningState_.elapsedSeconds = 0.0f;
    }
    else
    {
        const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        const world::BlockType equippedBlockType =
            selectedSlot.count == 0 ? world::BlockType::Air : selectedSlot.blockType;
        const EquippedItem equippedItem = selectedSlot.equippedItem;
        const bool targetChanged = !activeMiningState_.active
            || activeMiningState_.targetBlockPosition != raycastHit->solidBlock
            || activeMiningState_.targetBlockType != raycastHit->blockType
            || activeMiningState_.equippedBlockType != equippedBlockType
            || activeMiningState_.equippedItem != equippedItem;
        if (targetChanged)
        {
            activeMiningState_.active = true;
            activeMiningState_.targetBlockPosition = raycastHit->solidBlock;
            activeMiningState_.targetBlockType = raycastHit->blockType;
            activeMiningState_.equippedBlockType = equippedBlockType;
            activeMiningState_.equippedItem = equippedItem;
            activeMiningState_.elapsedSeconds = 0.0f;
            activeMiningState_.requiredSeconds = creativeModeEnabled_
                ? 0.0f
                : miningDurationSeconds(raycastHit->blockType, equippedBlockType, equippedItem);
            soundEffects_.playBlockDigTick(raycastHit->blockType);
            activeMiningState_.digSoundCooldownSeconds = 0.11f;
        }
        else
        {
            activeMiningState_.digSoundCooldownSeconds -= deltaTimeSeconds;
            if (activeMiningState_.digSoundCooldownSeconds <= 0.0f)
            {
                soundEffects_.playBlockDigTick(raycastHit->blockType);
                activeMiningState_.digSoundCooldownSeconds = 0.11f;
            }
        }
        activeMiningState_.elapsedSeconds += deltaTimeSeconds;

        if (activeMiningState_.elapsedSeconds >= activeMiningState_.requiredSeconds)
        {
            const world::WorldEditCommand command{
                .action = world::WorldEditAction::Remove,
                .position = raycastHit->solidBlock,
                .blockType = world::BlockType::Air,
            };
            if (world_.applyEditCommand(command))
            {
                if (raycastHit->blockType == world::BlockType::Chest)
                {
                    const auto chestIt = chestSlotsByPosition_.find(chestStorageKey(raycastHit->solidBlock));
                    if (chestIt != chestSlotsByPosition_.end())
                    {
                        for (const InventorySlot& slot : chestIt->second)
                        {
                            for (std::uint32_t i = 0; i < slot.count; ++i)
                            {
                                if (!creativeModeEnabled_ && slot.equippedItem != EquippedItem::None)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.equippedItem,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                                else if (!creativeModeEnabled_ && slot.blockType != world::BlockType::Air)
                                {
                                    spawnDroppedItemAtPosition(
                                        slot.blockType,
                                        glm::vec3(raycastHit->solidBlock) + glm::vec3(0.5f, 0.4f, 0.5f));
                                }
                            }
                        }
                        chestSlotsByPosition_.erase(chestIt);
                    }
                }
                if (!creativeModeEnabled_)
                {
                    spawnDroppedItem(raycastHit->blockType, raycastHit->solidBlock);
                }
                soundEffects_.playBlockBreak(raycastHit->blockType);
                if (hostSession_ != nullptr && hostSession_->running())
                {
                    hostSession_->broadcastBlockEdit({
                        .authorClientId = localClientId_,
                        .action = command.action,
                        .x = command.position.x,
                        .y = command.position.y,
                        .z = command.position.z,
                        .blockType = command.blockType,
                    });
                }
                if (clientSession_ != nullptr && clientSession_->connected())
                {
                    clientSession_->sendInput(
                        {
                            .clientId = clientSession_->clientId(),
                            .positionX = playerFeetPosition_.x,
                            .positionY = playerFeetPosition_.y,
                            .positionZ = playerFeetPosition_.z,
                            .yawDelta = camera_.yawDegrees(),
                            .pitchDelta = camera_.pitchDegrees(),
                            .health = playerVitals_.health(),
                            .air = playerVitals_.air(),
                        .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                        .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                            .breakBlock = true,
                            .targetX = command.position.x,
                            .targetY = command.position.y,
                            .targetZ = command.position.z,
                        },
                        networkServerTick_);
                }
            }
            activeMiningState_.active = false;
            activeMiningState_.elapsedSeconds = 0.0f;
        }
    }

    if (inputState_.rightMousePressed)
    {
        if (raycastHit->blockType == world::BlockType::CraftingTable)
        {
            openCraftingMenu(true, raycastHit->solidBlock);
            return;
        }
        if (raycastHit->blockType == world::BlockType::Chest)
        {
            openChestMenu(raycastHit->solidBlock);
            return;
        }

        InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
        if (selectedSlot.count == 0 || selectedSlot.blockType == world::BlockType::Air
            || selectedSlot.equippedItem != EquippedItem::None)
        {
            return;
        }

        const world::WorldEditCommand command{
            .action = world::WorldEditAction::Place,
            .position = raycastHit->buildTarget,
            .blockType = selectedSlot.blockType,
        };
        if (world_.applyEditCommand(command))
        {
            if (!creativeModeEnabled_)
            {
                consumeSelectedHotbarSlot(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
            }
            soundEffects_.playBlockPlace(command.blockType);
            if (hostSession_ != nullptr && hostSession_->running())
            {
                hostSession_->broadcastBlockEdit({
                    .authorClientId = localClientId_,
                    .action = command.action,
                    .x = command.position.x,
                    .y = command.position.y,
                    .z = command.position.z,
                    .blockType = command.blockType,
                });
            }
            if (clientSession_ != nullptr && clientSession_->connected())
            {
                clientSession_->sendInput(
                    {
                        .clientId = clientSession_->clientId(),
                        .positionX = playerFeetPosition_.x,
                        .positionY = playerFeetPosition_.y,
                        .positionZ = playerFeetPosition_.z,
                        .yawDelta = camera_.yawDegrees(),
                        .pitchDelta = camera_.pitchDegrees(),
                        .health = playerVitals_.health(),
                        .air = playerVitals_.air(),
                        .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                        .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                        .placeBlock = true,
                        .targetX = command.position.x,
                        .targetY = command.position.y,
                        .targetZ = command.position.z,
                        .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                        .placeBlockType = command.blockType,
                    },
                    networkServerTick_);
            }
        }
    }
}

void Application::spawnDroppedItem(
    const world::BlockType blockType,
    const glm::ivec3& blockPosition)
{
    if (blockType == world::BlockType::Air || blockType == world::BlockType::Water
        || blockType == world::BlockType::Lava)
    {
        return;
    }

    if (blockType == world::BlockType::CoalOre)
    {
        spawnDroppedItemAtPosition(
            EquippedItem::Coal,
            glm::vec3(
                static_cast<float>(blockPosition.x) + 0.5f,
                static_cast<float>(blockPosition.y) + 0.2f,
                static_cast<float>(blockPosition.z) + 0.5f));
        return;
    }

    spawnDroppedItemAtPosition(
        blockType,
        glm::vec3(
            static_cast<float>(blockPosition.x) + 0.5f,
            static_cast<float>(blockPosition.y) + 0.2f,
            static_cast<float>(blockPosition.z) + 0.5f));
}

void Application::spawnDroppedItemAtPosition(
    const world::BlockType blockType,
    const glm::vec3& worldPosition)
{
    if (blockType == world::BlockType::Air || blockType == world::BlockType::Water
        || blockType == world::BlockType::Lava)
    {
        return;
    }

    const float seed = worldPosition.x * 0.73f + worldPosition.z * 1.17f + worldPosition.y * 0.41f;
    droppedItems_.push_back(DroppedItem{
        .blockType = blockType,
        .equippedItem = EquippedItem::None,
        .worldPosition = worldPosition,
        .velocity = glm::vec3(
            std::sin(seed) * 1.05f,
            2.0f,
            std::cos(seed * 1.37f) * 1.05f),
        .ageSeconds = 0.0f,
        .pickupDelaySeconds = 0.2f,
        .spinRadians = 0.0f,
    });
}

void Application::spawnDroppedItemAtPosition(
    const EquippedItem equippedItem,
    const glm::vec3& worldPosition)
{
    if (equippedItem == EquippedItem::None)
    {
        return;
    }

    const float seed = worldPosition.x * 0.73f + worldPosition.z * 1.17f + worldPosition.y * 0.41f;
    droppedItems_.push_back(DroppedItem{
        .blockType = world::BlockType::Air,
        .equippedItem = equippedItem,
        .worldPosition = worldPosition,
        .velocity = glm::vec3(
            std::sin(seed) * 1.05f,
            2.0f,
            std::cos(seed * 1.37f) * 1.05f),
        .ageSeconds = 0.0f,
        .pickupDelaySeconds = 0.2f,
        .spinRadians = 0.0f,
    });
}

void Application::updateDroppedItems(const float deltaTimeSeconds, const float eyeHeight)
{
    const glm::vec3 pickupCenter = playerFeetPosition_ + glm::vec3(0.0f, eyeHeight * 0.6f, 0.0f);
    constexpr float kPickupRadius = 1.25f;
    constexpr float kPickupRadiusSq = kPickupRadius * kPickupRadius;
    constexpr float kMagnetRadius = 3.8f;
    constexpr float kMagnetRadiusSq = kMagnetRadius * kMagnetRadius;
    constexpr float kGravity = 16.0f;
    constexpr float kTau = 6.28318530718f;

    std::size_t itemIndex = 0;
    while (itemIndex < droppedItems_.size())
    {
        DroppedItem& droppedItem = droppedItems_[itemIndex];
        droppedItem.ageSeconds += deltaTimeSeconds;
        droppedItem.pickupDelaySeconds =
            std::max(0.0f, droppedItem.pickupDelaySeconds - deltaTimeSeconds);
        droppedItem.spinRadians = std::fmod(
            droppedItem.spinRadians + deltaTimeSeconds * 4.2f,
            kTau);

        droppedItem.velocity.y -= kGravity * deltaTimeSeconds;
        droppedItem.velocity *= std::pow(0.96f, deltaTimeSeconds * 60.0f);

        const glm::vec3 toPlayer = pickupCenter - droppedItem.worldPosition;
        const float distanceToPlayerSq = glm::dot(toPlayer, toPlayer);
        if (droppedItem.pickupDelaySeconds <= 0.0f && distanceToPlayerSq <= kMagnetRadiusSq)
        {
            const float distanceToPlayer = std::sqrt(std::max(distanceToPlayerSq, 0.0001f));
            const glm::vec3 pullDirection = toPlayer / distanceToPlayer;
            const float pullStrength = 8.0f + (1.0f - distanceToPlayer / kMagnetRadius) * 18.0f;
            droppedItem.velocity += pullDirection * pullStrength * deltaTimeSeconds;
        }

        droppedItem.worldPosition += droppedItem.velocity * deltaTimeSeconds;

        const int belowX = static_cast<int>(std::floor(droppedItem.worldPosition.x));
        const int belowY = static_cast<int>(std::floor(droppedItem.worldPosition.y - 0.36f));
        const int belowZ = static_cast<int>(std::floor(droppedItem.worldPosition.z));
        if (world::isSolid(world_.blockAt(belowX, belowY, belowZ)) && droppedItem.velocity.y < 0.0f)
        {
            droppedItem.worldPosition.y = static_cast<float>(belowY + 1) + 0.36f;
            droppedItem.velocity.y = 0.0f;
        }

        const glm::vec3 delta = droppedItem.worldPosition - pickupCenter;
        if (droppedItem.pickupDelaySeconds <= 0.0f && glm::dot(delta, delta) <= kPickupRadiusSq)
        {
            bool pickedUp = false;
            if (droppedItem.equippedItem != EquippedItem::None)
            {
                pickedUp = addEquippedItemToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    droppedItem.equippedItem,
                    selectedHotbarIndex_);
            }
            else
            {
                pickedUp = addBlockToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    droppedItem.blockType,
                    selectedHotbarIndex_);
            }

            if (pickedUp)
            {
                droppedItems_.erase(droppedItems_.begin() + static_cast<std::ptrdiff_t>(itemIndex));
                continue;
            }
        }

        ++itemIndex;
    }
}

void Application::respawnPlayer()
{
    playerFeetPosition_ = spawnFeetPosition_;
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    isGrounded_ =
        isGroundedAtFeetPosition(world_, playerFeetPosition_, kPlayerMovementSettings.standingColliderHeight);
    playerVitals_.reset();
    playerHazards_ = samplePlayerHazards(
        world_,
        playerFeetPosition_,
        kPlayerMovementSettings.standingColliderHeight,
        kPlayerMovementSettings.standingEyeHeight);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
}

void Application::openCraftingMenu(
    const bool useWorkbench,
    const glm::ivec3& workbenchBlockPosition)
{
    if (!craftingMenuState_.active)
    {
        craftingMenuState_ = CraftingMenuState{};
    }
    craftingMenuState_.active = true;
    craftingMenuState_.mode = useWorkbench
        ? CraftingMenuState::Mode::WorkbenchCrafting
        : CraftingMenuState::Mode::InventoryCrafting;
    craftingMenuState_.usesWorkbench = useWorkbench;
    craftingMenuState_.workbenchBlockPosition = workbenchBlockPosition;
    craftingMenuState_.chestBlockPosition = glm::ivec3(0);
    craftingMenuState_.bagStartRow = 0;
    craftingMenuState_.hint = useWorkbench
        ? "3x3 workbench crafting: left-click move, right-click split/place one."
        : "2x2 inventory crafting: logs -> planks, planks -> sticks/table.";
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();
}

void Application::openChestMenu(const glm::ivec3& chestBlockPosition)
{
    if (!craftingMenuState_.active)
    {
        craftingMenuState_ = CraftingMenuState{};
    }
    craftingMenuState_.active = true;
    craftingMenuState_.mode = CraftingMenuState::Mode::ChestStorage;
    craftingMenuState_.usesWorkbench = true;
    craftingMenuState_.workbenchBlockPosition = glm::ivec3(0);
    craftingMenuState_.chestBlockPosition = chestBlockPosition;
    craftingMenuState_.bagStartRow = 0;
    craftingMenuState_.hint = "Chest storage: move stacks between chest and inventory.";
    craftingMenuState_.gridSlots = chestSlotsByPosition_[chestStorageKey(chestBlockPosition)];
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();
}

void Application::returnCraftingSlotsToInventory()
{
    const auto tryInsertSlot = [&](InventorySlot& slot)
    {
        if (isInventorySlotEmpty(slot))
        {
            return true;
        }

        const auto tryMergeInto = [&](auto& slots) -> bool
        {
            for (InventorySlot& existingSlot : slots)
            {
                if (canMergeInventorySlots(existingSlot, slot) && existingSlot.count < kMaxStackSize)
                {
                    const std::uint32_t transfer = std::min(kMaxStackSize - existingSlot.count, slot.count);
                    existingSlot.count += transfer;
                    slot.count -= transfer;
                    if (slot.count == 0)
                    {
                        return true;
                    }
                }
            }
            for (InventorySlot& existingSlot : slots)
            {
                if (isInventorySlotEmpty(existingSlot))
                {
                    existingSlot = slot;
                    clearInventorySlot(slot);
                    return true;
                }
            }
            return isInventorySlotEmpty(slot);
        };

        if (tryMergeInto(hotbarSlots_) && isInventorySlotEmpty(slot))
        {
            return true;
        }
        if (tryMergeInto(bagSlots_) && isInventorySlotEmpty(slot))
        {
            return true;
        }
        return isInventorySlotEmpty(slot);
    };

    for (InventorySlot& slot : craftingMenuState_.gridSlots)
    {
        while (!isInventorySlotEmpty(slot))
        {
            if (tryInsertSlot(slot))
            {
                break;
            }
            else
            {
                if (slot.equippedItem != EquippedItem::None)
                {
                    spawnDroppedItemAtPosition(
                        slot.equippedItem,
                        playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
                }
                else
                {
                    spawnDroppedItemAtPosition(
                        slot.blockType,
                        playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
                }
                --slot.count;
                if (slot.count == 0)
                {
                    clearInventorySlot(slot);
                }
            }
        }
    }

    while (!isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        if (tryInsertSlot(craftingMenuState_.carriedSlot))
        {
            break;
        }
        else
        {
            if (craftingMenuState_.carriedSlot.equippedItem != EquippedItem::None)
            {
                spawnDroppedItemAtPosition(
                    craftingMenuState_.carriedSlot.equippedItem,
                    playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
            }
            else if (craftingMenuState_.carriedSlot.blockType != world::BlockType::Air)
            {
                spawnDroppedItemAtPosition(
                    craftingMenuState_.carriedSlot.blockType,
                    playerFeetPosition_ + glm::vec3(0.0f, 1.0f, 0.0f));
            }
            clearInventorySlot(craftingMenuState_.carriedSlot);
        }
    }
}

void Application::closeCraftingMenu()
{
    if (craftingMenuState_.active && craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage)
    {
        const std::int64_t key = chestStorageKey(craftingMenuState_.chestBlockPosition);
        bool hasAnyItem = false;
        for (const InventorySlot& slot : craftingMenuState_.gridSlots)
        {
            if (!isInventorySlotEmpty(slot))
            {
                hasAnyItem = true;
                break;
            }
        }
        if (hasAnyItem)
        {
            chestSlotsByPosition_[key] = craftingMenuState_.gridSlots;
        }
        else
        {
            chestSlotsByPosition_.erase(key);
        }
    }
    returnCraftingSlotsToInventory();
    craftingMenuState_ = CraftingMenuState{};
    mouseCaptured_ = true;
    window_.setRelativeMouseMode(true);
    inputState_.clearMouseMotion();
}

void Application::handleCraftingMenuClick()
{
    const bool chestMode = craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage;
    const int hit = render::Renderer::hitTestCraftingMenu(
        inputState_.mouseWindowX,
        inputState_.mouseWindowY,
        window_.width(),
        window_.height(),
        craftingMenuState_.usesWorkbench,
        craftingMenuState_.bagStartRow);
    if (hit < 0)
    {
        return;
    }

    if (!chestMode && hit == render::Renderer::kCraftingResultHit)
    {
        const std::optional<CraftingMatch> craftingMatch = evaluateCraftingGrid(
            craftingMenuState_.gridSlots,
            craftingMenuState_.usesWorkbench ? CraftingMode::Workbench3x3 : CraftingMode::Inventory2x2);
        if (!craftingMatch.has_value() || !canReceiveCraftingOutput(craftingMenuState_.carriedSlot, craftingMatch->output))
        {
            return;
        }
        if (isInventorySlotEmpty(craftingMenuState_.carriedSlot))
        {
            craftingMenuState_.carriedSlot = craftingMatch->output;
        }
        else
        {
            craftingMenuState_.carriedSlot.count += craftingMatch->output.count;
        }
        consumeCraftingIngredients(craftingMenuState_.gridSlots, craftingMatch.value());
        soundEffects_.playBlockPlace(craftingMatch->output.blockType);
        return;
    }
    if (chestMode && hit == render::Renderer::kCraftingResultHit)
    {
        return;
    }

    InventorySlot* targetSlot = nullptr;
    bool isCraftingGridSlot = false;
    if (hit >= render::Renderer::kCraftingGridHitBase && hit < render::Renderer::kCraftingGridHitBase + 9)
    {
        targetSlot = &craftingMenuState_.gridSlots[static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase)];
        isCraftingGridSlot = true;
    }
    else if (hit >= render::Renderer::kCraftingHotbarHitBase
             && hit < render::Renderer::kCraftingHotbarHitBase + static_cast<int>(hotbarSlots_.size()))
    {
        targetSlot = &hotbarSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingHotbarHitBase)];
    }
    else if (hit >= render::Renderer::kCraftingBagHitBase
             && hit < render::Renderer::kCraftingBagHitBase + static_cast<int>(bagSlots_.size()))
    {
        targetSlot = &bagSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingBagHitBase)];
    }

    if (targetSlot == nullptr)
    {
        return;
    }

    if (!chestMode && isCraftingGridSlot && !canPlaceIntoCraftingGrid(craftingMenuState_.carriedSlot)
        && !isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        return;
    }

    mergeOrSwapInventorySlot(
        craftingMenuState_.carriedSlot,
        *targetSlot,
        true);
}

void Application::handleCraftingMenuRightClick()
{
    const bool chestMode = craftingMenuState_.mode == CraftingMenuState::Mode::ChestStorage;
    const int hit = render::Renderer::hitTestCraftingMenu(
        inputState_.mouseWindowX,
        inputState_.mouseWindowY,
        window_.width(),
        window_.height(),
        craftingMenuState_.usesWorkbench,
        craftingMenuState_.bagStartRow);
    if (hit < 0 || hit == render::Renderer::kCraftingResultHit)
    {
        return;
    }

    InventorySlot* targetSlot = nullptr;
    bool isCraftingGridSlot = false;
    if (hit >= render::Renderer::kCraftingGridHitBase && hit < render::Renderer::kCraftingGridHitBase + 9)
    {
        targetSlot = &craftingMenuState_.gridSlots[static_cast<std::size_t>(hit - render::Renderer::kCraftingGridHitBase)];
        isCraftingGridSlot = true;
    }
    else if (hit >= render::Renderer::kCraftingHotbarHitBase
             && hit < render::Renderer::kCraftingHotbarHitBase + static_cast<int>(hotbarSlots_.size()))
    {
        targetSlot = &hotbarSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingHotbarHitBase)];
    }
    else if (hit >= render::Renderer::kCraftingBagHitBase
             && hit < render::Renderer::kCraftingBagHitBase + static_cast<int>(bagSlots_.size()))
    {
        targetSlot = &bagSlots_[static_cast<std::size_t>(hit - render::Renderer::kCraftingBagHitBase)];
    }

    if (targetSlot == nullptr)
    {
        return;
    }

    if (!chestMode && isCraftingGridSlot && !canPlaceIntoCraftingGrid(craftingMenuState_.carriedSlot)
        && !isInventorySlotEmpty(craftingMenuState_.carriedSlot))
    {
        return;
    }

    rightClickInventorySlot(
        craftingMenuState_.carriedSlot,
        *targetSlot,
        true);
}

bool Application::startHostSession()
{
    stopMultiplayerSessions();

    auto hostSession = std::make_unique<multiplayer::HostSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!hostSession->start(multiplayerPort_))
    {
        multiplayerStatusLine_ = "host failed: " + hostSession->lastError();
        return false;
    }

    hostSession_ = std::move(hostSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Host;
    localClientId_ = 0;
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("hosting on :{}", multiplayerPort_);
    return true;
}

bool Application::startClientSession(const std::string& address)
{
    stopMultiplayerSessions();

    auto clientSession = std::make_unique<multiplayer::ClientSession>(std::make_unique<multiplayer::UdpTransport>());
    if (!clientSession->connect(address, multiplayerPort_, "Player"))
    {
        multiplayerStatusLine_ = "join failed: " + clientSession->lastError();
        return false;
    }

    clientSession_ = std::move(clientSession);
    multiplayerMode_ = MultiplayerRuntimeMode::Client;
    localClientId_ = 0;
    remotePlayers_.clear();
    multiplayerStatusLine_ = fmt::format("connecting {}:{}", address, multiplayerPort_);
    return true;
}

void Application::stopMultiplayerSessions()
{
    if (clientSession_ != nullptr)
    {
        clientSession_->disconnect();
        clientSession_.reset();
    }
    if (hostSession_ != nullptr)
    {
        hostSession_->shutdown();
        hostSession_.reset();
    }
    multiplayerMode_ = MultiplayerRuntimeMode::SinglePlayer;
    localClientId_ = 0;
    remotePlayers_.clear();
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
}

std::filesystem::path Application::multiplayerPrefsPath() const
{
    std::filesystem::path directory = savePath_.parent_path();
    if (directory.empty())
    {
        directory = "assets/saves";
    }
    return directory / "multiplayer_prefs.txt";
}

std::filesystem::path Application::joinPresetsPath() const
{
    std::filesystem::path directory = savePath_.parent_path();
    if (directory.empty())
    {
        directory = "assets/saves";
    }
    return directory / "join_presets.txt";
}

void Application::applyJoinPreset(const JoinPresetEntry& preset)
{
    joinAddressInput_ = preset.host;
    joinPortInput_ = std::to_string(preset.port);
    multiplayerPort_ = preset.port;
    multiplayerAddress_ = preset.host;
}

void Application::loadJoinPresets()
{
    joinPresets_.clear();
    std::ifstream input(joinPresetsPath());
    if (input.is_open())
    {
        std::string line;
        while (std::getline(input, line) && joinPresets_.size() < 3)
        {
            trimInPlace(line);
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            const std::size_t p1 = line.find('|');
            if (p1 == std::string::npos)
            {
                continue;
            }
            const std::size_t p2 = line.find('|', p1 + 1);
            if (p2 == std::string::npos || p2 <= p1)
            {
                continue;
            }
            std::string label = trimCopy(line.substr(0, p1));
            std::string host = trimCopy(line.substr(p1 + 1, p2 - p1 - 1));
            std::string portStr = trimCopy(line.substr(p2 + 1));
            if (label.empty() || host.empty())
            {
                continue;
            }
            unsigned long port = 41234;
            try
            {
                port = std::stoul(portStr);
            }
            catch (...)
            {
                continue;
            }
            if (port > 65535UL)
            {
                continue;
            }
            joinPresets_.push_back(JoinPresetEntry{
                .label = std::move(label),
                .host = std::move(host),
                .port = static_cast<std::uint16_t>(port),
            });
        }
    }
    if (joinPresets_.empty())
    {
        joinPresets_.push_back(
            JoinPresetEntry{.label = "This PC (local)", .host = "127.0.0.1", .port = 41234});
    }
}

void Application::loadMultiplayerPrefs()
{
    joinAddressInput_ = resolveJoinAddressFromEnvironment();
    joinPortInput_ = "41234";
    std::ifstream input(multiplayerPrefsPath());
    if (!input.is_open())
    {
        multiplayerAddress_ = joinAddressInput_;
        loadJoinPresets();
        return;
    }

    std::string addressLine;
    std::string portLine;
    std::getline(input, addressLine);
    std::getline(input, portLine);
    trimInPlace(addressLine);
    trimInPlace(portLine);
    if (!addressLine.empty())
    {
        joinAddressInput_ = addressLine;
    }
    if (!portLine.empty())
    {
        joinPortInput_ = portLine;
    }
    multiplayerAddress_ = joinAddressInput_;
    try
    {
        if (!portLine.empty())
        {
            const unsigned long parsedPort = std::stoul(portLine);
            if (parsedPort <= 65535UL)
            {
                multiplayerPort_ = static_cast<std::uint16_t>(parsedPort);
            }
        }
    }
    catch (...)
    {
    }
    loadJoinPresets();
}

void Application::saveMultiplayerPrefs() const
{
    std::error_code errorCode;
    std::filesystem::create_directories(multiplayerPrefsPath().parent_path(), errorCode);
    std::ofstream output(multiplayerPrefsPath(), std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }
    output << joinAddressInput_ << '\n' << joinPortInput_ << '\n';
}

std::filesystem::path Application::audioPrefsPath() const
{
    std::filesystem::path directory = savePath_.parent_path();
    if (directory.empty())
    {
        directory = "assets/saves";
    }
    return directory / "audio_prefs.txt";
}

void Application::loadAudioPrefs()
{
    std::ifstream input(audioPrefsPath());
    if (!input.is_open())
    {
        return;
    }
    std::string musicLine;
    std::string sfxLine;
    std::getline(input, musicLine);
    std::getline(input, sfxLine);
    trimInPlace(musicLine);
    trimInPlace(sfxLine);
    try
    {
        if (!musicLine.empty())
        {
            musicVolume_ = std::clamp(std::stof(musicLine), 0.0f, 1.0f);
        }
        if (!sfxLine.empty())
        {
            sfxVolume_ = std::clamp(std::stof(sfxLine), 0.0f, 1.0f);
        }
    }
    catch (...)
    {
    }
}

void Application::saveAudioPrefs() const
{
    std::error_code errorCode;
    std::filesystem::create_directories(audioPrefsPath().parent_path(), errorCode);
    std::ofstream output(audioPrefsPath(), std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }
    output << fmt::format("{:.4f}\n{:.4f}\n", musicVolume_, sfxVolume_);
}

void Application::refreshDetectedLanAddress()
{
    detectedLanAddress_ = platform::primaryLanIPv4String();
}

void Application::processJoinMenuTextInput()
{
    if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::Join)
    {
        return;
    }

    if (inputState_.tabPressed)
    {
        joinFocusedField_ = 1 - joinFocusedField_;
    }

    if (inputState_.backspacePressed)
    {
        if (joinFocusedField_ == 0)
        {
            if (!joinAddressInput_.empty())
            {
                joinAddressInput_.pop_back();
            }
        }
        else if (!joinPortInput_.empty())
        {
            joinPortInput_.pop_back();
        }
    }

    if (!inputState_.textInputUtf8.empty())
    {
        if (joinFocusedField_ == 0)
        {
            joinAddressInput_ += inputState_.textInputUtf8;
        }
        else
        {
            for (const char character : inputState_.textInputUtf8)
            {
                if (character >= '0' && character <= '9')
                {
                    joinPortInput_ += character;
                }
            }
        }
    }
}

void Application::tryStartHostFromMenu()
{
    refreshDetectedLanAddress();
    pendingHostStartAfterWorldLoad_ = true;
    beginSingleplayerLoad();
}

void Application::tryConnectFromJoinMenu()
{
    const std::string host = trimCopy(joinAddressInput_);
    if (host.empty())
    {
        mainMenuNotice_ = "Enter the host address (e.g. 192.168.1.5).";
        return;
    }

    unsigned long parsedPort = 41234;
    try
    {
        if (!joinPortInput_.empty())
        {
            parsedPort = std::stoul(joinPortInput_);
        }
    }
    catch (...)
    {
        mainMenuNotice_ = "Port must be a number (default 41234).";
        return;
    }

    if (parsedPort > 65535UL)
    {
        mainMenuNotice_ = "Port must be between 0 and 65535.";
        return;
    }

    multiplayerPort_ = static_cast<std::uint16_t>(parsedPort);
    multiplayerAddress_ = host;
    if (!startClientSession(host))
    {
        mainMenuNotice_ = "Could not connect. Check address, firewall, and that the host is running.";
        return;
    }

    clearClientWorldAwaitingHostChunks();
    beginClientJoinLoad();

    saveMultiplayerPrefs();
}

void Application::beginClientJoinLoad()
{
    if (gameScreen_ != GameScreen::MainMenu || singleplayerLoadState_.active)
    {
        return;
    }

    clientJoinLoadDebugFrame_ = 0;
    clientJoinLoggedFirstChunkSummary_ = false;
    clientJoinAuthoritativeSnapLogsRemaining_ = 8;
    logMultiplayerJoinDiag(
        "beginClientJoinLoad -> {}:{} (local world cleared, awaiting snapshots + authoritative spawn)",
        multiplayerAddress_,
        multiplayerPort_);

    pendingHostStartAfterWorldLoad_ = false;
    pendingClientJoinAfterWorldLoad_ = true;
    singleplayerLoadState_.active = true;
    singleplayerLoadState_.worldPrepared = false;
    singleplayerLoadState_.progress = 0.02f;
    singleplayerLoadState_.label = "Connecting to host...";

    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
    mainMenuSoundSettingsOpen_ = false;
    mainMenuNotice_.clear();
    window_.setTextInputActive(false);
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();
}

void Application::sendInitialWorldToClient(const std::uint16_t clientId)
{
    if (hostSession_ == nullptr)
    {
        return;
    }

    for (const auto& [coord, chunk] : world_.chunks())
    {
        multiplayer::protocol::ChunkSnapshotMessage snapshot{
            .coord = coord,
        };
        const auto& blockStorage = chunk.blockStorage();
        for (std::size_t i = 0; i < blockStorage.size(); ++i)
        {
            snapshot.blocks[i] = static_cast<std::uint8_t>(blockStorage[i]);
        }
        hostSession_->sendChunkSnapshot(clientId, snapshot);
    }
}

void Application::applyChunkSnapshot(const multiplayer::protocol::ChunkSnapshotMessage& chunkMessage)
{
    world::Chunk chunk(chunkMessage.coord);
    auto& storage = chunk.mutableBlockStorage();
    for (std::size_t i = 0; i < storage.size(); ++i)
    {
        storage[i] = static_cast<world::BlockType>(chunkMessage.blocks[i]);
    }
    world_.replaceChunk(std::move(chunk));
}

void Application::applyRemoteBlockEdit(const multiplayer::protocol::BlockEditEventMessage& editMessage)
{
    static_cast<void>(world_.applyEditCommand({
        .action = editMessage.action,
        .position = {editMessage.x, editMessage.y, editMessage.z},
        .blockType = editMessage.blockType,
    }));
}

multiplayer::protocol::ServerSnapshotMessage Application::buildServerSnapshot() const
{
    multiplayer::protocol::ServerSnapshotMessage snapshot;
    snapshot.serverTick = networkServerTick_;
    snapshot.dayNightElapsedSeconds = dayNightCycle_.elapsedSeconds();
    snapshot.weatherElapsedSeconds = weatherSystem_.elapsedSeconds();
    snapshot.players.reserve(remotePlayers_.size() + 1);
    const InventorySlot& selectedSlot = hotbarSlots_[selectedHotbarIndex_];
    snapshot.players.push_back(multiplayer::protocol::PlayerSnapshotMessage{
        .clientId = localClientId_,
        .posX = playerFeetPosition_.x,
        .posY = playerFeetPosition_.y,
        .posZ = playerFeetPosition_.z,
        .yawDegrees = camera_.yawDegrees(),
        .pitchDegrees = camera_.pitchDegrees(),
        .health = playerVitals_.health(),
        .air = playerVitals_.air(),
        .selectedEquippedItem = selectedSlot.equippedItem,
        .selectedBlockType = selectedSlot.blockType,
    });
    for (const RemotePlayerState& remote : remotePlayers_)
    {
        snapshot.players.push_back(multiplayer::protocol::PlayerSnapshotMessage{
            .clientId = remote.clientId,
            .posX = remote.position.x,
            .posY = remote.position.y,
            .posZ = remote.position.z,
            .yawDegrees = remote.yawDegrees,
            .pitchDegrees = remote.pitchDegrees,
            .health = remote.health,
            .air = remote.air,
            .selectedEquippedItem = remote.selectedEquippedItem,
            .selectedBlockType = remote.selectedBlockType,
        });
    }
    snapshot.droppedItems.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        snapshot.droppedItems.push_back(multiplayer::protocol::DroppedItemSnapshotMessage{
            .blockType = droppedItem.equippedItem != EquippedItem::None
                ? networkFallbackBlockTypeForEquippedItem(droppedItem.equippedItem)
                : droppedItem.blockType,
            .posX = droppedItem.worldPosition.x,
            .posY = droppedItem.worldPosition.y,
            .posZ = droppedItem.worldPosition.z,
            .velocityX = droppedItem.velocity.x,
            .velocityY = droppedItem.velocity.y,
            .velocityZ = droppedItem.velocity.z,
            .ageSeconds = droppedItem.ageSeconds,
            .spinRadians = droppedItem.spinRadians,
        });
    }
    return snapshot;
}

void Application::updateMultiplayer(const float deltaTimeSeconds)
{
    networkTickAccumulatorSeconds_ += deltaTimeSeconds;

    if (hostSession_ != nullptr && hostSession_->running())
    {
        hostSession_->poll();
        std::unordered_set<std::uint16_t> activeClientIds;
        activeClientIds.reserve(hostSession_->clients().size());
        const auto rebuildChunkSyncList = [this](const std::uint16_t clientId, const world::ChunkCoord& centerChunk)
        {
            std::vector<world::ChunkCoord> coords;
            const int radius = kStreamingSettings.generationChunkRadius;
            coords.reserve(static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1)));
            for (int chunkZ = centerChunk.z - radius; chunkZ <= centerChunk.z + radius; ++chunkZ)
            {
                for (int chunkX = centerChunk.x - radius; chunkX <= centerChunk.x + radius; ++chunkX)
                {
                    const world::ChunkCoord coord{chunkX, chunkZ};
                    if (world_.chunks().contains(coord))
                    {
                        coords.push_back(coord);
                    }
                }
            }
            std::sort(
                coords.begin(),
                coords.end(),
                [&centerChunk](const world::ChunkCoord& lhs, const world::ChunkCoord& rhs)
                {
                    const int lhsDx = lhs.x - centerChunk.x;
                    const int lhsDz = lhs.z - centerChunk.z;
                    const int rhsDx = rhs.x - centerChunk.x;
                    const int rhsDz = rhs.z - centerChunk.z;
                    const int lhsDistanceSq = lhsDx * lhsDx + lhsDz * lhsDz;
                    const int rhsDistanceSq = rhsDx * rhsDx + rhsDz * rhsDz;
                    if (lhsDistanceSq != rhsDistanceSq)
                    {
                        return lhsDistanceSq < rhsDistanceSq;
                    }
                    if (lhs.z != rhs.z)
                    {
                        return lhs.z < rhs.z;
                    }
                    return lhs.x < rhs.x;
                });
            clientChunkSyncCoordsById_[clientId] = std::move(coords);
            clientChunkSyncCursorById_[clientId] = 0;
            clientChunkSyncCenterById_[clientId] = centerChunk;
        };

        constexpr std::size_t kChunkSnapshotsPerClientPerFrame = 2;
        for (const multiplayer::ConnectedClient& client : hostSession_->clients())
        {
            activeClientIds.insert(client.clientId);
            auto remotePlayerIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&client](const RemotePlayerState& remote)
                {
                    return remote.clientId == client.clientId;
                });
            if (remotePlayerIt == remotePlayers_.end())
            {
                const glm::vec3 spawnFeetPosition = findSafeMultiplayerJoinFeetPosition(playerFeetPosition_);
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = client.clientId,
                    .position = spawnFeetPosition,
                    .yawDegrees = camera_.yawDegrees(),
                    .pitchDegrees = camera_.pitchDegrees(),
                    .health = 20.0f,
                    .air = 10.0f,
                    .selectedBlockType = world::BlockType::Air,
                    .selectedEquippedItem = EquippedItem::None,
                });
                remotePlayerIt = std::prev(remotePlayers_.end());
                const world::ChunkCoord spawnChunk = world::worldToChunkCoord(
                    static_cast<int>(std::floor(spawnFeetPosition.x)),
                    static_cast<int>(std::floor(spawnFeetPosition.z)));
                logMultiplayerJoinDiag(
                    "host: new client id {} spawn feet ({:.2f},{:.2f},{:.2f}) chunk ({},{})  host feet ({:.2f},{:.2f},{:.2f})  "
                    "chunks in host world {}",
                    client.clientId,
                    spawnFeetPosition.x,
                    spawnFeetPosition.y,
                    spawnFeetPosition.z,
                    spawnChunk.x,
                    spawnChunk.z,
                    playerFeetPosition_.x,
                    playerFeetPosition_.y,
                    playerFeetPosition_.z,
                    world_.chunks().size());
            }
            const world::ChunkCoord centerChunk = remotePlayerIt != remotePlayers_.end()
                ? world::worldToChunkCoord(
                    static_cast<int>(std::floor(remotePlayerIt->position.x)),
                    static_cast<int>(std::floor(remotePlayerIt->position.z)))
                : world::worldToChunkCoord(
                    static_cast<int>(std::floor(playerFeetPosition_.x)),
                    static_cast<int>(std::floor(playerFeetPosition_.z)));
            if (!clientChunkSyncCenterById_.contains(client.clientId)
                || !(clientChunkSyncCenterById_.at(client.clientId) == centerChunk))
            {
                rebuildChunkSyncList(client.clientId, centerChunk);
            }

            const auto coordsIt = clientChunkSyncCoordsById_.find(client.clientId);
            if (coordsIt == clientChunkSyncCoordsById_.end() || coordsIt->second.empty())
            {
                continue;
            }

            std::size_t& cursor = clientChunkSyncCursorById_[client.clientId];
            const std::vector<world::ChunkCoord>& coords = coordsIt->second;
            for (std::size_t i = 0; i < kChunkSnapshotsPerClientPerFrame; ++i)
            {
                if (cursor >= coords.size())
                {
                    cursor = 0;
                }
                const world::ChunkCoord coord = coords[cursor++];
                const auto chunkIt = world_.chunks().find(coord);
                if (chunkIt == world_.chunks().end())
                {
                    continue;
                }
                multiplayer::protocol::ChunkSnapshotMessage snapshot{
                    .coord = coord,
                };
                const auto& blockStorage = chunkIt->second.blockStorage();
                for (std::size_t blockIndex = 0; blockIndex < blockStorage.size(); ++blockIndex)
                {
                    snapshot.blocks[blockIndex] = static_cast<std::uint8_t>(blockStorage[blockIndex]);
                }
                hostSession_->sendChunkSnapshot(client.clientId, snapshot);
            }
        }

        const std::vector<multiplayer::protocol::ClientInputMessage> inputs = hostSession_->takePendingInputs();
        for (const multiplayer::protocol::ClientInputMessage& input : inputs)
        {
            auto remotePlayerIt = std::find_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&input](const RemotePlayerState& state)
                {
                    return state.clientId == input.clientId;
                });
            if (remotePlayerIt == remotePlayers_.end())
            {
                const glm::vec3 spawnFeetPosition = findSafeMultiplayerJoinFeetPosition(playerFeetPosition_);
                remotePlayers_.push_back(RemotePlayerState{
                    .clientId = input.clientId,
                    .position = spawnFeetPosition,
                    .yawDegrees = camera_.yawDegrees(),
                    .pitchDegrees = input.pitchDelta,
                    .health = input.health,
                    .air = input.air,
                    .selectedBlockType = input.selectedBlockType,
                    .selectedEquippedItem = input.selectedEquippedItem,
                });
                remotePlayerIt = std::prev(remotePlayers_.end());
            }
            remotePlayerIt->position = {input.positionX, input.positionY, input.positionZ};
            remotePlayerIt->yawDegrees = input.yawDelta;
            remotePlayerIt->pitchDegrees = input.pitchDelta;
            remotePlayerIt->health = input.health;
            remotePlayerIt->air = input.air;
            remotePlayerIt->selectedBlockType = input.selectedBlockType;
            remotePlayerIt->selectedEquippedItem = input.selectedEquippedItem;

            if (input.breakBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Remove,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = world::BlockType::Air,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
            else if (input.placeBlock)
            {
                const glm::vec3 playerPosition{input.positionX, input.positionY, input.positionZ};
                const glm::vec3 targetPosition{
                    static_cast<float>(input.targetX) + 0.5f,
                    static_cast<float>(input.targetY) + 0.5f,
                    static_cast<float>(input.targetZ) + 0.5f};
                if (glm::distance(playerPosition, targetPosition) > (kInputTuning.reachDistance + 1.0f))
                {
                    continue;
                }
                const multiplayer::protocol::BlockEditEventMessage edit{
                    .authorClientId = input.clientId,
                    .action = world::WorldEditAction::Place,
                    .x = input.targetX,
                    .y = input.targetY,
                    .z = input.targetZ,
                    .blockType = input.placeBlockType,
                };
                applyRemoteBlockEdit(edit);
                hostSession_->broadcastBlockEdit(edit);
            }
        }

        while (networkTickAccumulatorSeconds_ >= kNetworkTickSeconds)
        {
            networkTickAccumulatorSeconds_ -= kNetworkTickSeconds;
            ++networkServerTick_;
            hostSession_->broadcastSnapshot(buildServerSnapshot());
        }

        multiplayerStatusLine_ =
            fmt::format("host {} client(s) @{}", hostSession_->clients().size(), multiplayerPort_);

        for (auto it = clientChunkSyncCoordsById_.begin(); it != clientChunkSyncCoordsById_.end();)
        {
            if (activeClientIds.contains(it->first))
            {
                ++it;
            }
            else
            {
                clientChunkSyncCursorById_.erase(it->first);
                clientChunkSyncCenterById_.erase(it->first);
                it = clientChunkSyncCoordsById_.erase(it);
            }
        }
        remotePlayers_.erase(
            std::remove_if(
                remotePlayers_.begin(),
                remotePlayers_.end(),
                [&activeClientIds](const RemotePlayerState& state)
                {
                    return !activeClientIds.contains(state.clientId);
                }),
            remotePlayers_.end());
    }

    if (clientSession_ != nullptr)
    {
        clientSession_->poll();
        if (const std::optional<multiplayer::protocol::JoinAcceptMessage> accepted = clientSession_->takeJoinAccept();
            accepted.has_value())
        {
            localClientId_ = accepted->clientId;
            dayNightCycle_.setElapsedSeconds(accepted->dayNightElapsedSeconds);
            weatherSystem_.setElapsedSeconds(accepted->weatherElapsedSeconds);
        }

        for (const multiplayer::protocol::ChunkSnapshotMessage& chunk : clientSession_->takeChunkSnapshots())
        {
            applyChunkSnapshot(chunk);
        }

        for (const multiplayer::protocol::BlockEditEventMessage& edit : clientSession_->takeBlockEdits())
        {
            applyRemoteBlockEdit(edit);
        }

        const std::vector<multiplayer::protocol::ServerSnapshotMessage> snapshots = clientSession_->takeSnapshots();
        if (!snapshots.empty())
        {
            const multiplayer::protocol::ServerSnapshotMessage& latest = snapshots.back();
            dayNightCycle_.setElapsedSeconds(latest.dayNightElapsedSeconds);
            weatherSystem_.setElapsedSeconds(latest.weatherElapsedSeconds);
            droppedItems_.clear();
            droppedItems_.reserve(latest.droppedItems.size());
            for (const multiplayer::protocol::DroppedItemSnapshotMessage& droppedItem : latest.droppedItems)
            {
                droppedItems_.push_back(DroppedItem{
                    .blockType = droppedItem.blockType,
                    .worldPosition = {droppedItem.posX, droppedItem.posY, droppedItem.posZ},
                    .velocity = {droppedItem.velocityX, droppedItem.velocityY, droppedItem.velocityZ},
                    .ageSeconds = droppedItem.ageSeconds,
                    .pickupDelaySeconds = 0.1f,
                    .spinRadians = droppedItem.spinRadians,
                });
            }
            std::vector<RemotePlayerState> updatedRemotePlayers;
            updatedRemotePlayers.reserve(latest.players.size());
            for (const multiplayer::protocol::PlayerSnapshotMessage& player : latest.players)
            {
                if (player.clientId == localClientId_)
                {
                    const glm::vec3 authoritativePosition{player.posX, player.posY, player.posZ};
                    if (pendingClientJoinAfterWorldLoad_)
                    {
                        if (clientJoinAuthoritativeSnapLogsRemaining_ > 0)
                        {
                            --clientJoinAuthoritativeSnapLogsRemaining_;
                            const world::ChunkCoord authChunk = world::worldToChunkCoord(
                                static_cast<int>(std::floor(authoritativePosition.x)),
                                static_cast<int>(std::floor(authoritativePosition.z)));
                            logMultiplayerJoinDiag(
                                "client: authoritative snap during join ({} left) feet ({:.2f},{:.2f},{:.2f}) chunk "
                                "({},{}) yaw {:.1f} pitch {:.1f} worldChunks {} tick {}",
                                static_cast<int>(clientJoinAuthoritativeSnapLogsRemaining_),
                                authoritativePosition.x,
                                authoritativePosition.y,
                                authoritativePosition.z,
                                authChunk.x,
                                authChunk.z,
                                player.yawDegrees,
                                player.pitchDegrees,
                                world_.chunks().size(),
                                latest.serverTick);
                        }
                        // During join bootstrap, snap to the host-authoritative spawn so chunk loading
                        // targets the same area the host chose for this client.
                        playerFeetPosition_ = authoritativePosition;
                        spawnFeetPosition_ = authoritativePosition;
                        verticalVelocity_ = 0.0f;
                        accumulatedFallDistance_ = 0.0f;
                        isGrounded_ = isGroundedAtFeetPosition(
                            world_,
                            playerFeetPosition_,
                            kPlayerMovementSettings.standingColliderHeight);
                        playerHazards_ = samplePlayerHazards(
                            world_,
                            playerFeetPosition_,
                            kPlayerMovementSettings.standingColliderHeight,
                            kPlayerMovementSettings.standingEyeHeight);
                        camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                    }
                    else
                    {
                        // After join bootstrap the client is authoritative for its own movement,
                        // so the host snapshot is effectively an echo of our earlier input.
                        // Pulling toward it every frame adds visible lag and weakens jumping.
                        const glm::vec3 authoritativeDelta = authoritativePosition - playerFeetPosition_;
                        if (glm::dot(authoritativeDelta, authoritativeDelta) > 4.0f)
                        {
                            playerFeetPosition_ = authoritativePosition;
                            verticalVelocity_ = 0.0f;
                            accumulatedFallDistance_ = 0.0f;
                            isGrounded_ = isGroundedAtFeetPosition(
                                world_,
                                playerFeetPosition_,
                                kPlayerMovementSettings.standingColliderHeight);
                            playerHazards_ = samplePlayerHazards(
                                world_,
                                playerFeetPosition_,
                                kPlayerMovementSettings.standingColliderHeight,
                                kPlayerMovementSettings.standingEyeHeight);
                            camera_.setYawPitch(player.yawDegrees, player.pitchDegrees);
                        }
                    }
                    camera_.setPosition(
                        playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
                    continue;
                }
                const auto previousIt = std::find_if(
                    remotePlayers_.begin(),
                    remotePlayers_.end(),
                    [&player](const RemotePlayerState& state)
                    {
                        return state.clientId == player.clientId;
                    });
                const glm::vec3 targetPosition{player.posX, player.posY, player.posZ};
                const glm::vec3 smoothedPosition = previousIt == remotePlayers_.end()
                    ? targetPosition
                    : glm::mix(previousIt->position, targetPosition, 0.35f);
                const float smoothedYawDegrees = previousIt == remotePlayers_.end()
                    ? normalizeDegrees(player.yawDegrees)
                    : lerpDegrees(previousIt->yawDegrees, player.yawDegrees, 0.35f);
                const float smoothedPitchDegrees = previousIt == remotePlayers_.end()
                    ? std::clamp(player.pitchDegrees, -89.0f, 89.0f)
                    : std::clamp(
                        previousIt->pitchDegrees + (player.pitchDegrees - previousIt->pitchDegrees) * 0.35f,
                        -89.0f,
                        89.0f);
                updatedRemotePlayers.push_back(RemotePlayerState{
                    .clientId = player.clientId,
                    .position = smoothedPosition,
                    .yawDegrees = smoothedYawDegrees,
                    .pitchDegrees = smoothedPitchDegrees,
                    .health = player.health,
                    .air = player.air,
                    .selectedBlockType = player.selectedBlockType,
                    .selectedEquippedItem = player.selectedEquippedItem,
                });
            }
            remotePlayers_ = std::move(updatedRemotePlayers);
        }

        if (clientSession_->connected())
        {
            if (!pendingClientJoinAfterWorldLoad_)
            {
                clientSession_->sendInput(
                    {
                        .clientId = localClientId_,
                        .dtSeconds = deltaTimeSeconds,
                        .positionX = playerFeetPosition_.x,
                        .positionY = playerFeetPosition_.y,
                        .positionZ = playerFeetPosition_.z,
                        .yawDelta = camera_.yawDegrees(),
                        .pitchDelta = camera_.pitchDegrees(),
                        .health = playerVitals_.health(),
                        .air = playerVitals_.air(),
                        .selectedEquippedItem = hotbarSlots_[selectedHotbarIndex_].equippedItem,
                        .selectedBlockType = hotbarSlots_[selectedHotbarIndex_].blockType,
                        .selectedHotbarIndex = static_cast<std::uint8_t>(selectedHotbarIndex_),
                    },
                    networkServerTick_);
            }
            multiplayerStatusLine_ = fmt::format("client id {} -> {}:{}", localClientId_, multiplayerAddress_, multiplayerPort_);
        }
        else if (clientSession_->connecting())
        {
            multiplayerStatusLine_ = fmt::format("connecting {}:{}...", multiplayerAddress_, multiplayerPort_);
        }
        else if (!clientSession_->lastError().empty())
        {
            multiplayerStatusLine_ = "client error: " + clientSession_->lastError();
        }
    }
}

glm::vec3 Application::findSafeMultiplayerJoinFeetPosition(const glm::vec3& anchorFeetPosition) const
{
    const float colliderHeight = kPlayerMovementSettings.standingColliderHeight;
    const float minDistance = kPlayerMovementSettings.colliderHalfWidth * 2.0f + 0.35f;
    const float minDistanceSq = minDistance * minDistance;
    const std::array<glm::vec2, 8> ringDirections{
        glm::vec2{1.0f, 0.0f},
        glm::vec2{0.7071f, 0.7071f},
        glm::vec2{0.0f, 1.0f},
        glm::vec2{-0.7071f, 0.7071f},
        glm::vec2{-1.0f, 0.0f},
        glm::vec2{-0.7071f, -0.7071f},
        glm::vec2{0.0f, -1.0f},
        glm::vec2{0.7071f, -0.7071f},
    };

    for (int radiusStep = 1; radiusStep <= 6; ++radiusStep)
    {
        const float radius = 1.8f + static_cast<float>(radiusStep - 1) * 0.9f;
        for (const glm::vec2& direction : ringDirections)
        {
            const glm::vec3 candidateProbe{
                anchorFeetPosition.x + direction.x * radius,
                anchorFeetPosition.y,
                anchorFeetPosition.z + direction.y * radius,
            };
            const glm::vec3 candidateFeet =
                findInitialSpawnFeetPosition(world_, terrainGenerator_, candidateProbe, colliderHeight);
            const glm::vec2 horizontalDelta{
                candidateFeet.x - anchorFeetPosition.x,
                candidateFeet.z - anchorFeetPosition.z,
            };
            if (glm::dot(horizontalDelta, horizontalDelta) < minDistanceSq)
            {
                continue;
            }
            if (game::collidesWithSolidBlock(world_, playerAabbAt(candidateFeet, colliderHeight)))
            {
                continue;
            }
            return candidateFeet;
        }
    }

    const glm::vec3 fallbackProbe{
        anchorFeetPosition.x + 2.2f,
        anchorFeetPosition.y,
        anchorFeetPosition.z,
    };
    return findInitialSpawnFeetPosition(world_, terrainGenerator_, fallbackProbe, colliderHeight);
}

void Application::clearClientWorldAwaitingHostChunks()
{
    logMultiplayerJoinDiag("clearClientWorldAwaitingHostChunks (drop resident meshes + empty chunk map)");
    std::vector<std::uint64_t> removedMeshIds(residentChunkMeshIds_.begin(), residentChunkMeshIds_.end());
    if (!removedMeshIds.empty())
    {
        renderer_.updateSceneMeshes({}, removedMeshIds);
    }
    residentChunkMeshIds_.clear();

    vibecraft::world::World::ChunkMap emptyChunks;
    world_.replaceChunks(std::move(emptyChunks));
    droppedItems_.clear();
    mobSpawnSystem_.clearAllMobs();
    activeMiningState_ = {};
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    footstepDistanceAccumulator_ = 0.0f;
    heldItemSwing_ = 0.0f;
}

void Application::beginSingleplayerLoad()
{
    if (gameScreen_ != GameScreen::MainMenu || singleplayerLoadState_.active)
    {
        return;
    }

    pendingClientJoinAfterWorldLoad_ = false;
    singleplayerLoadState_.active = true;
    singleplayerLoadState_.worldPrepared = false;
    singleplayerLoadState_.progress = 0.0f;
    singleplayerLoadState_.label = "Generating world...";

    stopMultiplayerSessions();
    mainMenuMultiplayerPanel_ = MainMenuMultiplayerPanel::None;
    mainMenuSoundSettingsOpen_ = false;
    mainMenuNotice_.clear();
    window_.setTextInputActive(false);
    mouseCaptured_ = false;
    window_.setRelativeMouseMode(false);
    inputState_.clearMouseMotion();

    std::error_code errorCode;
    std::filesystem::remove(savePath_, errorCode);

    std::vector<std::uint64_t> removedMeshIds(residentChunkMeshIds_.begin(), residentChunkMeshIds_.end());
    if (!removedMeshIds.empty())
    {
        renderer_.updateSceneMeshes({}, removedMeshIds);
    }
    residentChunkMeshIds_.clear();

    vibecraft::world::World::ChunkMap emptyChunks;
    world_.replaceChunks(std::move(emptyChunks));
    droppedItems_.clear();
    remotePlayers_.clear();
    worldSyncSentClients_.clear();
    clientChunkSyncCoordsById_.clear();
    clientChunkSyncCursorById_.clear();
    clientChunkSyncCenterById_.clear();
    mobSpawnSystem_.clearAllMobs();
    applyDefaultHotbarLoadout(hotbarSlots_, selectedHotbarIndex_);
    bagSlots_.fill({});
    activeMiningState_ = {};
    playerVitals_.reset();
    playerHazards_ = {};
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    isGrounded_ = false;
    jumpWasHeld_ = false;
    footstepDistanceAccumulator_ = 0.0f;
    heldItemSwing_ = 0.0f;
    respawnNotice_.clear();
    dayNightCycle_.setElapsedSeconds(70.0f);
    weatherSystem_.setElapsedSeconds(0.0f);
    const std::uint32_t worldSeed = generateRandomWorldSeed();
    world_.setGenerationSeed(worldSeed);
    terrainGenerator_.setWorldSeed(worldSeed);
    core::logInfo(fmt::format("Starting new singleplayer world with seed {}", worldSeed));
}

void Application::updateSingleplayerLoad()
{
    if (pendingClientJoinAfterWorldLoad_)
    {
        ++clientJoinLoadDebugFrame_;
        if (clientSession_ == nullptr)
        {
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;
            mainMenuNotice_ = "Could not connect. Check address, firewall, and that the host is running.";
            return;
        }

        world::ChunkCoord cameraChunk = world::worldToChunkCoord(
            static_cast<int>(std::floor(camera_.position().x)),
            static_cast<int>(std::floor(camera_.position().z)));
        const std::size_t residentTarget = static_cast<std::size_t>(
            (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1));
        const bool connected = clientSession_->connected();

        // While connected but before the first chunk snapshot arrives, keep a clear waiting phase.
        if (connected && world_.chunks().empty())
        {
            singleplayerLoadState_.progress = std::max(singleplayerLoadState_.progress, 0.18f);
            singleplayerLoadState_.label = "Waiting for world data from host...";
            if ((clientJoinLoadDebugFrame_ % 45U) == 1U)
            {
                logMultiplayerJoinDiag(
                    "client join load: waiting for first chunk (frame {}) connected={} localId={}",
                    clientJoinLoadDebugFrame_,
                    connected,
                    localClientId_);
            }
            return;
        }

        if (!clientJoinLoggedFirstChunkSummary_ && !world_.chunks().empty())
        {
            clientJoinLoggedFirstChunkSummary_ = true;
            world::ChunkCoord minC{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()};
            world::ChunkCoord maxC{std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
            for (const auto& [coord, chunk] : world_.chunks())
            {
                static_cast<void>(chunk);
                minC.x = std::min(minC.x, coord.x);
                minC.z = std::min(minC.z, coord.z);
                maxC.x = std::max(maxC.x, coord.x);
                maxC.z = std::max(maxC.z, coord.z);
            }
            logMultiplayerJoinDiag(
                "client join load: first chunk data — count {}  AABB chunks x[{},{}] z[{},{}]  cameraChunk ({},{})  "
                "feet ({:.2f},{:.2f},{:.2f})",
                world_.chunks().size(),
                minC.x,
                maxC.x,
                minC.z,
                maxC.z,
                cameraChunk.x,
                cameraChunk.z,
                playerFeetPosition_.x,
                playerFeetPosition_.y,
                playerFeetPosition_.z);
        }

        // If our current camera chunk is outside the received host snapshot area (e.g. stale local position),
        // re-anchor loading to the first received chunk so resident counting can progress.
        if (!world_.chunks().empty())
        {
            bool hasChunkNearCamera = false;
            for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
                 chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius && !hasChunkNearCamera;
                 ++chunkZ)
            {
                for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
                     chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
                     ++chunkX)
                {
                    if (world_.chunks().contains(world::ChunkCoord{chunkX, chunkZ}))
                    {
                        hasChunkNearCamera = true;
                        break;
                    }
                }
            }

            if (!hasChunkNearCamera)
            {
                const world::ChunkCoord firstReceivedCoord = world_.chunks().begin()->first;
                logMultiplayerJoinDiag(
                    "client join load: re-anchor camera to first received chunk — was ({},{}) -> ({},{}) "
                    "(no resident chunks near prior camera)",
                    cameraChunk.x,
                    cameraChunk.z,
                    firstReceivedCoord.x,
                    firstReceivedCoord.z);
                playerFeetPosition_.x = static_cast<float>(firstReceivedCoord.x * world::Chunk::kSize)
                    + static_cast<float>(world::Chunk::kSize) * 0.5f;
                playerFeetPosition_.z = static_cast<float>(firstReceivedCoord.z * world::Chunk::kSize)
                    + static_cast<float>(world::Chunk::kSize) * 0.5f;
                camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
                cameraChunk = firstReceivedCoord;
            }
        }

        const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
        std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
            dirtyCoords.begin(),
            dirtyCoords.end());
        const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
            world_,
            chunkMesher_,
            residentChunkMeshIds_,
            dirtyCoordSet,
            cameraChunk,
            std::max<std::size_t>(12, kStreamingSettings.meshBuildBudgetPerFrame * 3));
        applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);
        if (!cpuData.dirtyResidentMeshUpdates.empty())
        {
            world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
        }

        std::size_t residentGeneratedCount = 0;
        std::size_t residentMeshCount = 0;
        std::size_t residentDirtyCount = 0;
        for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
             chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
             ++chunkZ)
        {
            for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
                 chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
                 ++chunkX)
            {
                const world::ChunkCoord coord{chunkX, chunkZ};
                if (world_.chunks().contains(coord))
                {
                    ++residentGeneratedCount;
                }
                if (residentChunkMeshIds_.contains(chunkMeshId(coord)))
                {
                    ++residentMeshCount;
                }
                if (dirtyCoordSet.contains(coord))
                {
                    ++residentDirtyCount;
                }
            }
        }

        if (connected)
        {
            if ((clientJoinLoadDebugFrame_ % 30U) == 0U)
            {
                logMultiplayerJoinDiag(
                    "client join load: frame {}  camChunk ({},{})  feet ({:.2f},{:.2f},{:.2f})  yaw {:.1f}  "
                    "terrain {}/{}  mesh {}/{}  dirtyInWindow {}  worldChunks {}",
                    clientJoinLoadDebugFrame_,
                    cameraChunk.x,
                    cameraChunk.z,
                    playerFeetPosition_.x,
                    playerFeetPosition_.y,
                    playerFeetPosition_.z,
                    camera_.yawDegrees(),
                    residentGeneratedCount,
                    residentTarget,
                    residentMeshCount,
                    residentTarget,
                    residentDirtyCount,
                    world_.chunks().size());
            }
            singleplayerLoadState_.label = fmt::format(
                "Receiving world... terrain {}/{}  meshes {}/{}",
                residentGeneratedCount,
                residentTarget,
                residentMeshCount,
                residentTarget);
            const float residentGeneratedProgress = residentTarget > 0
                ? static_cast<float>(residentGeneratedCount) / static_cast<float>(residentTarget)
                : 1.0f;
            const float residentMeshProgress = residentTarget > 0
                ? static_cast<float>(residentMeshCount) / static_cast<float>(residentTarget)
                : 1.0f;
            singleplayerLoadState_.progress =
                std::clamp(0.25f + residentGeneratedProgress * 0.35f + residentMeshProgress * 0.40f, 0.0f, 1.0f);
        }
        else if (clientSession_->connecting())
        {
            singleplayerLoadState_.progress = std::max(singleplayerLoadState_.progress, 0.08f);
            singleplayerLoadState_.label = fmt::format("Connecting to {}:{}...", multiplayerAddress_, multiplayerPort_);
            return;
        }
        else
        {
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;
            mainMenuNotice_ = clientSession_->lastError().empty()
                ? "Could not connect. Check address, firewall, and that the host is running."
                : "Could not connect: " + clientSession_->lastError();
            stopMultiplayerSessions();
            return;
        }

        if (connected && residentGeneratedCount >= residentTarget && residentMeshCount >= residentTarget
            && residentDirtyCount == 0)
        {
            singleplayerLoadState_.progress = 1.0f;
            singleplayerLoadState_.label = "World ready";
            singleplayerLoadState_.active = false;
            pendingClientJoinAfterWorldLoad_ = false;

            gameScreen_ = GameScreen::Playing;
            mouseCaptured_ = true;
            window_.setRelativeMouseMode(true);
            inputState_.clearMouseMotion();
        }
        return;
    }

    const std::size_t bootstrapTarget = static_cast<std::size_t>(
        (kStreamingSettings.bootstrapChunkRadius * 2 + 1) * (kStreamingSettings.bootstrapChunkRadius * 2 + 1));
    const world::ChunkCoord originChunk{0, 0};

    if (!singleplayerLoadState_.worldPrepared)
    {
        world_.generateMissingChunksAround(
            terrainGenerator_,
            originChunk,
            kStreamingSettings.bootstrapChunkRadius,
            std::max<std::size_t>(6, kStreamingSettings.generationChunkBudgetPerFrame));

        const std::size_t generatedCount = std::min(world_.chunks().size(), bootstrapTarget);
        singleplayerLoadState_.progress =
            0.45f * (bootstrapTarget > 0 ? static_cast<float>(generatedCount) / static_cast<float>(bootstrapTarget) : 1.0f);
        singleplayerLoadState_.label =
            fmt::format("Generating world... {}/{} chunks", generatedCount, bootstrapTarget);

        if (generatedCount < bootstrapTarget)
        {
            return;
        }

        const float spawnHeight = kPlayerMovementSettings.standingColliderHeight;
        playerFeetPosition_ = resolveSpawnFeetPosition(
            world_,
            terrainGenerator_,
            spawnPreset_,
            spawnBiomeTarget_,
            camera_.position(),
            spawnHeight);
        isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, spawnHeight);
        spawnFeetPosition_ = playerFeetPosition_;
        accumulatedFallDistance_ = 0.0f;
        playerHazards_ = samplePlayerHazards(
            world_,
            playerFeetPosition_,
            kPlayerMovementSettings.standingColliderHeight,
            kPlayerMovementSettings.standingEyeHeight);
        camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
        singleplayerLoadState_.worldPrepared = true;
    }

    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    const std::size_t residentTarget = static_cast<std::size_t>(
        (kStreamingSettings.residentChunkRadius * 2 + 1) * (kStreamingSettings.residentChunkRadius * 2 + 1));

    world_.generateMissingChunksAround(
        terrainGenerator_,
        cameraChunk,
        kStreamingSettings.residentChunkRadius,
        std::max<std::size_t>(12, kStreamingSettings.generationChunkBudgetPerFrame * 2));

    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
        world_,
        chunkMesher_,
        residentChunkMeshIds_,
        dirtyCoordSet,
        cameraChunk,
        std::max<std::size_t>(12, kStreamingSettings.meshBuildBudgetPerFrame * 3));
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);
    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }

    std::size_t residentGeneratedCount = 0;
    std::size_t residentMeshCount = 0;
    std::size_t residentDirtyCount = 0;
    for (int chunkZ = cameraChunk.z - kStreamingSettings.residentChunkRadius;
         chunkZ <= cameraChunk.z + kStreamingSettings.residentChunkRadius;
         ++chunkZ)
    {
        for (int chunkX = cameraChunk.x - kStreamingSettings.residentChunkRadius;
             chunkX <= cameraChunk.x + kStreamingSettings.residentChunkRadius;
             ++chunkX)
        {
            const world::ChunkCoord coord{chunkX, chunkZ};
            if (world_.chunks().contains(coord))
            {
                ++residentGeneratedCount;
            }
            if (residentChunkMeshIds_.contains(chunkMeshId(coord)))
            {
                ++residentMeshCount;
            }
            if (dirtyCoordSet.contains(coord))
            {
                ++residentDirtyCount;
            }
        }
    }

    const float residentGeneratedProgress = residentTarget > 0
        ? static_cast<float>(residentGeneratedCount) / static_cast<float>(residentTarget)
        : 1.0f;
    const float residentMeshProgress = residentTarget > 0
        ? static_cast<float>(residentMeshCount) / static_cast<float>(residentTarget)
        : 1.0f;
    singleplayerLoadState_.progress =
        std::clamp(0.45f + residentGeneratedProgress * 0.2f + residentMeshProgress * 0.35f, 0.0f, 1.0f);
    singleplayerLoadState_.label = fmt::format(
        "Preparing spawn area... terrain {}/{}  meshes {}/{}",
        residentGeneratedCount,
        residentTarget,
        residentMeshCount,
        residentTarget);

    if (residentGeneratedCount >= residentTarget && residentMeshCount >= residentTarget && residentDirtyCount == 0)
    {
        singleplayerLoadState_.progress = 1.0f;
        singleplayerLoadState_.label = "World ready";
        singleplayerLoadState_.active = false;

        if (pendingHostStartAfterWorldLoad_)
        {
            if (!startHostSession())
            {
                pendingHostStartAfterWorldLoad_ = false;
                mainMenuNotice_ = "Could not start hosting. Is the port already in use?";
                return;
            }
            pendingHostStartAfterWorldLoad_ = false;
        }

        gameScreen_ = GameScreen::Playing;
        mouseCaptured_ = true;
        window_.setRelativeMouseMode(true);
        inputState_.clearMouseMotion();
    }
}

void Application::syncWorldData()
{
    const world::ChunkCoord cameraChunk = world::worldToChunkCoord(
        static_cast<int>(std::floor(camera_.position().x)),
        static_cast<int>(std::floor(camera_.position().z)));
    // Clients must only render chunks sent by the host; local procedural fill would desync terrain.
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        world_.generateMissingChunksAround(
            terrainGenerator_,
            cameraChunk,
            kStreamingSettings.generationChunkRadius,
            kStreamingSettings.generationChunkBudgetPerFrame);

        glm::vec3 prefetchDirection = camera_.forward();
        prefetchDirection.y = 0.0f;
        if (glm::dot(prefetchDirection, prefetchDirection) > 0.0f)
        {
            prefetchDirection = glm::normalize(prefetchDirection);
            const float prefetchDistanceWorldUnits =
                static_cast<float>(kStreamingSettings.forwardPrefetchChunks * world::Chunk::kSize);
            const int prefetchWorldX =
                static_cast<int>(std::floor(camera_.position().x + prefetchDirection.x * prefetchDistanceWorldUnits));
            const int prefetchWorldZ =
                static_cast<int>(std::floor(camera_.position().z + prefetchDirection.z * prefetchDistanceWorldUnits));
            const world::ChunkCoord prefetchChunk = world::worldToChunkCoord(prefetchWorldX, prefetchWorldZ);
            if (!(prefetchChunk == cameraChunk))
            {
                world_.generateMissingChunksAround(
                    terrainGenerator_,
                    prefetchChunk,
                    kStreamingSettings.generationChunkRadius,
                    kStreamingSettings.prefetchGenerationBudgetPerFrame);
            }
        }
    }

    const std::vector<world::ChunkCoord> dirtyCoords = world_.dirtyChunkCoords();
    std::unordered_set<world::ChunkCoord, world::ChunkCoordHash> dirtyCoordSet(
        dirtyCoords.begin(),
        dirtyCoords.end());
    const MeshSyncCpuData cpuData = buildMeshSyncCpuData(
        world_,
        chunkMesher_,
        residentChunkMeshIds_,
        dirtyCoordSet,
        cameraChunk,
        kStreamingSettings.meshBuildBudgetPerFrame);
    applyMeshSyncGpuData(renderer_, cpuData, residentChunkMeshIds_);

    if (!cpuData.dirtyResidentMeshUpdates.empty())
    {
        world_.applyMeshStatsAndClearDirty(cpuData.dirtyResidentMeshUpdates);
    }

    std::vector<world::ChunkCoord> offResidentDirtyCoords;
    offResidentDirtyCoords.reserve(kStreamingSettings.offResidentDirtyRebuildBudget);
    for (const world::ChunkCoord& coord : dirtyCoords)
    {
        if (cpuData.desiredResidentIds.contains(chunkMeshId(coord)))
        {
            continue;
        }

        offResidentDirtyCoords.push_back(coord);
        if (offResidentDirtyCoords.size() >= kStreamingSettings.offResidentDirtyRebuildBudget)
        {
            break;
        }
    }

    if (!offResidentDirtyCoords.empty())
    {
        world_.rebuildDirtyMeshes(chunkMesher_, offResidentDirtyCoords);
    }
}
}  // namespace vibecraft::app
