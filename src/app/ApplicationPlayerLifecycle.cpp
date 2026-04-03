#include "vibecraft/app/Application.hpp"

#include <glm/vec3.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"

namespace vibecraft::app
{
void Application::respawnPlayer()
{
    playerFeetPosition_ = spawnFeetPosition_;
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    autoJumpCooldownSeconds_ = 0.0f;
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
}  // namespace vibecraft::app
