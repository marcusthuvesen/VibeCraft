#include "vibecraft/app/Application.hpp"

#include <fmt/format.h>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "vibecraft/app/ApplicationConfig.hpp"
#include "vibecraft/app/ApplicationSpawnHelpers.hpp"
#include "vibecraft/app/ChatCommands.hpp"

namespace vibecraft::app
{
namespace
{
constexpr std::size_t kMaxChatHistoryLines = 48;
constexpr std::size_t kMaxSubmittedChatInputs = 32;
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

[[nodiscard]] std::string formatChatTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    std::ostringstream stream;
    stream << "[" << std::put_time(&localTime, "%H:%M") << "]";
    return stream.str();
}

template <typename ChatStateT>
void pushSubmittedChatInput(ChatStateT& chatState, const std::string& input)
{
    if (input.empty())
    {
        return;
    }
    if (chatState.submittedInputs.empty() || chatState.submittedInputs.back() != input)
    {
        chatState.submittedInputs.push_back(input);
        if (chatState.submittedInputs.size() > kMaxSubmittedChatInputs)
        {
            chatState.submittedInputs.erase(chatState.submittedInputs.begin());
        }
    }
    chatState.submittedHistoryIndex.reset();
    chatState.submittedHistoryDraft.clear();
}

template <typename ChatStateT>
void moveChatCursor(ChatStateT& chatState, const int delta)
{
    if (delta < 0)
    {
        const std::size_t distance = static_cast<std::size_t>(-delta);
        chatState.cursorIndex = chatState.cursorIndex > distance ? chatState.cursorIndex - distance : 0;
        return;
    }
    chatState.cursorIndex = std::min(chatState.cursorIndex + static_cast<std::size_t>(delta), chatState.inputBuffer.size());
}

template <typename ChatStateT>
void recallSubmittedInput(ChatStateT& chatState, const bool older)
{
    if (chatState.submittedInputs.empty())
    {
        return;
    }

    if (!chatState.submittedHistoryIndex.has_value())
    {
        chatState.submittedHistoryDraft = chatState.inputBuffer;
        chatState.submittedHistoryIndex = older ? chatState.submittedInputs.size() - 1 : 0;
    }
    else if (older)
    {
        if (*chatState.submittedHistoryIndex > 0)
        {
            --(*chatState.submittedHistoryIndex);
        }
    }
    else if (*chatState.submittedHistoryIndex + 1 < chatState.submittedInputs.size())
    {
        ++(*chatState.submittedHistoryIndex);
    }
    else
    {
        chatState.submittedHistoryIndex.reset();
        chatState.inputBuffer = chatState.submittedHistoryDraft;
        chatState.cursorIndex = chatState.inputBuffer.size();
        return;
    }

    chatState.inputBuffer = chatState.submittedInputs[*chatState.submittedHistoryIndex];
    chatState.cursorIndex = chatState.inputBuffer.size();
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
    chatState_.cursorIndex = chatState_.inputBuffer.size();
    chatState_.hintLine.clear();
    chatState_.submittedHistoryIndex.reset();
    chatState_.submittedHistoryDraft.clear();
    window_.setTextInputActive(true);
}

void Application::closeChat(const bool clearInput)
{
    chatState_.open = false;
    chatState_.hintLine.clear();
    chatState_.submittedHistoryIndex.reset();
    chatState_.submittedHistoryDraft.clear();
    if (clearInput)
    {
        chatState_.inputBuffer.clear();
        chatState_.cursorIndex = 0;
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
        .timestampLabel = formatChatTimestamp(),
        .createdAtSeconds = sessionPlayTimeSeconds_,
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

void Application::applyChatCommandResult(const ChatCommandResult& result)
{
    if (result.teleportFeetPosition.has_value())
    {
        teleportPlayerToFeetPosition(*result.teleportFeetPosition);
    }
    if (result.spawnFeetPosition.has_value())
    {
        spawnFeetPosition_ = *result.spawnFeetPosition;
    }
    if (result.creativeModeEnabled.has_value())
    {
        creativeModeEnabled_ = *result.creativeModeEnabled;
        if (creativeModeEnabled_)
        {
            applyCreativeInventoryLoadout(hotbarSlots_, bagSlots_, selectedHotbarIndex_);
        }
    }
    if (result.dayNightElapsedSeconds.has_value())
    {
        dayNightCycle_.setElapsedSeconds(*result.dayNightElapsedSeconds);
    }
    if (result.weatherElapsedSeconds.has_value())
    {
        weatherSystem_.setElapsedSeconds(*result.weatherElapsedSeconds);
    }
    for (const ChatGiveStack& stack : result.giveStacks)
    {
        std::uint32_t givenCount = 0;
        for (; givenCount < stack.count; ++givenCount)
        {
            const bool added = stack.equippedItem != EquippedItem::None
                ? addEquippedItemToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    stack.equippedItem,
                    selectedHotbarIndex_,
                    InventorySelectionBehavior::PreserveCurrent)
                : addBlockToInventory(
                    hotbarSlots_,
                    bagSlots_,
                    stack.blockType,
                    selectedHotbarIndex_,
                    InventorySelectionBehavior::PreserveCurrent);
            if (!added)
            {
                break;
            }
        }

        if (givenCount == stack.count)
        {
            appendChatLine(fmt::format("Gave {} x {}.", givenCount, stack.displayLabel), false);
        }
        else if (givenCount > 0)
        {
            appendChatLine(
                fmt::format("Inventory filled after {} / {} x {}.", givenCount, stack.count, stack.displayLabel),
                true);
        }
        else
        {
            appendChatLine(fmt::format("Inventory is full. Could not give {}.", stack.displayLabel), true);
        }
    }
    for (const std::string& feedbackLine : result.feedbackLines)
    {
        appendChatLine(feedbackLine, !result.succeeded);
    }
}

void Application::requestHostCommandExecution(const std::string& commandText)
{
    if (clientSession_ == nullptr || !clientSession_->connected())
    {
        appendChatLine("Unable to send command to host right now.", true);
        return;
    }
    clientSession_->sendCommandRequest(commandText, networkServerTick_);
    appendChatLine(fmt::format("Sent {} to host.", commandText), false);
}

void Application::handleHostRequestedCommand(const std::uint16_t clientId, const std::string& commandText)
{
    const ChatCommandResult result = executeChatCommand(
        commandText,
        ChatCommandContext{
            .currentFeetPosition = playerFeetPosition_,
            .allowTeleport = true,
            .globalWorldStateRequiresHostAuthority = false,
            .minWorldY = world::kWorldMinY,
            .maxWorldY = world::kWorldMaxY,
        });

    std::string feedback = "Unknown command.";
    bool isError = !result.succeeded;
    if (!result.handled)
    {
        feedback = "Unknown command.";
        isError = true;
    }
    else if (!result.requiresHostAuthority)
    {
        feedback = "Run that command locally. Only shared world commands go through the host.";
        isError = true;
    }
    else
    {
        applyChatCommandResult(result);
        if (!result.feedbackLines.empty())
        {
            feedback = result.feedbackLines.front();
        }
        else
        {
            feedback = "Host command applied.";
        }
        isError = !result.succeeded;
    }

    if (hostSession_ != nullptr)
    {
        hostSession_->sendCommandFeedback(clientId, feedback, isError);
    }
}

void Application::submitChatInput()
{
    const std::string submittedLine = trimCopy(chatState_.inputBuffer);
    pushSubmittedChatInput(chatState_, submittedLine);
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
                .allowTeleport = true,
                .globalWorldStateRequiresHostAuthority = multiplayerMode_ == MultiplayerRuntimeMode::Client,
                .minWorldY = world::kWorldMinY,
                .maxWorldY = world::kWorldMaxY,
            });
        if (result.requiresHostAuthority && multiplayerMode_ == MultiplayerRuntimeMode::Client)
        {
            requestHostCommandExecution(submittedLine);
            return;
        }
        applyChatCommandResult(result);
        return;
    }

    appendChatLine(fmt::format("<You> {}", submittedLine), false);
}

