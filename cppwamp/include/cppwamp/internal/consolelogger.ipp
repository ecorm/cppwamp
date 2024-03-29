/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../consolelogger.hpp"
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE ConsoleLogger::ConsoleLogger()
    : origin_("cppwamp")
{}

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(std::string originLabel)
    : origin_(std::move(originLabel))
{}

CPPWAMP_INLINE void ConsoleLogger::operator()(LogEntry entry) const
{
    if (entry.severity() < LogLevel::warning)
        toStream(std::clog, entry, origin_) << "\n";
    else
        toStream(std::cerr, entry, origin_) << std::endl;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger()
    : origin_("cppwamp")
{}

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(std::string originLabel)
    : origin_(std::move(originLabel))
{}

CPPWAMP_INLINE void ColorConsoleLogger::operator()(LogEntry entry) const
{
    if (entry.severity() < LogLevel::warning)
        toColorStream(std::clog, entry, origin_) << "\n";
    else
        toColorStream(std::cerr, entry, origin_) << std::endl;
}

} // namespace wamp
