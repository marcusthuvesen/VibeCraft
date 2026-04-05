#include <doctest/doctest.h>

#include "vibecraft/app/PauseMenuInputPolicy.hpp"

using vibecraft::app::shouldAllowPausedPointerInputWhileUnfocused;
using vibecraft::app::shouldBlockPausedMenuPointerActivation;

TEST_CASE("paused menu accepts pointer click after focus loss when mouse is released")
{
    CHECK(shouldAllowPausedPointerInputWhileUnfocused(false, true, false));
    CHECK(shouldAllowPausedPointerInputWhileUnfocused(false, false, true));
    CHECK_FALSE(shouldAllowPausedPointerInputWhileUnfocused(true, true, true));
    CHECK_FALSE(shouldAllowPausedPointerInputWhileUnfocused(false, false, false));
}

TEST_CASE("pause menu blocks click activation until mouse is released after esc")
{
    CHECK(shouldBlockPausedMenuPointerActivation(true, true, false));
    CHECK(shouldBlockPausedMenuPointerActivation(true, false, true));
    CHECK_FALSE(shouldBlockPausedMenuPointerActivation(true, false, false));
    CHECK_FALSE(shouldBlockPausedMenuPointerActivation(false, true, true));
}
