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
    Impl(std::string origin, bool flushOnWrite)
        : origin(std::move(origin)),
          flushOnWrite(flushOnWrite)
    {}

    std::string origin;
    bool flushOnWrite = false;
};

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger() = default;

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(bool flushOnWrite)
    : ConsoleLogger(std::string{"cppwamp"}, flushOnWrite)
{}

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(std::string originLabel,
                                            bool flushOnWrite)
    : impl_(std::make_shared<Impl>(std::move(originLabel), flushOnWrite))
{}

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(const char* originLabel,
                                            bool flushOnWrite)
    : ConsoleLogger(std::string{originLabel}, flushOnWrite)
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
    toStream(std::clog, entry) << "\n";
    if (impl.flushOnWrite)
        std::clog << std::flush;
}

//------------------------------------------------------------------------------
struct ColorConsoleLogger::Impl
{
    Impl(std::string origin, bool flushOnWrite)
        : origin(std::move(origin)),
          flushOnWrite(flushOnWrite)
    {}

    std::string origin;
    bool flushOnWrite = false;
};

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(bool flushOnWrite)
    : ColorConsoleLogger(std::string{"cppwamp"}, flushOnWrite)
{}

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(std::string originLabel,
                                                      bool flushOnWrite)
    : impl_(std::make_shared<Impl>(std::move(originLabel), flushOnWrite))
{}

CPPWAMP_INLINE ColorConsoleLogger::ColorConsoleLogger(const char* originLabel,
                                                      bool flushOnWrite)
    : ColorConsoleLogger(std::string{originLabel}, flushOnWrite)
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
    toColorStream(std::clog, entry) << "\n";
    if (impl.flushOnWrite)
        std::clog << std::flush;
}

} // namespace utils

} // namespace wamp
