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
struct ConsoleLogger::Impl
{
    Impl(std::string origin) : origin(std::move(origin)) {}

    std::string origin;
    bool flushOnWrite = false;
};

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(bool flushOnWrite)
    : ConsoleLogger("cppwamp", flushOnWrite)
{}

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(std::string originLabel,
                                            bool flushOnWrite)
    : impl_(std::make_shared<Impl>(std::move(originLabel)))
{}

CPPWAMP_INLINE void ConsoleLogger::operator()(const LogEntry& entry) const
{
    auto& impl = *impl_;
    if (entry.severity() < LogLevel::warning)
    {
        toStream(std::clog, entry, impl.origin) << "\n";
        if (impl.flushOnWrite)
            std::clog << std::flush;
    }
    else
    {
        toStream(std::cerr, entry, impl.origin) << std::endl;
    }
}

CPPWAMP_INLINE void ConsoleLogger::operator()(const AccessLogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(std::clog, entry, impl.origin) << "\n";
    if (impl.flushOnWrite)
        std::clog << std::flush;
}

//------------------------------------------------------------------------------
struct ColorConsoleLogger::Impl
{
    Impl(std::string origin) : origin(std::move(origin)) {}

    std::string origin;
    bool flushOnWrite = false;
};

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(bool flushOnWrite)
    : ColorConsoleLogger("cppwamp", flushOnWrite)
{}

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(std::string originLabel,
                                                      bool flushOnWrite)
    : impl_(std::make_shared<Impl>(std::move(originLabel)))
{}

CPPWAMP_INLINE void ColorConsoleLogger::operator()(const LogEntry& entry) const
{
    auto& impl = *impl_;
    if (entry.severity() < LogLevel::warning)
    {
        toColorStream(std::clog, entry, impl.origin) << "\n";
        if (impl.flushOnWrite)
            std::clog << std::flush;
    }
    else
    {
        toColorStream(std::cerr, entry, impl.origin) << std::endl;
    }
}

CPPWAMP_INLINE void
ColorConsoleLogger::operator()(const AccessLogEntry& entry) const
{
    auto& impl = *impl_;
    toColorStream(std::clog, entry, impl.origin) << "\n";
    if (impl.flushOnWrite)
        std::clog << std::flush;
}

} // namespace utils

} // namespace wamp
