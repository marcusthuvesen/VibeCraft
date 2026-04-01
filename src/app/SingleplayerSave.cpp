#include "vibecraft/app/SingleplayerSave.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace vibecraft::app
{
namespace
{
constexpr std::uint32_t kPlayerStateMagic = 0x56424350;  // VBCP
constexpr std::uint32_t kPlayerStateVersion = 3;

template<typename T>
bool writeBinary(std::ofstream& output, const T& value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return output.good();
}

template<typename T>
bool readBinary(std::ifstream& input, T& value)
{
    input.read(reinterpret_cast<char*>(&value), sizeof(T));
    return input.good();
}

bool writeInventorySlot(std::ofstream& output, const InventorySlot& slot)
{
    const std::uint8_t blockType = static_cast<std::uint8_t>(slot.blockType);
    const std::uint8_t equippedItem = static_cast<std::uint8_t>(slot.equippedItem);
    return writeBinary(output, blockType)
        && writeBinary(output, slot.count)
        && writeBinary(output, equippedItem);
}

bool readInventorySlot(std::ifstream& input, InventorySlot& slot)
{
    std::uint8_t blockType = 0;
    std::uint8_t equippedItem = 0;
    if (!readBinary(input, blockType) || !readBinary(input, slot.count) || !readBinary(input, equippedItem))
    {
        return false;
    }
    slot.blockType = static_cast<vibecraft::world::BlockType>(blockType);
    slot.equippedItem = static_cast<EquippedItem>(equippedItem);
    return true;
}

bool writeOxygenState(std::ofstream& output, const vibecraft::game::OxygenState& oxygenState)
{
    const std::uint8_t tankTier = static_cast<std::uint8_t>(oxygenState.tankTier);
    return writeBinary(output, tankTier)
        && writeBinary(output, oxygenState.oxygen)
        && writeBinary(output, oxygenState.capacity);
}

bool readOxygenState(std::ifstream& input, vibecraft::game::OxygenState& oxygenState)
{
    std::uint8_t tankTier = 0;
    if (!readBinary(input, tankTier)
        || !readBinary(input, oxygenState.oxygen)
        || !readBinary(input, oxygenState.capacity))
    {
        return false;
    }

    oxygenState.tankTier = static_cast<vibecraft::game::OxygenTankTier>(tankTier);
    oxygenState.capacity = std::max(0.0f, oxygenState.capacity);
    oxygenState.oxygen = std::clamp(oxygenState.oxygen, 0.0f, oxygenState.capacity);
    return true;
}

[[nodiscard]] std::string trimCopy(const std::string_view value)
{
    std::size_t start = 0;
    std::size_t end = value.size();
    while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

[[nodiscard]] std::optional<std::string> extractJsonStringField(const std::string& content, const std::string& key)
{
    const std::string needle = fmt::format("\"{}\"", key);
    const std::size_t keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return std::nullopt;
    }
    const std::size_t colonPos = content.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return std::nullopt;
    }
    const std::size_t firstQuote = content.find('"', colonPos + 1);
    if (firstQuote == std::string::npos)
    {
        return std::nullopt;
    }
    const std::size_t secondQuote = content.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos)
    {
        return std::nullopt;
    }
    return content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

template<typename Integer>
[[nodiscard]] std::optional<Integer> extractJsonIntegerField(const std::string& content, const std::string& key)
{
    const std::string needle = fmt::format("\"{}\"", key);
    const std::size_t keyPos = content.find(needle);
    if (keyPos == std::string::npos)
    {
        return std::nullopt;
    }
    const std::size_t colonPos = content.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return std::nullopt;
    }
    std::size_t valueStart = colonPos + 1;
    while (valueStart < content.size() && std::isspace(static_cast<unsigned char>(content[valueStart])) != 0)
    {
        ++valueStart;
    }
    std::size_t valueEnd = valueStart;
    while (valueEnd < content.size())
    {
        const char ch = content[valueEnd];
        if ((ch < '0' || ch > '9') && ch != '-')
        {
            break;
        }
        ++valueEnd;
    }
    if (valueEnd <= valueStart)
    {
        return std::nullopt;
    }
    try
    {
        if constexpr (std::is_same_v<Integer, std::uint32_t>)
        {
            return static_cast<std::uint32_t>(std::stoul(content.substr(valueStart, valueEnd - valueStart)));
        }
        else
        {
            return static_cast<Integer>(std::stoll(content.substr(valueStart, valueEnd - valueStart)));
        }
    }
    catch (...)
    {
        return std::nullopt;
    }
}
}  // namespace

