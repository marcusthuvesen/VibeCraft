#include <doctest/doctest.h>

#include "vibecraft/app/ChatCommands.hpp"

namespace
{
using vibecraft::app::ChatCommandContext;
using vibecraft::app::ChatCommandResult;
using vibecraft::app::executeChatCommand;
}  // namespace

TEST_CASE("tp command accepts absolute coordinates")
{
    const ChatCommandResult result = executeChatCommand(
        "/tp 12 80 -4.5",
        ChatCommandContext{
            .currentFeetPosition = {1.0f, 64.0f, 2.0f},
            .allowTeleport = true,
            .minWorldY = -64,
            .maxWorldY = 255,
        });

    CHECK(result.handled);
    CHECK(result.succeeded);
    REQUIRE(result.teleportFeetPosition.has_value());
    CHECK(result.teleportFeetPosition->x == doctest::Approx(12.0f));
    CHECK(result.teleportFeetPosition->y == doctest::Approx(80.0f));
    CHECK(result.teleportFeetPosition->z == doctest::Approx(-4.5f));
}

TEST_CASE("tp command supports minecraft-style relative coordinates")
{
    const ChatCommandResult result = executeChatCommand(
        "/tp ~1.5 ~ ~-2",
        ChatCommandContext{
            .currentFeetPosition = {10.0f, 70.0f, 5.0f},
            .allowTeleport = true,
            .minWorldY = -64,
            .maxWorldY = 255,
        });

    CHECK(result.handled);
    CHECK(result.succeeded);
    REQUIRE(result.teleportFeetPosition.has_value());
    CHECK(result.teleportFeetPosition->x == doctest::Approx(11.5f));
    CHECK(result.teleportFeetPosition->y == doctest::Approx(70.0f));
    CHECK(result.teleportFeetPosition->z == doctest::Approx(3.0f));
}

TEST_CASE("tp command rejects client-side teleport attempts")
{
    const ChatCommandResult result = executeChatCommand(
        "/tp 0 80 0",
        ChatCommandContext{
            .currentFeetPosition = {0.0f, 64.0f, 0.0f},
            .allowTeleport = false,
            .minWorldY = -64,
            .maxWorldY = 255,
        });

    CHECK(result.handled);
    CHECK_FALSE(result.succeeded);
    CHECK_FALSE(result.teleportFeetPosition.has_value());
}

TEST_CASE("help command lists available chat commands")
{
    const ChatCommandResult result = executeChatCommand("/help", ChatCommandContext{});

    CHECK(result.handled);
    CHECK(result.succeeded);
    CHECK(result.feedback.find("/tp") != std::string::npos);
}
