#pragma once

namespace vibecraft::app
{
/// When the window is unfocused, still run one input tick if this returns true (pause menu, or
/// inventory / workbench / chest / furnace crafting UI — same pointer + !capture rule).
[[nodiscard]] bool shouldAllowPausedPointerInputWhileUnfocused(
    bool mouseCaptured,
    bool leftMousePressed,
    bool leftMouseClicked);

[[nodiscard]] bool shouldBlockPausedMenuPointerActivation(
    bool awaitingMouseRelease,
    bool leftMousePressed,
    bool leftMouseClicked);
}  // namespace vibecraft::app
