#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cctype>
#include <string>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ChatCommands.hpp"

namespace vibecraft::app
{
namespace
{
constexpr std::size_t kMaxChatHistoryLines = 6;
constexpr std::size_t kMaxChatInputChars = 160;

[[nodiscard]] std::string trimCopy(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.pop_back();
    }
    return value;
}
}  // namespace

void Application::openChat(const std::string& initialText)
{
    if (gameScreen_ != GameScreen::Playing || craftingMenuState_.active)
    {
        return;
    }

    chatState_.open = true;
    chatState_.inputBuffer = initialText.substr(0, kMaxChatInputChars);
    window_.setTextInputActive(true);
}

void Application::closeChat(const bool clearInput)
{
    chatState_.open = false;
    if (clearInput)
    {
        chatState_.inputBuffer.clear();
    }
    window_.setTextInputActive(false);
}

void Application::appendChatLine(const std::string& text, const bool isError)
{
    const std::string trimmedText = trimCopy(text);
    if (trimmedText.empty())
    {
        return;
    }

    if (chatState_.history.size() >= kMaxChatHistoryLines)
    {
        chatState_.history.erase(chatState_.history.begin());
    }

    chatState_.history.push_back(ChatLine{
        .text = trimmedText,
        .isError = isError,
    });
}

void Application::teleportPlayerToFeetPosition(const glm::vec3& feetPosition)
{
    playerFeetPosition_ = feetPosition;
    verticalVelocity_ = 0.0f;
    accumulatedFallDistance_ = 0.0f;
    jumpWasHeld_ = false;
    autoJumpCooldownSeconds_ = 0.0f;
    activeMiningState_ = {};
    isGrounded_ =
        isGroundedAtFeetPosition(world_, playerFeetPosition_, kPlayerMovementSettings.standingColliderHeight);
    playerHazards_ = samplePlayerHazards(
        world_,
        playerFeetPosition_,
        kPlayerMovementSettings.standingColliderHeight,
        kPlayerMovementSettings.standingEyeHeight);
    camera_.setPosition(playerFeetPosition_ + glm::vec3(0.0f, kPlayerMovementSettings.standingEyeHeight, 0.0f));
}

void Application::submitChatInput()
{
    const std::string submittedLine = trimCopy(chatState_.inputBuffer);
    closeChat(true);
    if (submittedLine.empty())
    {
        return;
    }

    if (!submittedLine.empty() && submittedLine.front() == '/')
    {
        const ChatCommandResult result = executeChatCommand(
            submittedLine,
            ChatCommandContext{
                .currentFeetPosition = playerFeetPosition_,
                .allowTeleport = multiplayerMode_ != MultiplayerRuntimeMode::Client,
                .minWorldY = world::kWorldMinY,
                .maxWorldY = world::kWorldMaxY,
            });
        if (result.teleportFeetPosition.has_value())
        {
            teleportPlayerToFeetPosition(*result.teleportFeetPosition);
        }
        appendChatLine(result.feedback, !result.succeeded);
        return;
    }

    appendChatLine(fmt::format("<You> {}", submittedLine), false);
}

void Application::processPlayingChatInput(const bool submitPressed)
{
    if (!chatState_.open)
    {
        return;
    }

    if (inputState_.escapePressed)
    {
        closeChat(true);
        return;
    }

    if (inputState_.backspacePressed && !chatState_.inputBuffer.empty())
    {
        chatState_.inputBuffer.pop_back();
    }

    if (!inputState_.textInputUtf8.empty())
    {
        for (const char character : inputState_.textInputUtf8)
        {
            if (static_cast<unsigned char>(character) < 32U)
            {
                continue;
            }
            if (chatState_.inputBuffer.size() >= kMaxChatInputChars)
            {
                break;
            }
            chatState_.inputBuffer.push_back(character);
        }
    }

    if (submitPressed)
    {
        submitChatInput();
    }
}
}  // namespace vibecraft::app
