#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>

namespace vibecraft::render
{
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
    void renderFrame(const FrameDebugData& frameDebugData);

  private:
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    bool initialized_ = false;
};
}  // namespace vibecraft::render
