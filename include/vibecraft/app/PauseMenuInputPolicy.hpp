#pragma once

namespace vibecraft::app
{
[[nodiscard]] bool shouldAllowPausedPointerInputWhileUnfocused(
    bool mouseCaptured,
    bool leftMousePressed,
    bool leftMouseClicked);

[[nodiscard]] bool shouldBlockPausedMenuPointerActivation(
    bool awaitingMouseRelease,
    bool leftMousePressed,
    bool leftMouseClicked);
}  // namespace vibecraft::app
