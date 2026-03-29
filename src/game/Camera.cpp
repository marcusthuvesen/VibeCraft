#include "vibecraft/game/Camera.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace vibecraft::game
{
namespace
{
constexpr float kMaxPitchDegrees = 89.0f;
}

Camera::Camera() : position_(0.0f, 28.0f, 18.0f) {}

void Camera::setPosition(const glm::vec3& position)
{
    position_ = position;
}

void Camera::setYawPitch(const float yawDegrees, const float pitchDegrees)
{
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = glm::clamp(pitchDegrees, -kMaxPitchDegrees, kMaxPitchDegrees);
}

void Camera::addYawPitch(const float yawDeltaDegrees, const float pitchDeltaDegrees)
{
    yawDegrees_ += yawDeltaDegrees;
    pitchDegrees_ = glm::clamp(pitchDegrees_ + pitchDeltaDegrees, -kMaxPitchDegrees, kMaxPitchDegrees);
}

void Camera::moveLocal(const glm::vec3& localMotion)
{
    position_ += right() * localMotion.x;
    position_ += up() * localMotion.y;
    position_ += forward() * localMotion.z;
}

const glm::vec3& Camera::position() const
{
    return position_;
}

glm::vec3 Camera::forward() const
{
    const float yawRadians = glm::radians(yawDegrees_);
    const float pitchRadians = glm::radians(pitchDegrees_);
    const glm::vec3 direction(
        glm::cos(yawRadians) * glm::cos(pitchRadians),
        glm::sin(pitchRadians),
        glm::sin(yawRadians) * glm::cos(pitchRadians));
    return glm::normalize(direction);
}

glm::vec3 Camera::right() const
{
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::up() const
{
    return glm::normalize(glm::cross(right(), forward()));
}

float Camera::yawDegrees() const
{
    return yawDegrees_;
}

float Camera::pitchDegrees() const
{
    return pitchDegrees_;
}
}  // namespace vibecraft::game
