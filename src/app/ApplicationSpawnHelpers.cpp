#include "vibecraft/app/ApplicationSpawnHelpers.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <random>

#include "vibecraft/app/Application.hpp"
#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/input/ApplicationInputMenuHelpers.hpp"
#include "vibecraft/game/CollisionHelpers.hpp"
#include "vibecraft/world/BlockMetadata.hpp"
#include "vibecraft/world/biomes/BiomeProfile.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] game::Aabb playerAabbAt(const glm::vec3& feetPosition, const float colliderHeight)
{
    return game::aabbAtFeet(feetPosition, kPlayerMovementSettings.colliderHalfWidth, colliderHeight);
}

[[nodiscard]] bool feetRestOnDryLand(
    const world::World& worldState,
    const glm::vec3& feetPosition)
{
    const int belowX = static_cast<int>(std::floor(feetPosition.x));
    const int belowY = static_cast<int>(std::floor(feetPosition.y - kFloatEpsilon));
    const int belowZ = static_cast<int>(std::floor(feetPosition.z));
    if (belowY < world::kWorldMinY || belowY > world::kWorldMaxY)
    {
        return false;
    }
    const world::BlockType belowBlock = worldState.blockAt(belowX, belowY, belowZ);
    return world::isSolid(belowBlock) && !world::isFluid(belowBlock);
}

[[nodiscard]] bool aabbTouchesFluid(
    const world::World& worldState,
    const game::Aabb& aabb)
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
                if (world::isFluid(worldState.blockAt(x, y, z)))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

[[nodiscard]] bool isSpawnFeetPositionSafe(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight)
{
    const game::Aabb candidateAabb = playerAabbAt(feetPosition, colliderHeight);
    if (game::collidesWithSolidBlock(worldState, candidateAabb))
    {
        return false;
    }
    if (aabbTouchesFluid(worldState, candidateAabb))
    {
        return false;
    }
    return feetRestOnDryLand(worldState, feetPosition);
}

[[nodiscard]] bool isFoliageCrowdingBlock(const world::BlockType blockType)
{
    using BK = world::BlockType;
    return blockType == BK::OakLeaves || blockType == BK::JungleLeaves || blockType == BK::SpruceLeaves
        || blockType == BK::OakLog || blockType == BK::JungleLog || blockType == BK::SpruceLog
        || blockType == BK::Bamboo || blockType == BK::Vines;
}

[[nodiscard]] int spawnCrowdingScore(const world::World& worldState, const glm::vec3& feetPosition)
{
    const int baseX = static_cast<int>(std::floor(feetPosition.x));
    const int baseY = static_cast<int>(std::floor(feetPosition.y));
    const int baseZ = static_cast<int>(std::floor(feetPosition.z));
    int score = 0;

    for (int dy = 0; dy <= 5; ++dy)
    {
        for (int dz = -2; dz <= 2; ++dz)
        {
            for (int dx = -2; dx <= 2; ++dx)
            {
                if (dx == 0 && dz == 0 && dy <= 2)
                {
                    continue;
                }
                const world::BlockType blockType = worldState.blockAt(baseX + dx, baseY + dy, baseZ + dz);
                if (blockType == world::BlockType::Air)
                {
                    continue;
                }
                score += isFoliageCrowdingBlock(blockType) ? 3 : 1;
            }
        }
    }

    return score;
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

    constexpr int kSpawnClearanceSearchLimit = world::kWorldHeight;
    for (int attempt = 0; attempt <= kSpawnClearanceSearchLimit; ++attempt)
    {
        if (spawnFeetPosition.y >= static_cast<float>(world::kWorldMaxY) - colliderHeight)
        {
            break;
        }
        if (isSpawnFeetPositionSafe(worldState, spawnFeetPosition, colliderHeight))
        {
            return spawnFeetPosition;
        }
        spawnFeetPosition.y += 1.0f;
    }

    return spawnFeetPosition;
}

