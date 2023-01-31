/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../utils/consolelogger.hpp"
#include "../api.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE ConsoleLogger::ConsoleLogger()
    : origin_("cppwamp")
{}

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(std::string originLabel)
    : origin_(std::move(originLabel))
{}

CPPWAMP_INLINE void ConsoleLogger::operator()(const LogEntry& entry) const
{
    if (entry.severity() < LogLevel::warning)
        toStream(std::clog, entry, origin_) << "\n";
    else
        toStream(std::cerr, entry, origin_) << std::endl;
}

CPPWAMP_INLINE void ConsoleLogger::operator()(const AccessLogEntry& entry) const
{
    toStream(std::clog, entry, origin_) << "\n";
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger()
    : origin_("cppwamp")
{}

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(std::string originLabel)
    : origin_(std::move(originLabel))
{}

CPPWAMP_INLINE void ColorConsoleLogger::operator()(const LogEntry& entry) const
{
    if (entry.severity() < LogLevel::warning)
        toColorStream(std::clog, entry, origin_) << "\n";
    else
        toColorStream(std::cerr, entry, origin_) << std::endl;
}

CPPWAMP_INLINE void
ColorConsoleLogger::operator()(const AccessLogEntry& entry) const
{
    toColorStream(std::clog, entry, origin_) << "\n";
}

} // namespace utils

} // namespace wamp
