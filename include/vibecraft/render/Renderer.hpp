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
    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> indices;
    std::uint32_t abgr = 0xff90caf9;
};

struct FrameDebugData
{
    std::uint32_t chunkCount = 0;
    std::uint32_t dirtyChunkCount = 0;
    std::uint32_t totalFaces = 0;
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
    void replaceSceneMeshes(const std::vector<SceneMeshData>& sceneMeshes);
    void renderFrame(const FrameDebugData& frameDebugData, const CameraFrameData& cameraFrameData);

  private:
    void destroySceneMeshes();

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool initialized_ = false;
    std::unordered_map<std::uint64_t, std::uint16_t> sceneMeshHandles_;
    std::unordered_map<std::uint64_t, std::uint32_t> sceneMeshColors_;
};
}  // namespace vibecraft::render
