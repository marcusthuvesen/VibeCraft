#include "vibecraft/app/Application.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>

#include "vibecraft/platform/LocalNetworkAddress.hpp"

namespace vibecraft::app
{
namespace
{
[[nodiscard]] std::int64_t currentUnixTimeSeconds()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

[[nodiscard]] std::uint32_t generateRandomWorldSeed()
{
    std::random_device randomDevice;
    const std::uint32_t deviceSeed = randomDevice();
    const std::uint64_t ticks = SDL_GetTicksNS();
    std::uint32_t seed = deviceSeed ^ static_cast<std::uint32_t>(ticks) ^ static_cast<std::uint32_t>(ticks >> 32U);
    if (seed == 0)
    {
        seed = 0x6d2b79f5U;
    }
    return seed;
}

void trimInPlace(std::string& value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.pop_back();
    }
}

[[nodiscard]] std::string trimCopy(std::string value)
{
    trimInPlace(value);
    return value;
}

[[nodiscard]] std::string resolveJoinAddressFromEnvironment()
{
    if (const char* const address = std::getenv("VIBECRAFT_JOIN_ADDRESS"); address != nullptr
        && address[0] != '\0')
    {
        return std::string(address);
    }
    return "127.0.0.1";
}
}  // namespace

std::filesystem::path Application::prefsRootPath() const
{
    if (const char* const explicitHome = std::getenv("VIBECRAFT_HOME");
        explicitHome != nullptr && std::string(explicitHome).empty() == false)
    {
        return std::filesystem::path(explicitHome) / "assets" / "saves";
    }
    return "assets/saves";
}

std::filesystem::path Application::singleplayerWorldsRootPath() const
{
    return prefsRootPath() / "worlds";
}

std::filesystem::path Application::singleplayerWorldDirectory(const std::string& folderName) const
{
    return singleplayerWorldsRootPath() / folderName;
}

std::filesystem::path Application::singleplayerWorldDataPath(const std::string& folderName) const
{
    return singleplayerWorldDirectory(folderName) / "world.bin";
}

std::filesystem::path Application::singleplayerPlayerDataPath(const std::string& folderName) const
{
    return singleplayerWorldDirectory(folderName) / "player.bin";
}

std::filesystem::path Application::singleplayerWorldMetadataPath(const std::string& folderName) const
{
    return singleplayerWorldDirectory(folderName) / "meta.json";
}

void Application::refreshSingleplayerWorldList()
{
    std::string previouslySelectedFolder = activeSingleplayerWorldFolderName_;
    if (previouslySelectedFolder.empty()
        && selectedSingleplayerWorldIndex_ < singleplayerWorlds_.size())
    {
        previouslySelectedFolder = singleplayerWorlds_[selectedSingleplayerWorldIndex_].folderName;
    }

    singleplayerWorlds_.clear();
    std::error_code errorCode;
    std::filesystem::create_directories(singleplayerWorldsRootPath(), errorCode);
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(singleplayerWorldsRootPath(), errorCode))
    {
        if (errorCode || !entry.is_directory())
        {
            continue;
        }
        SingleplayerWorldEntry worldEntry;
        worldEntry.folderName = entry.path().filename().string();
        if (const std::optional<SingleplayerWorldMetadata> metadata =
                SingleplayerSaveSerializer::loadMetadata(singleplayerWorldMetadataPath(worldEntry.folderName));
            metadata.has_value())
        {
            worldEntry.metadata = *metadata;
        }
        else
        {
            worldEntry.metadata.displayName = worldEntry.folderName;
        }
        worldEntry.hasWorldData = std::filesystem::exists(singleplayerWorldDataPath(worldEntry.folderName), errorCode);
        worldEntry.hasPlayerData = std::filesystem::exists(singleplayerPlayerDataPath(worldEntry.folderName), errorCode);
        const bool hasMetadata = std::filesystem::exists(singleplayerWorldMetadataPath(worldEntry.folderName), errorCode);
        if (!hasMetadata && !worldEntry.hasWorldData && !worldEntry.hasPlayerData)
        {
            continue;
        }
        singleplayerWorlds_.push_back(std::move(worldEntry));
    }

    std::sort(
        singleplayerWorlds_.begin(),
        singleplayerWorlds_.end(),
        [](const SingleplayerWorldEntry& lhs, const SingleplayerWorldEntry& rhs)
        {
            if (lhs.metadata.lastPlayedUnixSeconds != rhs.metadata.lastPlayedUnixSeconds)
            {
                return lhs.metadata.lastPlayedUnixSeconds > rhs.metadata.lastPlayedUnixSeconds;
            }
            return lhs.folderName < rhs.folderName;
        });

    if (singleplayerWorlds_.empty())
    {
        selectedSingleplayerWorldIndex_ = 0;
        return;
    }

    const auto selectedIt = std::find_if(
        singleplayerWorlds_.begin(),
        singleplayerWorlds_.end(),
        [&previouslySelectedFolder](const SingleplayerWorldEntry& entry)
        {
            return entry.folderName == previouslySelectedFolder;
        });
    selectedSingleplayerWorldIndex_ = selectedIt != singleplayerWorlds_.end()
        ? static_cast<std::size_t>(std::distance(singleplayerWorlds_.begin(), selectedIt))
        : 0;
}

