#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "vibecraft/app/Inventory.hpp"
#include "vibecraft/world/Block.hpp"

namespace vibecraft::app
{
struct ChatCommandContext
{
    glm::vec3 currentFeetPosition{0.0f};
    bool allowTeleport = true;
    bool globalWorldStateRequiresHostAuthority = false;
    int minWorldY = 0;
    int maxWorldY = 255;
};

struct ChatGiveStack
{
    world::BlockType blockType = world::BlockType::Air;
    EquippedItem equippedItem = EquippedItem::None;
    std::uint32_t count = 0;
    std::string displayLabel;
};

struct ChatCommandResult
{
    bool handled = false;
    bool succeeded = false;
    bool requiresHostAuthority = false;
    std::vector<std::string> feedbackLines;
    std::optional<glm::vec3> teleportFeetPosition;
    std::optional<glm::vec3> spawnFeetPosition;
    std::optional<bool> creativeModeEnabled;
    std::optional<float> dayNightElapsedSeconds;
    std::optional<float> weatherElapsedSeconds;
    std::vector<ChatGiveStack> giveStacks;
};

struct ChatAutocompleteResult
{
    bool applied = false;
    std::string updatedInput;
    std::size_t updatedCursorIndex = 0;
    std::string hintLine;
};

[[nodiscard]] ChatCommandResult executeChatCommand(
    std::string_view input,
    const ChatCommandContext& context);

[[nodiscard]] ChatAutocompleteResult autocompleteChatInput(std::string_view input, std::size_t cursorIndex);
}  // namespace vibecraft::app
