#pragma once

#include <glm/vec3.hpp>

namespace vibecraft::game
{
class Camera
{
  public:
    Camera();

    void setPosition(const glm::vec3& position);
    void setYawPitch(float yawDegrees, float pitchDegrees);
    void addYawPitch(float yawDeltaDegrees, float pitchDeltaDegrees);
    void moveLocal(const glm::vec3& localMotion);

    [[nodiscard]] const glm::vec3& position() const;
    [[nodiscard]] glm::vec3 forward() const;
    [[nodiscard]] glm::vec3 right() const;
    [[nodiscard]] glm::vec3 up() const;
    [[nodiscard]] float yawDegrees() const;
    [[nodiscard]] float pitchDegrees() const;

    /// Same basis as `forward()` (degrees: yaw around Y, pitch up).
    [[nodiscard]] static glm::vec3 forwardFromYawPitchDegrees(float yawDegrees, float pitchDegrees);

  private:
    glm::vec3 position_;
    float yawDegrees_ = -90.0f;
    float pitchDegrees_ = -20.0f;
};
}  // namespace vibecraft::game
