#include "vibecraft/app/Application.hpp"

#include <glm/vec3.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationEquipment.hpp"
#include "vibecraft/app/ApplicationOxygenRuntime.hpp"
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
    oxygenSystem_.resetForNewGame();
    syncOxygenSystemFromEquipmentSlot(equipmentSlots_, oxygenSystem_, true);
    syncOxygenEquipmentSlotFromSystem(equipmentSlots_, oxygenSystem_);
    if (ensureStarterRelayAvailable(hotbarSlots_, bagSlots_, selectedHotbarIndex_))
    {
        respawnNotice_ = "Emergency Oxygen Generator restocked.";
    }
    playerHazards_ = samplePlayerHazards(
        world_,
        playerFeetPosition_,
        kPlayerMovementSettings.standingColliderHeight,
        kPlayerMovementSettings.standingEyeHeight);
    playerOxygenEnvironment_ =
        refreshPlayerOxygenEnvironment(world_, terrainGenerator_, playerFeetPosition_, playerHazards_, creativeModeEnabled_);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
}
}  // namespace vibecraft::app
