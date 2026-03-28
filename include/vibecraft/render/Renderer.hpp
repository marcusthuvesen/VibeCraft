#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace vibecraft::render
{
struct CameraFrameData
{
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 60.0f;
    float nearClip = 0.1f;
    float farClip = 1024.0f;
};

struct SceneMeshData
{
    std::uint64_t id = 0;
    struct Vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        std::uint32_t abgr = 0xffffffff;
    };

    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

struct FrameDebugData
{
    std::uint32_t chunkCount = 0;
    std::uint32_t dirtyChunkCount = 0;
    std::uint32_t totalFaces = 0;
    std::uint32_t residentChunkCount = 0;
    glm::vec3 cameraPosition{0.0f};
    bool hasTarget = false;
    glm::ivec3 targetBlock{0, 0, 0};
    std::string statusLine;
};

class Renderer
{
  public:
    bool initialize(void* nativeWindowHandle, std::uint32_t width, std::uint32_t height);
    void shutdown();
    void resize(std::uint32_t width, std::uint32_t height);
    void updateSceneMeshes(
        const std::vector<SceneMeshData>& sceneMeshes,
        const std::vector<std::uint64_t>& removedMeshIds);
    void renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);

  private:
    struct SceneGpuMesh
    {
        std::uint16_t vertexBufferHandle = UINT16_MAX;
        std::uint16_t indexBufferHandle = UINT16_MAX;
        std::uint32_t indexCount = 0;
        glm::vec3 boundsMin{0.0f};
        glm::vec3 boundsMax{0.0f};
    };

    void destroySceneMesh(std::uint64_t sceneMeshId);
    void destroySceneMeshes();

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool initialized_ = false;
    std::uint16_t chunkProgramHandle_ = UINT16_MAX;
    std::unordered_map<std::uint64_t, SceneGpuMesh> sceneMeshes_;
};
}  // namespace vibecraft::render