[[nodiscard]] std::optional<glm::vec3> findNearbyDrySpawnFeetPosition(
    const world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& probePosition,
    const float colliderHeight)
{
    constexpr int kSearchStep = 8;
    constexpr int kSearchRadius = 256;
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
                const glm::vec3 candidateProbe{
                    probePosition.x + static_cast<float>(dx),
                    probePosition.y,
                    probePosition.z + static_cast<float>(dz),
                };
                const glm::vec3 candidateFeet = findInitialSpawnFeetPosition(
                    worldState,
                    terrainGenerator,
                    candidateProbe,
                    colliderHeight);
                if (isSpawnFeetPositionSafe(worldState, candidateFeet, colliderHeight))
                {
                    return candidateFeet;
                }
            }
        }
    }
    return std::nullopt;
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
        // UI label "Forest": green overworld biomes (exclude desert, cold, jungle, dry savanna).
        return biome == SB::Plains || biome == SB::SunflowerPlains || biome == SB::Meadow
            || biome == SB::Forest || biome == SB::FlowerForest || biome == SB::BirchForest
            || biome == SB::OldGrowthBirchForest || biome == SB::DarkForest || biome == SB::WindsweptHills;
    case SpawnBiomeTarget::Sandy:
        return biome == SB::Desert;
    case SpawnBiomeTarget::Snowy:
        return world::biomes::isSnowySurfaceBiome(biome);
    case SpawnBiomeTarget::Jungle:
        return world::biomes::isJungleSurfaceBiome(biome);
    }
    return true;
}