void Application::processPlayingChatInput(const ChatInputIntent& intent)
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

    bool editedBuffer = false;
    if (intent.historyPrev)
    {
        recallSubmittedInput(chatState_, true);
    }
    if (intent.historyNext)
    {
        recallSubmittedInput(chatState_, false);
    }
    if (intent.moveCursorLeft)
    {
        moveChatCursor(chatState_, -1);
    }
    if (intent.moveCursorRight)
    {
        moveChatCursor(chatState_, 1);
    }
    if (intent.moveCursorHome)
    {
        chatState_.cursorIndex = 0;
    }
    if (intent.moveCursorEnd)
    {
        chatState_.cursorIndex = chatState_.inputBuffer.size();
    }

    if (inputState_.backspacePressed && chatState_.cursorIndex > 0 && !chatState_.inputBuffer.empty())
    {
        chatState_.inputBuffer.erase(chatState_.cursorIndex - 1, 1);
        --chatState_.cursorIndex;
        editedBuffer = true;
    }
    if (intent.deleteForward && chatState_.cursorIndex < chatState_.inputBuffer.size())
    {
        chatState_.inputBuffer.erase(chatState_.cursorIndex, 1);
        editedBuffer = true;
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
            chatState_.inputBuffer.insert(chatState_.cursorIndex, 1, character);
            ++chatState_.cursorIndex;
            editedBuffer = true;
        }
    }

    if (intent.autocomplete)
    {
        const ChatAutocompleteResult completion =
            autocompleteChatInput(chatState_.inputBuffer, chatState_.cursorIndex);
        if (completion.applied)
        {
            chatState_.inputBuffer = completion.updatedInput.substr(0, kMaxChatInputChars);
            chatState_.cursorIndex = std::min(completion.updatedCursorIndex, chatState_.inputBuffer.size());
        }
        chatState_.hintLine = completion.hintLine;
    }
    else if (editedBuffer)
    {
        chatState_.hintLine.clear();
    }

    if (intent.submit)
    {
        submitChatInput();
    }
}
}  // namespace vibecraft::app