bool Application::createNewSingleplayerWorld()
{
    std::error_code errorCode;
    std::filesystem::create_directories(singleplayerWorldsRootPath(), errorCode);

    int nextWorldIndex = 1;
    while (std::filesystem::exists(
        singleplayerWorldDirectory(fmt::format("world_{:03d}", nextWorldIndex)),
        errorCode))
    {
        ++nextWorldIndex;
    }

    const std::string folderName = fmt::format("world_{:03d}", nextWorldIndex);
    const std::uint32_t seed = generateRandomWorldSeed();
    const std::int64_t now = currentUnixTimeSeconds();
    SingleplayerWorldMetadata metadata{
        .displayName = fmt::format("World {}", nextWorldIndex),
        .seed = seed,
        .createdUnixSeconds = now,
        .lastPlayedUnixSeconds = now,
    };
    if (!SingleplayerSaveSerializer::saveMetadata(metadata, singleplayerWorldMetadataPath(folderName)))
    {
        mainMenuNotice_ = "Could not create a new world.";
        return false;
    }

    refreshSingleplayerWorldList();
    const auto it = std::find_if(
        singleplayerWorlds_.begin(),
        singleplayerWorlds_.end(),
        [&folderName](const SingleplayerWorldEntry& entry)
        {
            return entry.folderName == folderName;
        });
    if (it != singleplayerWorlds_.end())
    {
        selectedSingleplayerWorldIndex_ = static_cast<std::size_t>(std::distance(singleplayerWorlds_.begin(), it));
        mainMenuNotice_ = fmt::format("Created {}.", it->metadata.displayName);
        return true;
    }
    return false;
}

bool Application::ensureSelectedSingleplayerWorld()
{
    if (!singleplayerWorlds_.empty())
    {
        selectedSingleplayerWorldIndex_ = std::min(selectedSingleplayerWorldIndex_, singleplayerWorlds_.size() - 1);
        return true;
    }
    return createNewSingleplayerWorld();
}

void Application::cycleSelectedSingleplayerWorld(const int direction)
{
    if (singleplayerWorlds_.empty())
    {
        mainMenuNotice_ = "No worlds yet. Press N to create one.";
        return;
    }
    const int count = static_cast<int>(singleplayerWorlds_.size());
    const int current = static_cast<int>(std::min(selectedSingleplayerWorldIndex_, singleplayerWorlds_.size() - 1));
    selectedSingleplayerWorldIndex_ = static_cast<std::size_t>((current + direction + count) % count);
    mainMenuNotice_ = fmt::format(
        "Selected {}.",
        singleplayerWorlds_[selectedSingleplayerWorldIndex_].metadata.displayName);
}

