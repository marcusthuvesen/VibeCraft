#include "vibecraft/app/PauseMenuInputPolicy.hpp"

namespace vibecraft::app
{
bool shouldAllowPausedPointerInputWhileUnfocused(
    const bool mouseCaptured,
    const bool leftMousePressed,
    const bool leftMouseClicked)
{
    return !mouseCaptured && (leftMousePressed || leftMouseClicked);
}

bool shouldBlockPausedMenuPointerActivation(
    const bool awaitingMouseRelease,
    const bool leftMousePressed,
    const bool leftMouseClicked)
{
    return awaitingMouseRelease && (leftMousePressed || leftMouseClicked);
}
}  // namespace vibecraft::app
