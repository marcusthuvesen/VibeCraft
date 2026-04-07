#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationMovementHelpers.hpp"
#include "vibecraft/app/WaterMovement.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ApplicationSurvival.hpp"

namespace vibecraft::app
{
bool Application::processPlayingMovementInput(const float deltaTimeSeconds, const bool craftingKeyPressed)
{
    if (autoJumpCooldownSeconds_ > 0.0f)
    {
        autoJumpCooldownSeconds_ = std::max(0.0f, autoJumpCooldownSeconds_ - deltaTimeSeconds);
    }

    // First-person held item swing: phase 0 = rest, 0..1 = one swing cycle (matches sin(phase * pi) in renderer).
    constexpr float kHeldItemSwingSpeed = 4.6f;
    if (inputState_.leftMousePressed || inputState_.rightMousePressed)
    {
        heldItemSwing_ = std::max(deltaTimeSeconds * kHeldItemSwingSpeed, 0.001f);
    }
    else if (heldItemSwing_ > 0.0f)
    {
        heldItemSwing_ += deltaTimeSeconds * kHeldItemSwingSpeed;
        if (heldItemSwing_ >= 1.0f)
        {
            heldItemSwing_ = 0.0f;
        }
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
        return true;
    }

    if (!creativeModeEnabled_)
    {
        creativeFlightActive_ = false;
        creativeFlightToggleWindowSeconds_ = 0.0f;
    }
    else if (creativeFlightToggleWindowSeconds_ > 0.0f)
    {
        creativeFlightToggleWindowSeconds_ =
            std::max(0.0f, creativeFlightToggleWindowSeconds_ - deltaTimeSeconds);
    }

    const bool descendHeld = inputState_.isKeyDown(SDL_SCANCODE_LSHIFT);
    const bool sneaking = descendHeld && !creativeFlightActive_;
    const bool sprinting =
        (inputState_.isKeyDown(SDL_SCANCODE_LCTRL) || inputState_.isKeyDown(SDL_SCANCODE_RCTRL))
        && !descendHeld;
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
    if (creativeFlightActive_)
    {
        currentMoveSpeed *= 1.16f;
    }
    const game::EnvironmentalHazards movementHazardsBeforeStep =
        samplePlayerHazards(world_, playerFeetPosition_, colliderHeight, eyeHeight);
    const bool inWaterForMovement = movementHazardsBeforeStep.bodyInWater;
    const bool headSubmergedInWater = movementHazardsBeforeStep.headSubmergedInWater;
    if (inWaterForMovement)
    {
        currentMoveSpeed *= headSubmergedInWater ? kPlayerMovementSettings.waterSubmergedMoveSpeedMultiplier
                                                 : kPlayerMovementSettings.waterSurfaceMoveSpeedMultiplier;
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

    glm::vec3 horizontalDisplacement(0.0f);
    float swimVerticalFromLook = 0.0f;
    if (inWaterForMovement)
    {
        WaterSwimInput swimIn{};
        swimIn.cameraForward = camera_.forward();
        swimIn.cameraRight = camera_.right();
        swimIn.localMotion = localMotion;
        swimIn.headSubmergedInWater = headSubmergedInWater;
        WaterSwimOutput swimOut{};
        computeWaterSwimDisplacement(swimIn, swimOut);
        horizontalDisplacement = swimOut.horizontalDisplacement;
        swimVerticalFromLook = swimOut.swimVerticalFromLook;
    }
    else
    {
        horizontalDisplacement =
            horizontalForward * localMotion.z + horizontalRight * localMotion.x;
    }

    const float horizontalMoveDistance =
        glm::length(glm::vec2(horizontalDisplacement.x, horizontalDisplacement.z));

    const glm::vec3 feetBeforeHorizontal = playerFeetPosition_;

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

    const glm::vec2 wishXZ(horizontalDisplacement.x, horizontalDisplacement.z);
    const float wishLen = glm::length(wishXZ);
    const glm::vec2 movedXZ(
        playerFeetPosition_.x - feetBeforeHorizontal.x,
        playerFeetPosition_.z - feetBeforeHorizontal.z);
    const float movedLen = glm::length(movedXZ);
    const bool tryingToMoveHorizontally = wishLen > 0.0001f;
    const bool stuckLowProgress =
        tryingToMoveHorizontally && movedLen < wishLen * 0.42f;

    const bool wasGrounded = isGrounded_;
    isGrounded_ = isGroundedAtFeetPosition(world_, playerFeetPosition_, colliderHeight);

    const bool jumpHeld = inputState_.isKeyDown(SDL_SCANCODE_SPACE);
    if (creativeModeEnabled_ && jumpHeld && !jumpWasHeld_)
    {
        constexpr float kCreativeFlightToggleWindowSeconds = 0.28f;
        if (creativeFlightToggleWindowSeconds_ > 0.0f)
        {
            creativeFlightActive_ = !creativeFlightActive_;
            creativeFlightToggleWindowSeconds_ = 0.0f;
            verticalVelocity_ = 0.0f;
            accumulatedFallDistance_ = 0.0f;
            autoJumpCooldownSeconds_ = 0.0f;
            if (creativeFlightActive_)
            {
                isGrounded_ = false;
            }
        }
        else
        {
            creativeFlightToggleWindowSeconds_ = kCreativeFlightToggleWindowSeconds;
        }
    }

    if (!creativeFlightActive_ && !inWaterForMovement && jumpHeld && !jumpWasHeld_ && isGrounded_)
    {
        verticalVelocity_ = kPlayerMovementSettings.jumpVelocity;
        isGrounded_ = false;
        soundEffects_.playPlayerJump();
    }
    else if (
        !creativeFlightActive_
        &&
        !inWaterForMovement && !sneaking && autoJumpCooldownSeconds_ <= 0.0f && stuckLowProgress && isGrounded_
        && canAutoJumpOneBlockLedge(world_, playerFeetPosition_, wishXZ, colliderHeight))
    {
        verticalVelocity_ = kPlayerMovementSettings.jumpVelocity;
        isGrounded_ = false;
        soundEffects_.playPlayerJump();
        autoJumpCooldownSeconds_ = 0.32f;
    }
    jumpWasHeld_ = jumpHeld;

    const game::Aabb playerBodyForVines = playerAabbAt(playerFeetPosition_, colliderHeight);
    const bool touchingClimbableVines =
        !inWaterForMovement
        && (aabbTouchesBlockType(world_, playerBodyForVines, world::BlockType::Vines)
            || aabbTouchesBlockType(world_, playerBodyForVines, world::BlockType::Ladder));

    if (creativeFlightActive_)
    {
        if (jumpHeld)
        {
            verticalVelocity_ = currentMoveSpeed;
        }
        else if (descendHeld)
        {
            verticalVelocity_ = -currentMoveSpeed;
        }
        else
        {
            verticalVelocity_ = 0.0f;
        }
    }
    else if (inWaterForMovement)
    {
        WaterVerticalInput waterVert{};
        waterVert.settings = &kPlayerMovementSettings;
        waterVert.headSubmergedInWater = headSubmergedInWater;
        waterVert.jumpHeld = jumpHeld;
        waterVert.sneaking = sneaking;
        waterVert.deltaTimeSeconds = deltaTimeSeconds;
        integrateWaterVerticalVelocity(verticalVelocity_, waterVert);
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
    if (!creativeFlightActive_ && inWaterForMovement)
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
        && (aabbTouchesBlockType(
                world_,
                playerAabbAt(playerFeetPosition_, colliderHeight),
                world::BlockType::Vines)
            || aabbTouchesBlockType(
                world_,
                playerAabbAt(playerFeetPosition_, colliderHeight),
                world::BlockType::Ladder));
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

    const bool landedThisFrame = !creativeFlightActive_ && !wasGrounded && isGrounded_;
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
        if (!creativeModeEnabled_ && !playerHazards_.bodyInWater)
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

    if (!creativeModeEnabled_ && !creativeFlightActive_ && isGrounded_ && !playerHazards_.bodyInWater
        && horizontalMoveDistance > 0.0001f)
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
    else if (!creativeModeEnabled_ && playerHazards_.bodyInWater)
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
    if (creativeFlightActive_)
    {
        accumulatedFallDistance_ = 0.0f;
    }

    if (creativeModeEnabled_)
    {
        playerVitals_.reset();
    }
    else
    {
        const float healthBeforeTick = playerVitals_.health();
        playerVitals_.tickEnvironment(deltaTimeSeconds, playerHazards_);
        playerTookDamageThisFrame = playerVitals_.health() + 0.001f < healthBeforeTick;
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

    return false;
}
}  // namespace vibecraft::app