bool Application::saveActiveSingleplayerWorld(const bool showNotice)
{
    if (activeSingleplayerWorldFolderName_.empty())
    {
        return false;
    }

    const std::string folderName = activeSingleplayerWorldFolderName_;
    if (!world_.save(singleplayerWorldDataPath(folderName)))
    {
        if (showNotice)
        {
            pauseMenuNotice_ = "Could not save world.";
        }
        return false;
    }

    SingleplayerPlayerState playerState;
    playerState.playerFeetPosition = playerFeetPosition_;
    playerState.spawnFeetPosition = spawnFeetPosition_;
    playerState.cameraYawDegrees = camera_.yawDegrees();
    playerState.cameraPitchDegrees = camera_.pitchDegrees();
    playerState.health = playerVitals_.health();
    playerState.air = playerVitals_.air();
    playerState.creativeModeEnabled = creativeModeEnabled_;
    playerState.selectedHotbarIndex = static_cast<std::uint8_t>(std::min<std::size_t>(selectedHotbarIndex_, 255));
    playerState.dayNightElapsedSeconds = dayNightCycle_.elapsedSeconds();
    playerState.weatherElapsedSeconds = weatherSystem_.elapsedSeconds();
    playerState.hotbarSlots = hotbarSlots_;
    playerState.bagSlots = bagSlots_;
    playerState.equipmentSlots = equipmentSlots_;
    playerState.chestSlotsByPosition = chestSlotsByPosition_;
    playerState.furnaceStatesByPosition = furnaceStatesByPosition_;
    playerState.droppedItems.reserve(droppedItems_.size());
    for (const DroppedItem& droppedItem : droppedItems_)
    {
        playerState.droppedItems.push_back(SavedDroppedItem{
            .blockType = droppedItem.blockType,
            .equippedItem = droppedItem.equippedItem,
            .worldPosition = droppedItem.worldPosition,
            .velocity = droppedItem.velocity,
            .ageSeconds = droppedItem.ageSeconds,
            .pickupDelaySeconds = droppedItem.pickupDelaySeconds,
            .spinRadians = droppedItem.spinRadians,
        });
    }
    if (!SingleplayerSaveSerializer::savePlayerState(playerState, singleplayerPlayerDataPath(folderName)))
    {
        if (showNotice)
        {
            pauseMenuNotice_ = "Could not save player data.";
        }
        return false;
    }

    SingleplayerWorldMetadata metadata =
        selectedSingleplayerWorldIndex_ < singleplayerWorlds_.size()
            ? singleplayerWorlds_[selectedSingleplayerWorldIndex_].metadata
            : SingleplayerWorldMetadata{};
    if (metadata.displayName.empty())
    {
        metadata.displayName = activeSingleplayerWorldDisplayName_.empty()
            ? folderName
            : activeSingleplayerWorldDisplayName_;
    }
    metadata.seed = world_.generationSeed();
    if (metadata.createdUnixSeconds == 0)
    {
        metadata.createdUnixSeconds = currentUnixTimeSeconds();
    }
    metadata.lastPlayedUnixSeconds = currentUnixTimeSeconds();
    static_cast<void>(SingleplayerSaveSerializer::saveMetadata(
        metadata,
        singleplayerWorldMetadataPath(folderName)));
    refreshSingleplayerWorldList();
    autosaveAccumulatorSeconds_ = 0.0f;
    if (showNotice)
    {
        pauseMenuNotice_ = fmt::format("Saved {}.", metadata.displayName);
    }
    return true;
}

void Application::unloadActiveSingleplayerWorld()
{
    activeSingleplayerWorldFolderName_.clear();
    activeSingleplayerWorldDisplayName_.clear();
    autosaveAccumulatorSeconds_ = 0.0f;
    sessionPlayTimeSeconds_ = 0.0f;
    chatState_ = {};
}

std::filesystem::path Application::multiplayerPrefsPath() const
{
    return prefsRootPath() / "multiplayer_prefs.txt";
}

std::filesystem::path Application::joinPresetsPath() const
{
    return prefsRootPath() / "join_presets.txt";
}

void Application::applyJoinPreset(const JoinPresetEntry& preset)
{
    joinAddressInput_ = preset.host;
    joinPortInput_ = std::to_string(preset.port);
    multiplayerPort_ = preset.port;
    multiplayerAddress_ = preset.host;
}

void Application::loadJoinPresets()
{
    joinPresets_.clear();
    std::ifstream input(joinPresetsPath());
    if (input.is_open())
    {
        std::string line;
        while (std::getline(input, line) && joinPresets_.size() < 3)
        {
            trimInPlace(line);
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            const std::size_t p1 = line.find('|');
            if (p1 == std::string::npos)
            {
                continue;
            }
            const std::size_t p2 = line.find('|', p1 + 1);
            if (p2 == std::string::npos || p2 <= p1)
            {
                continue;
            }
            std::string label = trimCopy(line.substr(0, p1));
            std::string host = trimCopy(line.substr(p1 + 1, p2 - p1 - 1));
            std::string portStr = trimCopy(line.substr(p2 + 1));
            if (label.empty() || host.empty())
            {
                continue;
            }
            unsigned long port = 41234;
            try
            {
                port = std::stoul(portStr);
            }
            catch (...)
            {
                continue;
            }
            if (port > 65535UL)
            {
                continue;
            }
            joinPresets_.push_back(JoinPresetEntry{
                .label = std::move(label),
                .host = std::move(host),
                .port = static_cast<std::uint16_t>(port),
            });
        }
    }
    if (joinPresets_.empty())
    {
        joinPresets_.push_back(
            JoinPresetEntry{.label = "This PC (local)", .host = "127.0.0.1", .port = 41234});
    }
}

