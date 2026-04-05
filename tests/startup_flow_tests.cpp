#include <doctest/doctest.h>

#include "vibecraft/app/StartupFlow.hpp"

using vibecraft::app::SingleplayerStartAction;
using vibecraft::app::SingleplayerStartRequest;
using vibecraft::app::envFlagEnabled;
using vibecraft::app::resolveSingleplayerStartAction;

TEST_CASE("env flag parser accepts common truthy values")
{
    CHECK(envFlagEnabled("1"));
    CHECK(envFlagEnabled("true"));
    CHECK(envFlagEnabled(" YES "));
    CHECK(envFlagEnabled("On"));
}

TEST_CASE("env flag parser rejects empty and falsey values")
{
    CHECK_FALSE(envFlagEnabled(""));
    CHECK_FALSE(envFlagEnabled("0"));
    CHECK_FALSE(envFlagEnabled("false"));
    CHECK_FALSE(envFlagEnabled("off"));
    CHECK_FALSE(envFlagEnabled(" maybe "));
}

TEST_CASE("loading a saved world requires an existing world")
{
    CHECK(
        resolveSingleplayerStartAction(SingleplayerStartRequest::LoadSavedWorld, true)
        == SingleplayerStartAction::StartSelectedWorld);
    CHECK(
        resolveSingleplayerStartAction(SingleplayerStartRequest::LoadSavedWorld, false)
        == SingleplayerStartAction::MissingSavedWorld);
}

TEST_CASE("creating a new world stays explicit")
{
    CHECK(
        resolveSingleplayerStartAction(SingleplayerStartRequest::CreateNewWorld, true)
        == SingleplayerStartAction::CreateAndStartWorld);
    CHECK(
        resolveSingleplayerStartAction(SingleplayerStartRequest::CreateNewWorld, false)
        == SingleplayerStartAction::CreateAndStartWorld);
}
