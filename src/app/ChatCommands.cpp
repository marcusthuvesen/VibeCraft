#include "vibecraft/app/ChatCommands.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <unordered_set>

#include "vibecraft/game/DayNightCycle.hpp"

namespace vibecraft::app
{
namespace
{
struct CatalogEntry
{
    std::string canonicalName;
    std::string displayLabel;
    world::BlockType blockType = world::BlockType::Air;
    EquippedItem equippedItem = EquippedItem::None;
    std::vector<std::string> aliases;
};

struct TokenRange
{
    std::size_t start = 0;
    std::size_t end = 0;
};

struct TimePreset
{
    float elapsedSeconds = 0.0f;
    std::string label;
};

[[nodiscard]] std::string trimCopy(std::string value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] std::vector<std::string> splitWhitespace(std::string_view value)
{
    std::vector<std::string> tokens;
    std::size_t cursor = 0;
    while (cursor < value.size())
    {
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])) != 0)
        {
            ++cursor;
        }
        if (cursor >= value.size())
        {
            break;
        }

        std::size_t end = cursor;
        while (end < value.size() && std::isspace(static_cast<unsigned char>(value[end])) == 0)
        {
            ++end;
        }
        tokens.emplace_back(value.substr(cursor, end - cursor));
        cursor = end;
    }
    return tokens;
}

[[nodiscard]] std::vector<TokenRange> splitTokenRanges(std::string_view value)
{
    std::vector<TokenRange> ranges;
    std::size_t cursor = 0;
    while (cursor < value.size())
    {
        while (cursor < value.size() && std::isspace(static_cast<unsigned char>(value[cursor])) != 0)
        {
            ++cursor;
        }
        if (cursor >= value.size())
        {
            break;
        }

        std::size_t end = cursor;
        while (end < value.size() && std::isspace(static_cast<unsigned char>(value[end])) == 0)
        {
            ++end;
        }
        ranges.push_back(TokenRange{cursor, end});
        cursor = end;
    }
    return ranges;
}

[[nodiscard]] bool parseFloatToken(std::string_view token, float& value)
{
    if (token.empty())
    {
        return false;
    }

    std::string owned(token);
    char* end = nullptr;
    const float parsedValue = std::strtof(owned.c_str(), &end);
    if (end == owned.c_str() || end == nullptr || *end != '\0')
    {
        return false;
    }

    value = parsedValue;
    return true;
}

