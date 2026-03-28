#pragma once

#include <string_view>

namespace vibecraft::core
{
void initializeLogger();
void logInfo(std::string_view message);
void logWarning(std::string_view message);
void logError(std::string_view message);
}  // namespace vibecraft::core
