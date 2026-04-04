#include "vibecraft/app/ChatCommands.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace vibecraft::app
{
namespace
{
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

[[nodiscard]] std::vector<std::string> splitWhitespace(std::string_view value)
{
    std::vector<std::string> tokens;
    std::size_t cursor = 0;
    while (cursor < value.size())
    {
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])) != 0)
        {
            ++cursor;
        }
        if (cursor >= value.size())
        {
            break;
        }

        std::size_t end = cursor;
        while (end < value.size() && std::isspace(static_cast<unsigned char>(value[end])) == 0)
        {
            ++end;
        }
        tokens.emplace_back(value.substr(cursor, end - cursor));
        cursor = end;
    }
    return tokens;
}

[[nodiscard]] bool parseFloatToken(std::string_view token, float& value)
{
    if (token.empty())
    {
        return false;
    }

    std::string owned(token);
    char* end = nullptr;
    const float parsedValue = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || end == nullptr || *end != '\0')
    {
        return false;
    }

    value = parsedValue;
    return true;
}

[[nodiscard]] std::optional<float> parseTeleportCoordinate(std::string_view token, const float baseValue)
{
    if (token.empty())
    {
        return std::nullopt;
    }

    if (token.front() == '~')
    {
        if (token.size() == 1)
        {
            return baseValue;
        }

        float offset = 0.0f;
        if (!parseFloatToken(token.substr(1), offset))
        {
            return std::nullopt;
        }
        return baseValue + offset;
    }

    float absoluteValue = 0.0f;
    if (!parseFloatToken(token, absoluteValue))
    {
        return std::nullopt;
    }
    return absoluteValue;
}

[[nodiscard]] std::string lowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}
}  // namespace

ChatCommandResult executeChatCommand(std::string_view input, const ChatCommandContext& context)
{
    const std::string trimmedInput = trimCopy(std::string(input));
    if (trimmedInput.empty() || trimmedInput.front() != '/')
    {
        return {};
    }

    const std::vector<std::string> tokens = splitWhitespace(trimmedInput.substr(1));
    if (tokens.empty())
    {
        return {
            .handled = true,
            .succeeded = false,
            .feedback = "Command expected after '/'. Try /help.",
        };
    }

    const std::string command = lowerCopy(tokens.front());
    if (command == "help")
    {
        return {
            .handled = true,
            .succeeded = true,
            .feedback = "Commands: /help, /tp <x> <y> <z>, /tp <~dx> <~dy> <~dz>",
        };
    }

    if (command == "tp" || command == "teleport")
    {
        if (!context.allowTeleport)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedback = "Teleport commands are unavailable in multiplayer client mode.",
            };
        }
        if (tokens.size() != 4)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedback = "Usage: /tp <x> <y> <z> or /tp <~dx> <~dy> <~dz>",
            };
        }

        const std::optional<float> x = parseTeleportCoordinate(tokens[1], context.currentFeetPosition.x);
        const std::optional<float> y = parseTeleportCoordinate(tokens[2], context.currentFeetPosition.y);
        const std::optional<float> z = parseTeleportCoordinate(tokens[3], context.currentFeetPosition.z);
        if (!x.has_value() || !y.has_value() || !z.has_value())
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedback = "Invalid teleport coordinates. Example: /tp 10 80 -4 or /tp ~ ~1 ~-3",
            };
        }

        const float maxFeetY = static_cast<float>(context.maxWorldY) + 1.0f;
        if (*y < static_cast<float>(context.minWorldY) || *y > maxFeetY)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedback = fmt::format(
                    "Teleport Y must stay between {} and {:.0f}.",
                    context.minWorldY,
                    maxFeetY),
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .feedback = fmt::format("Teleported to {:.2f} {:.2f} {:.2f}.", *x, *y, *z),
            .teleportFeetPosition = glm::vec3(*x, *y, *z),
        };
    }

    return {
        .handled = true,
        .succeeded = false,
        .feedback = fmt::format("Unknown command: /{}. Try /help.", command),
    };
}
}  // namespace vibecraft::app