[[nodiscard]] bool parseUnsignedToken(std::string_view token, std::uint32_t& value)
{
    if (token.empty())
    {
        return false;
    }
    value = 0;
    const auto [endPtr, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    return error == std::errc{} && endPtr == token.data() + token.size();
}

[[nodiscard]] std::optional<float> parseRelativeCoordinate(std::string_view token, const float baseValue)
{
    if (token.empty())
    {
        return std::nullopt;
    }

    if (token.front() == '~')
    {
        if (token.size() == 1)
        {
            return baseValue;
        }

        float offset = 0.0f;
        if (!parseFloatToken(token.substr(1), offset))
        {
            return std::nullopt;
        }
        return baseValue + offset;
    }

    float absoluteValue = 0.0f;
    if (!parseFloatToken(token, absoluteValue))
    {
        return std::nullopt;
    }
    return absoluteValue;
}

[[nodiscard]] std::string lowerCopy(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

[[nodiscard]] std::string normalizeIdentifier(std::string_view value)
{
    std::string normalized;
    normalized.reserve(value.size());
    bool previousWasSeparator = false;
    for (const unsigned char ch : value)
    {
        if (std::isalnum(ch) != 0)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
            previousWasSeparator = false;
        }
        else if (!previousWasSeparator && !normalized.empty())
        {
            normalized.push_back('_');
            previousWasSeparator = true;
        }
    }
    while (!normalized.empty() && normalized.back() == '_')
    {
        normalized.pop_back();
    }
    return normalized;
}

[[nodiscard]] std::string compactIdentifier(std::string value)
{
    value.erase(
        std::remove(value.begin(), value.end(), '_'),
        value.end());
    return value;
}

void addAliasVariants(std::vector<std::string>& aliases, const std::string& value)
{
    const std::string normalized = normalizeIdentifier(value);
    if (normalized.empty())
    {
        return;
    }

    const auto maybeAdd = [&aliases](const std::string& alias)
    {
        if (!alias.empty() && std::find(aliases.begin(), aliases.end(), alias) == aliases.end())
        {
            aliases.push_back(alias);
        }
    };

    maybeAdd(normalized);
    maybeAdd(compactIdentifier(normalized));

    if (normalized.starts_with("wooden_"))
    {
        maybeAdd("wood_" + normalized.substr(7));
    }
    if (normalized.starts_with("golden_"))
    {
        maybeAdd("gold_" + normalized.substr(7));
    }
    if (normalized == "raw_pork")
    {
        maybeAdd("raw_porkchop");
    }
}

[[nodiscard]] const std::vector<EquippedItem>& equippedItemCatalogValues()
{
    static const std::vector<EquippedItem> kValues{
        EquippedItem::DiamondSword,
        EquippedItem::Stick,
        EquippedItem::RottenFlesh,
        EquippedItem::Leather,
        EquippedItem::RawPorkchop,
        EquippedItem::Mutton,
        EquippedItem::Feather,
        EquippedItem::WoodSword,
        EquippedItem::StoneSword,
        EquippedItem::IronSword,
        EquippedItem::GoldSword,
        EquippedItem::WoodPickaxe,
        EquippedItem::StonePickaxe,
        EquippedItem::IronPickaxe,
        EquippedItem::GoldPickaxe,
        EquippedItem::DiamondPickaxe,
        EquippedItem::WoodAxe,
        EquippedItem::StoneAxe,
        EquippedItem::IronAxe,
        EquippedItem::GoldAxe,
        EquippedItem::DiamondAxe,
        EquippedItem::Coal,
        EquippedItem::Charcoal,
        EquippedItem::ScoutHelmet,
        EquippedItem::ScoutChestRig,
        EquippedItem::ScoutGreaves,
        EquippedItem::ScoutBoots,
        EquippedItem::IronIngot,
        EquippedItem::GoldIngot,
        EquippedItem::Arrow,
        EquippedItem::Bow,
        EquippedItem::String,
        EquippedItem::Gunpowder,
    };
    return kValues;
}

[[nodiscard]] const std::vector<CatalogEntry>& giveCatalog()
{
    static const std::vector<CatalogEntry> kCatalog = []
    {
        std::vector<CatalogEntry> entries;
        std::unordered_set<std::uint8_t> seenBlocks;
        for (std::uint16_t raw = 1; raw <= static_cast<std::uint8_t>(world::BlockType::IronDoorUpperWestOpen); ++raw)
        {
            const world::BlockType normalized =
                world::normalizePlaceVariantBlockType(static_cast<world::BlockType>(raw));
            if (normalized == world::BlockType::Air
                || seenBlocks.contains(static_cast<std::uint8_t>(normalized)))
            {
                continue;
            }
            seenBlocks.insert(static_cast<std::uint8_t>(normalized));
            CatalogEntry entry;
            entry.blockType = normalized;
            entry.displayLabel = blockTypeLabel(normalized);
            entry.canonicalName = normalizeIdentifier(entry.displayLabel);
            addAliasVariants(entry.aliases, entry.displayLabel);
            entries.push_back(std::move(entry));
        }

        for (const EquippedItem item : equippedItemCatalogValues())
        {
            CatalogEntry entry;
            entry.equippedItem = item;
            entry.displayLabel = equippedItemLabel(item);
            entry.canonicalName = normalizeIdentifier(entry.displayLabel);
            addAliasVariants(entry.aliases, entry.displayLabel);
            entries.push_back(std::move(entry));
        }

        std::sort(
            entries.begin(),
            entries.end(),
            [](const CatalogEntry& lhs, const CatalogEntry& rhs)
            {
                return lhs.canonicalName < rhs.canonicalName;
            });
        return entries;
    }();
    return kCatalog;
}

[[nodiscard]] const CatalogEntry* findCatalogEntry(std::string_view token)
{
    const std::string alias = normalizeIdentifier(token);
    if (alias.empty())
    {
        return nullptr;
    }

    for (const CatalogEntry& entry : giveCatalog())
    {
        if (entry.canonicalName == alias
            || std::find(entry.aliases.begin(), entry.aliases.end(), alias) != entry.aliases.end())
        {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] std::optional<TimePreset> parseTimePreset(std::string_view token)
{
    const std::string normalized = normalizeIdentifier(token);
    if (normalized == "day")
    {
        return TimePreset{90.0f, "day"};
    }
    if (normalized == "noon")
    {
        return TimePreset{150.0f, "noon"};
    }
    if (normalized == "night")
    {
        return TimePreset{390.0f, "night"};
    }
    if (normalized == "midnight")
    {
        return TimePreset{450.0f, "midnight"};
    }

    float seconds = 0.0f;
    if (!parseFloatToken(token, seconds))
    {
        return std::nullopt;
    }
    return TimePreset{
        game::DayNightCycle::wrapCycleSeconds(seconds),
        fmt::format("{:.0f}s", seconds),
    };
}

[[nodiscard]] std::optional<std::pair<float, std::string>> parseWeatherPreset(std::string_view token)
{
    const std::string normalized = normalizeIdentifier(token);
    if (normalized == "clear")
    {
        return std::pair<float, std::string>{0.0f, "clear"};
    }
    if (normalized == "cloudy" || normalized == "clouds")
    {
        return std::pair<float, std::string>{140.0f, "cloudy"};
    }
    if (normalized == "rain" || normalized == "rainy")
    {
        return std::pair<float, std::string>{230.0f, "rain"};
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::string> commandNames()
{
    return {"gamemode", "give", "help", "spawnpoint", "teleport", "time", "tp", "weather"};
}

[[nodiscard]] std::vector<std::string> commandCandidates(std::string_view command, const std::size_t tokenIndex)
{
    const std::string normalizedCommand = normalizeIdentifier(command);
    if (tokenIndex == 0)
    {
        return commandNames();
    }

    if (normalizedCommand == "gamemode" || normalizedCommand == "gm")
    {
        return {"creative", "survival"};
    }
    if (normalizedCommand == "weather")
    {
        return {"clear", "cloudy", "rain"};
    }
    if (normalizedCommand == "time")
    {
        if (tokenIndex == 1)
        {
            return {"set", "day", "noon", "night", "midnight"};
        }
        return {"day", "noon", "night", "midnight"};
    }
    if (normalizedCommand == "give" && tokenIndex == 1)
    {
        std::vector<std::string> values;
        values.reserve(giveCatalog().size());
        for (const CatalogEntry& entry : giveCatalog())
        {
            values.push_back(entry.canonicalName);
        }
        return values;
    }
    return {};
}

[[nodiscard]] std::string sharedPrefix(const std::vector<std::string>& values)
{
    if (values.empty())
    {
        return {};
    }

    std::string prefix = values.front();
    for (std::size_t index = 1; index < values.size() && !prefix.empty(); ++index)
    {
        std::size_t matchLength = 0;
        while (matchLength < prefix.size() && matchLength < values[index].size()
               && prefix[matchLength] == values[index][matchLength])
        {
            ++matchLength;
        }
        prefix.resize(matchLength);
    }
    return prefix;
}

[[nodiscard]] std::string joinSuggestionHint(const std::vector<std::string>& values)
{
    std::string hint = "Suggestions: ";
    for (std::size_t index = 0; index < values.size() && index < 5; ++index)
    {
        if (index > 0)
        {
            hint += ", ";
        }
        hint += values[index];
    }
    if (values.size() > 5)
    {
        hint += ", ...";
    }
    return hint;
}
}  // namespace

ChatCommandResult executeChatCommand(std::string_view input, const ChatCommandContext& context)
{
    const std::string trimmedInput = trimCopy(std::string(input));
    if (trimmedInput.empty() || trimmedInput.front() != '/')
    {
        return {};
    }

    const std::vector<std::string> tokens = splitWhitespace(trimmedInput.substr(1));
    if (tokens.empty())
    {
        return {
            .handled = true,
            .succeeded = false,
            .feedbackLines = {"Command expected after '/'. Try /help."},
        };
    }

    const std::string command = lowerCopy(tokens.front());
    if (command == "help")
    {
        return {
            .handled = true,
            .succeeded = true,
            .feedbackLines =
                {
                    "Commands:",
                    "/tp <x> <y> <z> or /tp <~dx> <~dy> <~dz>",
                    "/spawnpoint [x y z]",
                    "/gamemode <creative|survival>",
                    "/give <item_id> [count]",
                    "/time [set] <day|noon|night|midnight|seconds>",
                    "/weather <clear|cloudy|rain>",
                },
        };
    }

    if (command == "tp" || command == "teleport")
    {
        if (!context.allowTeleport)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Teleport is unavailable right now."},
            };
        }
        if (tokens.size() != 4)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /tp <x> <y> <z> or /tp <~dx> <~dy> <~dz>"},
            };
        }

        const std::optional<float> x = parseRelativeCoordinate(tokens[1], context.currentFeetPosition.x);
        const std::optional<float> y = parseRelativeCoordinate(tokens[2], context.currentFeetPosition.y);
        const std::optional<float> z = parseRelativeCoordinate(tokens[3], context.currentFeetPosition.z);
        if (!x.has_value() || !y.has_value() || !z.has_value())
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Invalid teleport coordinates. Example: /tp 10 80 -4 or /tp ~ ~1 ~-3"},
            };
        }

        const float maxFeetY = static_cast<float>(context.maxWorldY) + 1.0f;
        if (*y < static_cast<float>(context.minWorldY) || *y > maxFeetY)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {
                    fmt::format("Teleport Y must stay between {} and {:.0f}.", context.minWorldY, maxFeetY)},
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .feedbackLines = {fmt::format("Teleported to {:.2f} {:.2f} {:.2f}.", *x, *y, *z)},
            .teleportFeetPosition = glm::vec3(*x, *y, *z),
        };
    }

    if (command == "spawnpoint")
    {
        glm::vec3 targetFeet = context.currentFeetPosition;
        if (tokens.size() == 4)
        {
            const std::optional<float> x = parseRelativeCoordinate(tokens[1], context.currentFeetPosition.x);
            const std::optional<float> y = parseRelativeCoordinate(tokens[2], context.currentFeetPosition.y);
            const std::optional<float> z = parseRelativeCoordinate(tokens[3], context.currentFeetPosition.z);
            if (!x.has_value() || !y.has_value() || !z.has_value())
            {
                return {
                    .handled = true,
                    .succeeded = false,
                    .feedbackLines = {"Usage: /spawnpoint or /spawnpoint <x> <y> <z>"},
                };
            }
            targetFeet = glm::vec3(*x, *y, *z);
        }
        else if (tokens.size() != 1)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /spawnpoint or /spawnpoint <x> <y> <z>"},
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .feedbackLines = {fmt::format(
                "Spawnpoint set to {:.2f} {:.2f} {:.2f}.",
                targetFeet.x,
                targetFeet.y,
                targetFeet.z)},
            .spawnFeetPosition = targetFeet,
        };
    }

    if (command == "gamemode" || command == "gm")
    {
        if (tokens.size() != 2)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /gamemode <creative|survival>"},
            };
        }

        const std::string mode = normalizeIdentifier(tokens[1]);
        if (mode == "creative" || mode == "c" || mode == "1")
        {
            return {
                .handled = true,
                .succeeded = true,
                .feedbackLines = {"Set own gamemode to Creative."},
                .creativeModeEnabled = true,
            };
        }
        if (mode == "survival" || mode == "s" || mode == "0")
        {
            return {
                .handled = true,
                .succeeded = true,
                .feedbackLines = {"Set own gamemode to Survival."},
                .creativeModeEnabled = false,
            };
        }

        return {
            .handled = true,
            .succeeded = false,
            .feedbackLines = {"Unknown gamemode. Use creative or survival."},
        };
    }

    if (command == "give")
    {
        if (tokens.size() < 2)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /give <item name or id> [count]"},
            };
        }

        std::uint32_t count = 1;
        std::size_t itemTokenEnd = tokens.size();
        if (tokens.size() >= 3)
        {
            std::uint32_t parsedCount = 0;
            if (parseUnsignedToken(tokens.back(), parsedCount))
            {
                if (parsedCount == 0)
                {
                    return {
                        .handled = true,
                        .succeeded = false,
                        .feedbackLines = {"Give count must be a positive whole number."},
                    };
                }
                count = parsedCount;
                itemTokenEnd = tokens.size() - 1;
            }
        }
        if (itemTokenEnd <= 1)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /give <item name or id> [count]"},
            };
        }

        std::string itemQuery = tokens[1];
        for (std::size_t index = 2; index < itemTokenEnd; ++index)
        {
            itemQuery += " ";
            itemQuery += tokens[index];
        }

        const CatalogEntry* const entry = findCatalogEntry(itemQuery);
        if (entry == nullptr)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {fmt::format("Unknown item '{}'. Try /give oak planks 64.", itemQuery)},
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .giveStacks =
                {{
                    .blockType = entry->blockType,
                    .equippedItem = entry->equippedItem,
                    .count = count,
                    .displayLabel = entry->displayLabel,
                }},
        };
    }

    if (command == "time")
    {
        const std::size_t presetIndex = tokens.size() >= 2 && lowerCopy(tokens[1]) == "set" ? 2 : 1;
        if (tokens.size() <= presetIndex || tokens.size() > presetIndex + 1)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /time [set] <day|noon|night|midnight|seconds>"},
            };
        }

        const std::optional<TimePreset> preset = parseTimePreset(tokens[presetIndex]);
        if (!preset.has_value())
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Unknown time preset. Use day, noon, night, midnight, or a number."},
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .requiresHostAuthority = context.globalWorldStateRequiresHostAuthority,
            .feedbackLines = {fmt::format("Time set to {}.", preset->label)},
            .dayNightElapsedSeconds = preset->elapsedSeconds,
        };
    }

    if (command == "weather")
    {
        if (tokens.size() != 2)
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Usage: /weather <clear|cloudy|rain>"},
            };
        }

        const std::optional<std::pair<float, std::string>> preset = parseWeatherPreset(tokens[1]);
        if (!preset.has_value())
        {
            return {
                .handled = true,
                .succeeded = false,
                .feedbackLines = {"Unknown weather preset. Use clear, cloudy, or rain."},
            };
        }

        return {
            .handled = true,
            .succeeded = true,
            .requiresHostAuthority = context.globalWorldStateRequiresHostAuthority,
            .feedbackLines = {fmt::format("Weather set to {}.", preset->second)},
            .weatherElapsedSeconds = preset->first,
        };
    }

    return {
        .handled = true,
        .succeeded = false,
        .feedbackLines = {fmt::format("Unknown command: /{}. Try /help.", command)},
    };
}

