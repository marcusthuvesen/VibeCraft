#include "vibecraft/app/Application.hpp"

namespace vibecraft::app
{
void Application::updateMultiplayer(const float deltaTimeSeconds)
{
    networkTickAccumulatorSeconds_ += deltaTimeSeconds;
    if (multiplayerMode_ != MultiplayerRuntimeMode::Client)
    {
        clientReplicatedMobs_.clear();
    }

    updateHostMultiplayer(deltaTimeSeconds);
    updateClientMultiplayer(deltaTimeSeconds);
}
}  // namespace vibecraft::app
