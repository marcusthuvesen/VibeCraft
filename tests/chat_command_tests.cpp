#include <doctest/doctest.h>

#include "vibecraft/app/ChatCommands.hpp"

namespace
{
using vibecraft::app::ChatCommandContext;
using vibecraft::app::ChatAutocompleteResult;
using vibecraft::app::ChatCommandResult;
using vibecraft::app::autocompleteChatInput;
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
    CHECK_FALSE(result.feedbackLines.empty());
    CHECK(result.feedbackLines[1].find("/tp") != std::string::npos);
}

TEST_CASE("gamemode command toggles creative mode")
{
    const ChatCommandResult result = executeChatCommand("/gamemode creative", ChatCommandContext{});

    CHECK(result.handled);
    CHECK(result.succeeded);
    REQUIRE(result.creativeModeEnabled.has_value());
    CHECK(*result.creativeModeEnabled);
}

TEST_CASE("give command resolves minecraft-style item ids")
{
    const ChatCommandResult result = executeChatCommand("/give oak_planks 12", ChatCommandContext{});

    CHECK(result.handled);
    CHECK(result.succeeded);
    REQUIRE(result.giveStacks.size() == 1);
    CHECK(result.giveStacks.front().count == 12);
    CHECK(result.giveStacks.front().displayLabel == "Oak Planks");
}

TEST_CASE("give command accepts display-name style item names")
{
    const ChatCommandResult sand = executeChatCommand("/give sand 40", ChatCommandContext{});
    CHECK(sand.handled);
    CHECK(sand.succeeded);
    REQUIRE(sand.giveStacks.size() == 1);
    CHECK(sand.giveStacks.front().count == 40);
    CHECK(sand.giveStacks.front().displayLabel == "Sand");

    const ChatCommandResult spaced = executeChatCommand("/give oak planks 5", ChatCommandContext{});
    CHECK(spaced.handled);
    CHECK(spaced.succeeded);
    REQUIRE(spaced.giveStacks.size() == 1);
    CHECK(spaced.giveStacks.front().count == 5);
    CHECK(spaced.giveStacks.front().displayLabel == "Oak Planks");
}

TEST_CASE("time command marks world-state changes for host routing")
{
    const ChatCommandResult result = executeChatCommand(
        "/time set night",
        ChatCommandContext{
            .globalWorldStateRequiresHostAuthority = true,
        });

    CHECK(result.handled);
    CHECK(result.succeeded);
    CHECK(result.requiresHostAuthority);
    REQUIRE(result.dayNightElapsedSeconds.has_value());
    CHECK(*result.dayNightElapsedSeconds == doctest::Approx(390.0f));
}

TEST_CASE("autocomplete expands command names and item ids")
{
    const ChatAutocompleteResult commandCompletion = autocompleteChatInput("/ga", 3);
    CHECK(commandCompletion.applied);
    CHECK(commandCompletion.updatedInput == "/gamemode ");

    const ChatAutocompleteResult itemCompletion = autocompleteChatInput("/give oak_p", 11);
    CHECK(itemCompletion.applied);
    CHECK(itemCompletion.updatedInput == "/give oak_planks ");
}