[[nodiscard]] bool isSpawnBiomeCoreArea(
    const world::TerrainGenerator& terrainGenerator,
    const int worldX,
    const int worldZ,
    const SpawnBiomeTarget target)
{
    if (target == SpawnBiomeTarget::Any)
    {
        return true;
    }

    // Local consistency only (not a huge homogeneous region). The old 9-point ±96 grid
    // rejected valid spawns on normal biome edges and made "Travel now" often fall back incorrectly.
    constexpr std::array<std::array<int, 2>, 5> kCoreOffsets{{
        {{0, 0}},
        {{24, 0}},
        {{-24, 0}},
        {{0, 24}},
        {{0, -24}},
    }};

    for (const auto& offset : kCoreOffsets)
    {
        const world::SurfaceBiome sampledBiome =
            terrainGenerator.surfaceBiomeAt(worldX + offset[0], worldZ + offset[1]);
        if (!matchesSpawnBiomeTarget(sampledBiome, target))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool isPreferredSpawnSurfaceBlock(
    const world::BlockType blockType,
    const SpawnBiomeTarget target)
{
    using BK = world::BlockType;
    switch (target)
    {
    case SpawnBiomeTarget::Temperate:
        return blockType == BK::Grass;
    case SpawnBiomeTarget::Sandy:
        return blockType == BK::Sand || blockType == BK::Sandstone;
    case SpawnBiomeTarget::Snowy:
        return blockType == BK::SnowGrass;
    case SpawnBiomeTarget::Jungle:
        return blockType == BK::JungleGrass;
    case SpawnBiomeTarget::Any:
    default:
        return blockType == BK::Grass || blockType == BK::JungleGrass || blockType == BK::SnowGrass;
    }
}

[[nodiscard]] int spawnSurfacePenalty(
    const world::TerrainGenerator& terrainGenerator,
    const glm::vec3& feetPosition,
    const SpawnBiomeTarget target)
{
    constexpr int kExpectedSeaLevel = 63;
    const int baseX = static_cast<int>(std::floor(feetPosition.x));
    const int baseZ = static_cast<int>(std::floor(feetPosition.z));
    const int surfaceY = terrainGenerator.surfaceHeightAt(baseX, baseZ);
    const world::BlockType baseSurfaceBlock = terrainGenerator.blockTypeAt(baseX, surfaceY, baseZ);
    int penalty = isPreferredSpawnSurfaceBlock(baseSurfaceBlock, target) ? 0 : 80;
    if (surfaceY <= kExpectedSeaLevel + 2)
    {
        penalty += 90;
    }

    for (int dz = -8; dz <= 8; ++dz)
    {
        for (int dx = -8; dx <= 8; ++dx)
        {
            const int sampleX = baseX + dx;
            const int sampleZ = baseZ + dz;
            const int sampleSurfaceY = terrainGenerator.surfaceHeightAt(sampleX, sampleZ);
            const world::BlockType sampleSurfaceBlock = terrainGenerator.blockTypeAt(sampleX, sampleSurfaceY, sampleZ);
            const world::BlockType aboveSurfaceBlock = terrainGenerator.blockTypeAt(sampleX, sampleSurfaceY + 1, sampleZ);

            if (!matchesSpawnBiomeTarget(terrainGenerator.surfaceBiomeAt(sampleX, sampleZ), target))
            {
                penalty += 10;
            }
            if (!isPreferredSpawnSurfaceBlock(sampleSurfaceBlock, target))
            {
                penalty += (dx == 0 && dz == 0) ? 40 : 4;
            }
            if (aboveSurfaceBlock == world::BlockType::Water)
            {
                penalty += (std::abs(dx) <= 2 && std::abs(dz) <= 2) ? 80 : 18;
            }
            if (sampleSurfaceY <= kExpectedSeaLevel + 1)
            {
                penalty += 3;
            }
            if (std::abs(sampleSurfaceY - surfaceY) >= 4)
            {
                penalty += 2;
            }
        }
    }

    return penalty;
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

[[nodiscard]] std::vector<glm::ivec2> buildSpawnSearchOffsets()
{
    constexpr int kSearchStep = 24;
    constexpr int kSearchRadius = 4096;

    std::vector<glm::ivec2> offsets;
    offsets.reserve(static_cast<std::size_t>((kSearchRadius / kSearchStep) * 32));
    offsets.push_back(glm::ivec2{0, 0});
    for (int radius = kSearchStep; radius <= kSearchRadius; radius += kSearchStep)
    {
        for (int dz = -radius; dz <= radius; dz += kSearchStep)
        {
            for (int dx = -radius; dx <= radius; dx += kSearchStep)
            {
                if (std::abs(dx) != radius && std::abs(dz) != radius)
                {
                    continue;
                }
                offsets.push_back(glm::ivec2{dx, dz});
            }
        }
    }

    return offsets;
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
}  // namespace

std::uint32_t generateRandomWorldSeed()
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

glm::vec3 resolveSpawnFeetPosition(
    world::World& worldState,
    const world::TerrainGenerator& terrainGenerator,
    const SpawnPreset spawnPreset,
    const SpawnBiomeTarget spawnBiomeTarget,
    const glm::vec3& fallbackCameraPosition,
    const float colliderHeight)
{
    constexpr int kSpawnProbeChunkRadius = 2;
    constexpr std::size_t kSpawnProbeChunkBudget =
        static_cast<std::size_t>((kSpawnProbeChunkRadius * 2 + 1) * (kSpawnProbeChunkRadius * 2 + 1));
    const auto ensureProbeGenerated = [&](const int worldX, const int worldZ)
    {
        worldState.generateMissingChunksAround(
            terrainGenerator,
            world::worldToChunkCoord(worldX, worldZ),
            kSpawnProbeChunkRadius,
            kSpawnProbeChunkBudget);
    };

    const glm::vec3 spawnProbePosition = preferredSpawnProbePosition(spawnPreset, fallbackCameraPosition);
    ensureProbeGenerated(
        static_cast<int>(std::floor(spawnProbePosition.x)),
        static_cast<int>(std::floor(spawnProbePosition.z)));
    if (spawnBiomeTarget == SpawnBiomeTarget::Any)
    {
        glm::vec3 spawnFeet = findInitialSpawnFeetPosition(worldState, terrainGenerator, spawnProbePosition, colliderHeight);
        if (isSpawnFeetPositionSafe(worldState, spawnFeet, colliderHeight))
        {
            return spawnFeet;
        }
        if (const std::optional<glm::vec3> dryFeet = findNearbyDrySpawnFeetPosition(
                worldState,
                terrainGenerator,
                spawnProbePosition,
                colliderHeight);
            dryFeet.has_value())
        {
            return *dryFeet;
        }
        return spawnFeet;
    }

    constexpr int kSearchStep = 32;
    constexpr int kSearchRadius = 3072;
    constexpr int kMaxGeneratedTargetProbes = 224;
    std::optional<glm::vec3> bestSafeCandidate;
    int bestSafeCrowding = std::numeric_limits<int>::max();
    int bestSafePenalty = std::numeric_limits<int>::max();
    int generatedProbeCount = 0;
    bool generationBudgetExhausted = false;
    for (int radius = 0; radius <= kSearchRadius; radius += kSearchStep)
    {
        if (generationBudgetExhausted)
        {
            break;
        }
        for (int dz = -radius; dz <= radius; dz += kSearchStep)
        {
            if (generationBudgetExhausted)
            {
                break;
            }
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
                if (!isSpawnBiomeCoreArea(terrainGenerator, sampleX, sampleZ, spawnBiomeTarget))
                {
                    continue;
                }
                if (generatedProbeCount >= kMaxGeneratedTargetProbes)
                {
                    generationBudgetExhausted = true;
                    break;
                }
                ensureProbeGenerated(sampleX, sampleZ);
                ++generatedProbeCount;
                const glm::vec3 biomeProbe{
                    static_cast<float>(sampleX),
                    spawnProbePosition.y,
                    static_cast<float>(sampleZ),
                };
                const glm::vec3 candidateFeet = findInitialSpawnFeetPosition(
                    worldState,
                    terrainGenerator,
                    biomeProbe,
                    colliderHeight);
                if (isSpawnFeetPositionSafe(worldState, candidateFeet, colliderHeight))
                {
                    const int surfacePenalty = spawnSurfacePenalty(terrainGenerator, candidateFeet, spawnBiomeTarget);
                    const int crowding = spawnCrowdingScore(worldState, candidateFeet);
                    if (surfacePenalty == 0 && crowding <= 8)
                    {
                        return candidateFeet;
                    }
                    if (surfacePenalty < bestSafePenalty
                        || (surfacePenalty == bestSafePenalty && crowding < bestSafeCrowding))
                    {
                        bestSafePenalty = surfacePenalty;
                        bestSafeCrowding = crowding;
                        bestSafeCandidate = candidateFeet;
                    }
                }
            }
        }
    }

    if (bestSafeCandidate.has_value())
    {
        return *bestSafeCandidate;
    }

    const glm::vec3 fallbackFeet = findInitialSpawnFeetPosition(worldState, terrainGenerator, spawnProbePosition, colliderHeight);
    if (isSpawnFeetPositionSafe(worldState, fallbackFeet, colliderHeight))
    {
        return fallbackFeet;
    }
    if (const std::optional<glm::vec3> dryFeet = findNearbyDrySpawnFeetPosition(
            worldState,
            terrainGenerator,
            spawnProbePosition,
            colliderHeight);
        dryFeet.has_value())
    {
        return *dryFeet;
    }

    const int fallbackX = static_cast<int>(std::floor(spawnProbePosition.x));
    const int fallbackZ = static_cast<int>(std::floor(spawnProbePosition.z));
    const int surfaceY = terrainGenerator.surfaceHeightAt(fallbackX, fallbackZ);
    return glm::vec3(
        static_cast<float>(fallbackX) + 0.5f,
        static_cast<float>(surfaceY + 1),
        static_cast<float>(fallbackZ) + 0.5f);
}

bool Application::continueSingleplayerSpawnSearch(const float colliderHeight)
{
    const auto commitSpawnFeetPosition = [&](const glm::vec3& feetPosition)
    {
        playerFeetPosition_ = feetPosition;
        spawnFeetPosition_ = feetPosition;
        camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
        singleplayerLoadState_.spawnSearchActive = false;
        singleplayerLoadState_.spawnSearchOffsets.clear();
        singleplayerLoadState_.spawnSearchIndex = 0;
        singleplayerLoadState_.bestSpawnCandidate.reset();
        singleplayerLoadState_.bestSpawnCrowding = std::numeric_limits<int>::max();
        singleplayerLoadState_.bestSpawnPenalty = std::numeric_limits<int>::max();
    };

    if (!singleplayerLoadState_.playerStateLoaded && spawnBiomeTarget_ == SpawnBiomeTarget::Any)
    {
        commitSpawnFeetPosition(resolveSpawnFeetPosition(
            world_,
            terrainGenerator_,
            spawnPreset_,
            SpawnBiomeTarget::Any,
            camera_.position(),
            colliderHeight));
        return true;
    }

    if (singleplayerLoadState_.playerStateLoaded && spawnBiomeTarget_ == SpawnBiomeTarget::Any)
    {
        commitSpawnFeetPosition(resolveSpawnFeetPosition(
            world_,
            terrainGenerator_,
            spawnPreset_,
            spawnBiomeTarget_,
            camera_.position(),
            colliderHeight));
        return true;
    }

    constexpr int kSpawnProbeChunkRadius = 2;
    const std::size_t spawnProbeGenerationBudgetThisFrame = [this]()
    {
        if (smoothedFrameTimeMs_ >= 28.0f)
        {
            return static_cast<std::size_t>(1);
        }
        if (smoothedFrameTimeMs_ >= 20.0f)
        {
            return static_cast<std::size_t>(2);
        }
        if (smoothedFrameTimeMs_ >= 16.0f)
        {
            return static_cast<std::size_t>(3);
        }
        return static_cast<std::size_t>(5);
    }();
    std::size_t generatedProbeChunksThisFrame = 0;
    const auto tryGenerateProbeChunks = [&](const int worldX, const int worldZ)
    {
        if (generatedProbeChunksThisFrame >= spawnProbeGenerationBudgetThisFrame)
        {
            return false;
        }
        const std::size_t beforeChunkCount = world_.chunks().size();
        world_.generateMissingChunksAround(
            terrainGenerator_,
            world::worldToChunkCoord(worldX, worldZ),
            kSpawnProbeChunkRadius,
            spawnProbeGenerationBudgetThisFrame - generatedProbeChunksThisFrame);
        const std::size_t afterChunkCount = world_.chunks().size();
        if (afterChunkCount > beforeChunkCount)
        {
            generatedProbeChunksThisFrame += (afterChunkCount - beforeChunkCount);
        }
        return true;
    };

    if (!singleplayerLoadState_.spawnSearchActive)
    {
        singleplayerLoadState_.spawnSearchActive = true;
        singleplayerLoadState_.spawnProbePosition = preferredSpawnProbePosition(spawnPreset_, camera_.position());
        singleplayerLoadState_.spawnSearchOffsets = buildSpawnSearchOffsets();
        singleplayerLoadState_.spawnSearchIndex = 0;
        singleplayerLoadState_.bestSpawnCandidate.reset();
        singleplayerLoadState_.bestSpawnCrowding = std::numeric_limits<int>::max();
        singleplayerLoadState_.bestSpawnPenalty = std::numeric_limits<int>::max();
    }

    constexpr std::size_t kSpawnCandidatesPerFrame = 4;
    std::size_t processedCandidates = 0;
    while (singleplayerLoadState_.spawnSearchIndex < singleplayerLoadState_.spawnSearchOffsets.size()
           && processedCandidates < kSpawnCandidatesPerFrame)
    {
        const glm::ivec2 offset = singleplayerLoadState_.spawnSearchOffsets[singleplayerLoadState_.spawnSearchIndex++];
        ++processedCandidates;

        const int sampleX =
            static_cast<int>(std::floor(singleplayerLoadState_.spawnProbePosition.x)) + offset.x;
        const int sampleZ =
            static_cast<int>(std::floor(singleplayerLoadState_.spawnProbePosition.z)) + offset.y;
        if (!matchesSpawnBiomeTarget(terrainGenerator_.surfaceBiomeAt(sampleX, sampleZ), spawnBiomeTarget_))
        {
            continue;
        }
        if (!isSpawnBiomeCoreArea(terrainGenerator_, sampleX, sampleZ, spawnBiomeTarget_))
        {
            continue;
        }

        const world::ChunkCoord sampleChunk = world::worldToChunkCoord(sampleX, sampleZ);
        if (!world_.chunks().contains(sampleChunk))
        {
            if (!tryGenerateProbeChunks(sampleX, sampleZ))
            {
                // Defer remaining spawn probes to future frames to avoid long bootstrap stalls.
                break;
            }
            if (!world_.chunks().contains(sampleChunk))
            {
                continue;
            }
        }
        const glm::vec3 biomeProbe{
            static_cast<float>(sampleX),
            singleplayerLoadState_.spawnProbePosition.y,
            static_cast<float>(sampleZ),
        };
        const glm::vec3 candidateFeet =
            findInitialSpawnFeetPosition(world_, terrainGenerator_, biomeProbe, colliderHeight);
        if (!isSpawnFeetPositionSafe(world_, candidateFeet, colliderHeight))
        {
            continue;
        }

        const int surfacePenalty = spawnSurfacePenalty(terrainGenerator_, candidateFeet, spawnBiomeTarget_);
        const int crowding = spawnCrowdingScore(world_, candidateFeet);
        if (surfacePenalty == 0 && crowding <= 8)
        {
            commitSpawnFeetPosition(candidateFeet);
            return true;
        }
        if (surfacePenalty < singleplayerLoadState_.bestSpawnPenalty
            || (surfacePenalty == singleplayerLoadState_.bestSpawnPenalty
                && crowding < singleplayerLoadState_.bestSpawnCrowding))
        {
            singleplayerLoadState_.bestSpawnPenalty = surfacePenalty;
            singleplayerLoadState_.bestSpawnCrowding = crowding;
            singleplayerLoadState_.bestSpawnCandidate = candidateFeet;
        }
    }

    const float searchProgress = singleplayerLoadState_.spawnSearchOffsets.empty()
        ? 1.0f
        : static_cast<float>(singleplayerLoadState_.spawnSearchIndex)
            / static_cast<float>(singleplayerLoadState_.spawnSearchOffsets.size());
    singleplayerLoadState_.progress = std::clamp(0.45f + searchProgress * 0.10f, 0.0f, 0.55f);
    singleplayerLoadState_.label = fmt::format(
        "Searching for safe {} spawn... {}/{}",
        spawnBiomeTargetLabel(spawnBiomeTarget_),
        singleplayerLoadState_.spawnSearchIndex,
        singleplayerLoadState_.spawnSearchOffsets.size());

    if (singleplayerLoadState_.spawnSearchIndex < singleplayerLoadState_.spawnSearchOffsets.size())
    {
        return false;
    }

    if (singleplayerLoadState_.bestSpawnCandidate.has_value())
    {
        commitSpawnFeetPosition(*singleplayerLoadState_.bestSpawnCandidate);
        return true;
    }

    commitSpawnFeetPosition(resolveSpawnFeetPosition(
        world_,
        terrainGenerator_,
        spawnPreset_,
        spawnBiomeTarget_,
        camera_.position(),
        colliderHeight));
    return true;
}

bool isGroundedAtFeetPosition(
    const world::World& worldState,
    const glm::vec3& feetPosition,
    const float colliderHeight)
{
    glm::vec3 groundedProbe = feetPosition;
    groundedProbe.y -= kPlayerMovementSettings.groundProbeDistance;
    return game::collidesWithSolidBlock(worldState, playerAabbAt(groundedProbe, colliderHeight));
}

game::EnvironmentalHazards samplePlayerHazards(
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

void applyDefaultHotbarLoadout(HotbarSlots& hotbarSlots, std::size_t& selectedHotbarIndex)
{
    hotbarSlots.fill({});
    hotbarSlots[0].equippedItem = EquippedItem::WoodPickaxe;
    hotbarSlots[0].count = 1;
    hotbarSlots[0].blockType = world::BlockType::Air;
    hotbarSlots[1].equippedItem = EquippedItem::None;
    hotbarSlots[1].count = 1;
    hotbarSlots[1].blockType = world::BlockType::CraftingTable;
    hotbarSlots[7].equippedItem = EquippedItem::None;
    hotbarSlots[7].count = 32;
    hotbarSlots[7].blockType = world::BlockType::Cobblestone;
    hotbarSlots[8].equippedItem = EquippedItem::None;
    hotbarSlots[8].count = 16;
    hotbarSlots[8].blockType = world::BlockType::Torch;
    selectedHotbarIndex = 0;
}
}  // namespace vibecraft::app