bool SingleplayerSaveSerializer::saveMetadata(
    const SingleplayerWorldMetadata& metadata,
    const std::filesystem::path& outputPath)
{
    std::error_code errorCode;
    std::filesystem::create_directories(outputPath.parent_path(), errorCode);

    std::ofstream output(outputPath, std::ios::trunc);
    if (!output.is_open())
    {
        return false;
    }

    output
        << "{\n"
        << fmt::format("  \"version\": {},\n", 1)
        << fmt::format("  \"name\": \"{}\",\n", metadata.displayName)
        << fmt::format("  \"seed\": {},\n", metadata.seed)
        << fmt::format("  \"created_unix\": {},\n", metadata.createdUnixSeconds)
        << fmt::format("  \"last_played_unix\": {}\n", metadata.lastPlayedUnixSeconds)
        << "}\n";
    return output.good();
}

std::optional<SingleplayerWorldMetadata> SingleplayerSaveSerializer::loadMetadata(
    const std::filesystem::path& inputPath)
{
    std::ifstream input(inputPath);
    if (!input.is_open())
    {
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string content = buffer.str();

    const std::optional<std::string> displayName = extractJsonStringField(content, "name");
    const std::optional<std::uint32_t> seed = extractJsonIntegerField<std::uint32_t>(content, "seed");
    if (!displayName.has_value() || !seed.has_value())
    {
        return std::nullopt;
    }

    SingleplayerWorldMetadata metadata;
    metadata.displayName = trimCopy(*displayName);
    metadata.seed = *seed;
    metadata.createdUnixSeconds = extractJsonIntegerField<std::int64_t>(content, "created_unix").value_or(0);
    metadata.lastPlayedUnixSeconds = extractJsonIntegerField<std::int64_t>(content, "last_played_unix").value_or(0);
    return metadata;
}

bool SingleplayerSaveSerializer::savePlayerState(
    const SingleplayerPlayerState& state,
    const std::filesystem::path& outputPath)
{
    std::error_code errorCode;
    std::filesystem::create_directories(outputPath.parent_path(), errorCode);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output.is_open())
    {
        return false;
    }

    if (!writeBinary(output, kPlayerStateMagic)
        || !writeBinary(output, kPlayerStateVersion)
        || !writeBinary(output, state.playerFeetPosition.x)
        || !writeBinary(output, state.playerFeetPosition.y)
        || !writeBinary(output, state.playerFeetPosition.z)
        || !writeBinary(output, state.spawnFeetPosition.x)
        || !writeBinary(output, state.spawnFeetPosition.y)
        || !writeBinary(output, state.spawnFeetPosition.z)
        || !writeBinary(output, state.cameraYawDegrees)
        || !writeBinary(output, state.cameraPitchDegrees)
        || !writeBinary(output, state.health)
        || !writeBinary(output, state.air)
        || !writeOxygenState(output, state.oxygenState))
    {
        return false;
    }

    const std::uint8_t creativeModeEnabled = state.creativeModeEnabled ? 1U : 0U;
    if (!writeBinary(output, creativeModeEnabled)
        || !writeBinary(output, state.selectedHotbarIndex)
        || !writeBinary(output, state.dayNightElapsedSeconds)
        || !writeBinary(output, state.weatherElapsedSeconds))
    {
        return false;
    }

    for (const InventorySlot& slot : state.hotbarSlots)
    {
        if (!writeInventorySlot(output, slot))
        {
            return false;
        }
    }
    for (const InventorySlot& slot : state.bagSlots)
    {
        if (!writeInventorySlot(output, slot))
        {
            return false;
        }
    }
    for (const InventorySlot& slot : state.equipmentSlots)
    {
        if (!writeInventorySlot(output, slot))
        {
            return false;
        }
    }

    const std::uint32_t droppedItemCount = static_cast<std::uint32_t>(state.droppedItems.size());
    if (!writeBinary(output, droppedItemCount))
    {
        return false;
    }
    for (const SavedDroppedItem& droppedItem : state.droppedItems)
    {
        const std::uint8_t blockType = static_cast<std::uint8_t>(droppedItem.blockType);
        const std::uint8_t equippedItem = static_cast<std::uint8_t>(droppedItem.equippedItem);
        if (!writeBinary(output, blockType)
            || !writeBinary(output, equippedItem)
            || !writeBinary(output, droppedItem.worldPosition.x)
            || !writeBinary(output, droppedItem.worldPosition.y)
            || !writeBinary(output, droppedItem.worldPosition.z)
            || !writeBinary(output, droppedItem.velocity.x)
            || !writeBinary(output, droppedItem.velocity.y)
            || !writeBinary(output, droppedItem.velocity.z)
            || !writeBinary(output, droppedItem.ageSeconds)
            || !writeBinary(output, droppedItem.pickupDelaySeconds)
            || !writeBinary(output, droppedItem.spinRadians))
        {
            return false;
        }
    }

    const std::uint32_t chestCount = static_cast<std::uint32_t>(state.chestSlotsByPosition.size());
    if (!writeBinary(output, chestCount))
    {
        return false;
    }
    for (const auto& [storageKey, slots] : state.chestSlotsByPosition)
    {
        if (!writeBinary(output, storageKey))
        {
            return false;
        }
        for (const InventorySlot& slot : slots)
        {
            if (!writeInventorySlot(output, slot))
            {
                return false;
            }
        }
    }

    return output.good();
}

