#pragma once

#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace vibecraft::app
{
struct ChatCommandContext
{
    glm::vec3 currentFeetPosition{0.0f};
    bool allowTeleport = true;
    int minWorldY = 0;
    int maxWorldY = 255;
};

struct ChatCommandResult
{
    bool handled = false;
    bool succeeded = false;
    std::string feedback;
    std::optional<glm::vec3> teleportFeetPosition;
};

[[nodiscard]] ChatCommandResult executeChatCommand(
    std::string_view input,
    const ChatCommandContext& context);
}  // namespace vibecraft::app