void Application::loadMultiplayerPrefs()
{
    joinAddressInput_ = resolveJoinAddressFromEnvironment();
    joinPortInput_ = "41234";
    std::ifstream input(multiplayerPrefsPath());
    if (!input.is_open())
    {
        multiplayerAddress_ = joinAddressInput_;
        loadJoinPresets();
        return;
    }

    std::string addressLine;
    std::string portLine;
    std::getline(input, addressLine);
    std::getline(input, portLine);
    trimInPlace(addressLine);
    trimInPlace(portLine);
    if (!addressLine.empty())
    {
        joinAddressInput_ = addressLine;
    }
    if (!portLine.empty())
    {
        joinPortInput_ = portLine;
    }
    multiplayerAddress_ = joinAddressInput_;
    try
    {
        if (!portLine.empty())
        {
            const unsigned long parsedPort = std::stoul(portLine);
            if (parsedPort <= 65535UL)
            {
                multiplayerPort_ = static_cast<std::uint16_t>(parsedPort);
            }
        }
    }
    catch (...)
    {
    }
    loadJoinPresets();
}

void Application::saveMultiplayerPrefs() const
{
    std::error_code errorCode;
    std::filesystem::create_directories(multiplayerPrefsPath().parent_path(), errorCode);
    std::ofstream output(multiplayerPrefsPath(), std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }
    output << joinAddressInput_ << '\n' << joinPortInput_ << '\n';
}

std::filesystem::path Application::audioPrefsPath() const
{
    return prefsRootPath() / "audio_prefs.txt";
}

void Application::loadAudioPrefs()
{
    std::ifstream input(audioPrefsPath());
    if (!input.is_open())
    {
        return;
    }
    std::string musicLine;
    std::string sfxLine;
    std::getline(input, musicLine);
    std::getline(input, sfxLine);
    trimInPlace(musicLine);
    trimInPlace(sfxLine);
    try
    {
        if (!musicLine.empty())
        {
            musicVolume_ = std::clamp(std::stof(musicLine), 0.0f, 1.0f);
        }
        if (!sfxLine.empty())
        {
            sfxVolume_ = std::clamp(std::stof(sfxLine), 0.0f, 1.0f);
        }
    }
    catch (...)
    {
    }
}

void Application::saveAudioPrefs() const
{
    std::error_code errorCode;
    std::filesystem::create_directories(audioPrefsPath().parent_path(), errorCode);
    std::ofstream output(audioPrefsPath(), std::ios::trunc);
    if (!output.is_open())
    {
        return;
    }
    output << fmt::format("{:.4f}\n{:.4f}\n", musicVolume_, sfxVolume_);
}

void Application::refreshDetectedLanAddress()
{
    detectedLanAddress_ = platform::primaryLanIPv4String();
}

void Application::processJoinMenuTextInput()
{
    if (mainMenuMultiplayerPanel_ != MainMenuMultiplayerPanel::Join)
    {
        return;
    }

    if (inputState_.tabPressed)
    {
        joinFocusedField_ = 1 - joinFocusedField_;
    }

    if (inputState_.backspacePressed)
    {
        if (joinFocusedField_ == 0)
        {
            if (!joinAddressInput_.empty())
            {
                joinAddressInput_.pop_back();
            }
        }
        else if (!joinPortInput_.empty())
        {
            joinPortInput_.pop_back();
        }
    }

    if (!inputState_.textInputUtf8.empty())
    {
        if (joinFocusedField_ == 0)
        {
            joinAddressInput_ += inputState_.textInputUtf8;
        }
        else
        {
            for (const char character : inputState_.textInputUtf8)
            {
                if (character >= '0' && character <= '9')
                {
                    joinPortInput_ += character;
                }
            }
        }
    }
}

void Application::tryStartHostFromMenu()
{
    refreshDetectedLanAddress();
    pendingHostStartAfterWorldLoad_ = true;
    beginSingleplayerLoad();
}

void Application::tryConnectFromJoinMenu()
{
    const std::string host = trimCopy(joinAddressInput_);
    if (host.empty())
    {
        mainMenuNotice_ = "Enter the host address (e.g. 192.168.1.5).";
        return;
    }

    unsigned long parsedPort = 41234;
    try
    {
        if (!joinPortInput_.empty())
        {
            parsedPort = std::stoul(joinPortInput_);
        }
    }
    catch (...)
    {
        mainMenuNotice_ = "Port must be a number (default 41234).";
        return;
    }

    if (parsedPort > 65535UL)
    {
        mainMenuNotice_ = "Port must be between 0 and 65535.";
        return;
    }

    multiplayerPort_ = static_cast<std::uint16_t>(parsedPort);
    multiplayerAddress_ = host;
    if (!startClientSession(host))
    {
        mainMenuNotice_ = "Could not connect. Check address, firewall, and that the host is running.";
        return;
    }

    clearClientWorldAwaitingHostChunks();
    beginClientJoinLoad();

    saveMultiplayerPrefs();
}
}  // namespace vibecraft::app