std::optional<SingleplayerPlayerState> SingleplayerSaveSerializer::loadPlayerState(
    const std::filesystem::path& inputPath)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input.is_open())
    {
        return std::nullopt;
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!readBinary(input, magic) || !readBinary(input, version)
        || magic != kPlayerStateMagic || (version < 1 || version > kPlayerStateVersion))
    {
        return std::nullopt;
    }

    SingleplayerPlayerState state;
    std::uint8_t creativeModeEnabled = 0;
    if (!readBinary(input, state.playerFeetPosition.x)
        || !readBinary(input, state.playerFeetPosition.y)
        || !readBinary(input, state.playerFeetPosition.z)
        || !readBinary(input, state.spawnFeetPosition.x)
        || !readBinary(input, state.spawnFeetPosition.y)
        || !readBinary(input, state.spawnFeetPosition.z)
        || !readBinary(input, state.cameraYawDegrees)
        || !readBinary(input, state.cameraPitchDegrees)
        || !readBinary(input, state.health)
        || !readBinary(input, state.air))
    {
        return std::nullopt;
    }

    if (version >= 2)
    {
        if (!readOxygenState(input, state.oxygenState))
        {
            return std::nullopt;
        }
    }

    if (!readBinary(input, creativeModeEnabled)
        || !readBinary(input, state.selectedHotbarIndex)
        || !readBinary(input, state.dayNightElapsedSeconds)
        || !readBinary(input, state.weatherElapsedSeconds))
    {
        return std::nullopt;
    }
    state.creativeModeEnabled = creativeModeEnabled != 0;

    for (InventorySlot& slot : state.hotbarSlots)
    {
        if (!readInventorySlot(input, slot))
        {
            return std::nullopt;
        }
    }
    for (InventorySlot& slot : state.bagSlots)
    {
        if (!readInventorySlot(input, slot))
        {
            return std::nullopt;
        }
    }
    if (version >= 3)
    {
        for (InventorySlot& slot : state.equipmentSlots)
        {
            if (!readInventorySlot(input, slot))
            {
                return std::nullopt;
            }
        }
    }

    std::uint32_t droppedItemCount = 0;
    if (!readBinary(input, droppedItemCount))
    {
        return std::nullopt;
    }
    state.droppedItems.reserve(droppedItemCount);
    for (std::uint32_t i = 0; i < droppedItemCount; ++i)
    {
        SavedDroppedItem droppedItem;
        std::uint8_t blockType = 0;
        std::uint8_t equippedItem = 0;
        if (!readBinary(input, blockType)
            || !readBinary(input, equippedItem)
            || !readBinary(input, droppedItem.worldPosition.x)
            || !readBinary(input, droppedItem.worldPosition.y)
            || !readBinary(input, droppedItem.worldPosition.z)
            || !readBinary(input, droppedItem.velocity.x)
            || !readBinary(input, droppedItem.velocity.y)
            || !readBinary(input, droppedItem.velocity.z)
            || !readBinary(input, droppedItem.ageSeconds)
            || !readBinary(input, droppedItem.pickupDelaySeconds)
            || !readBinary(input, droppedItem.spinRadians))
        {
            return std::nullopt;
        }
        droppedItem.blockType = static_cast<vibecraft::world::BlockType>(blockType);
        droppedItem.equippedItem = static_cast<EquippedItem>(equippedItem);
        state.droppedItems.push_back(droppedItem);
    }

    std::uint32_t chestCount = 0;
    if (!readBinary(input, chestCount))
    {
        return std::nullopt;
    }
    for (std::uint32_t chestIndex = 0; chestIndex < chestCount; ++chestIndex)
    {
        std::int64_t storageKey = 0;
        if (!readBinary(input, storageKey))
        {
            return std::nullopt;
        }
        CraftingGridSlots slots{};
        for (InventorySlot& slot : slots)
        {
            if (!readInventorySlot(input, slot))
            {
                return std::nullopt;
            }
        }
        state.chestSlotsByPosition.emplace(storageKey, slots);
    }

    return state;
}
}  // namespace vibecraft::app