ChatAutocompleteResult autocompleteChatInput(std::string_view input, std::size_t cursorIndex)
{
    if (input.empty() || input.front() != '/')
    {
        return {};
    }

    const std::string ownedInput(input);
    cursorIndex = std::min(cursorIndex, ownedInput.size());
    const std::vector<TokenRange> ranges = splitTokenRanges(std::string_view(ownedInput).substr(1));
    const std::size_t cursorSansSlash = cursorIndex == 0 ? 0 : cursorIndex - 1;

    std::size_t tokenIndex = ranges.size();
    std::size_t tokenStart = cursorIndex;
    std::size_t tokenEnd = cursorIndex;
    for (std::size_t index = 0; index < ranges.size(); ++index)
    {
        const TokenRange& rangeSansSlash = ranges[index];
        const TokenRange range{rangeSansSlash.start + 1, rangeSansSlash.end + 1};
        if (cursorIndex >= range.start && cursorIndex <= range.end)
        {
            tokenIndex = index;
            tokenStart = range.start;
            tokenEnd = range.end;
            break;
        }
        if (cursorSansSlash < rangeSansSlash.start)
        {
            tokenIndex = index;
            tokenStart = cursorIndex;
            tokenEnd = cursorIndex;
            break;
        }
    }

    if (tokenIndex == ranges.size())
    {
        tokenStart = cursorIndex;
        tokenEnd = cursorIndex;
    }

    const std::string command =
        ranges.empty() ? std::string() : ownedInput.substr(ranges.front().start + 1, ranges.front().end - ranges.front().start);
    const std::vector<std::string> candidates = commandCandidates(command, tokenIndex);
    if (candidates.empty())
    {
        return {};
    }

    const std::string currentToken = tokenEnd > tokenStart ? ownedInput.substr(tokenStart, tokenEnd - tokenStart) : std::string();
    const std::string currentTokenLower = lowerCopy(currentToken);

    std::vector<std::string> matches;
    for (const std::string& candidate : candidates)
    {
        if (candidate.starts_with(currentTokenLower))
        {
            matches.push_back(candidate);
        }
    }
    if (matches.empty())
    {
        return {
            .hintLine = "No autocomplete match.",
        };
    }

    std::sort(matches.begin(), matches.end());
    const std::string replacement =
        matches.size() == 1 ? matches.front() : sharedPrefix(matches);
    ChatAutocompleteResult result;
    result.updatedInput = ownedInput;
    result.updatedCursorIndex = cursorIndex;

    if (!replacement.empty() && replacement.size() > currentTokenLower.size())
    {
        result.updatedInput.replace(tokenStart, tokenEnd - tokenStart, replacement);
        result.updatedCursorIndex = tokenStart + replacement.size();
        result.applied = true;
    }

    if (matches.size() == 1)
    {
        if (result.updatedCursorIndex == result.updatedInput.size()
            || std::isspace(static_cast<unsigned char>(result.updatedInput[result.updatedCursorIndex])) == 0)
        {
            result.updatedInput.insert(result.updatedCursorIndex, " ");
        }
        ++result.updatedCursorIndex;
        result.applied = true;
    }
    else
    {
        result.hintLine = joinSuggestionHint(matches);
    }

    return result;
}
}  // namespace vibecraft::app
