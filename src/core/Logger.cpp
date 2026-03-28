#include "vibecraft/core/Logger.hpp"

#include <fmt/base.h>
#include <cstdio>

namespace vibecraft::core
{
void initializeLogger()
{
    std::fflush(stdout);
}

void logInfo(const std::string_view message)
{
    fmt::print(stdout, "[info] {}\n", message);
}

void logWarning(const std::string_view message)
{
    fmt::print(stdout, "[warn] {}\n", message);
}

void logError(const std::string_view message)
{
    fmt::print(stderr, "[error] {}\n", message);
}
}  // namespace vibecraft::core
