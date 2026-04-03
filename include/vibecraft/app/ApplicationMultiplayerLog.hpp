#pragma once

#include <fmt/format.h>
#include <utility>

#include "vibecraft/core/Logger.hpp"

#if !defined(NDEBUG) || defined(VIBECRAFT_DEBUG_MULTIPLAYER_JOIN)
template<typename... Args>
void logMultiplayerJoinDiag(fmt::format_string<Args...> fmtStr, Args&&... args)
{
    vibecraft::core::logInfo(fmt::format("[mp-join] {}", fmt::format(fmtStr, std::forward<Args>(args)...)));
}
#else
template<typename... Args>
void logMultiplayerJoinDiag(fmt::format_string<Args...>, Args&&...)
{
}
#endif
