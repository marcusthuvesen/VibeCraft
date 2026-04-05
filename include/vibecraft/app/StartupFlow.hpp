#pragma once

#include <string_view>

namespace vibecraft::app
{
enum class SingleplayerStartRequest
{
    LoadSavedWorld,
    CreateNewWorld,
};

enum class SingleplayerStartAction
{
    StartSelectedWorld,
    CreateAndStartWorld,
    MissingSavedWorld,
};

[[nodiscard]] bool envFlagEnabled(std::string_view value);

[[nodiscard]] SingleplayerStartAction resolveSingleplayerStartAction(
    SingleplayerStartRequest request,
    bool hasSavedWorlds);
}  // namespace vibecraft::app
