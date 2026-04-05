#include "vibecraft/app/StartupFlow.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::string trimAsciiWhitespace(std::string_view value)
{
    const auto isSpace = [](const unsigned char ch)
    {
        return std::isspace(ch) != 0;
    };

    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    {
        value.remove_suffix(1);
    }

    return std::string(value);
}
}  // namespace

bool envFlagEnabled(const std::string_view value)
{
    std::string normalized = trimAsciiWhitespace(value);
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

SingleplayerStartAction resolveSingleplayerStartAction(
    const SingleplayerStartRequest request,
    const bool hasSavedWorlds)
{
    if (request == SingleplayerStartRequest::CreateNewWorld)
    {
        return SingleplayerStartAction::CreateAndStartWorld;
    }

    if (hasSavedWorlds)
    {
        return SingleplayerStartAction::StartSelectedWorld;
    }

    return SingleplayerStartAction::MissingSavedWorld;
}
}  // namespace vibecraft::app
