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

//******************************************************************************
// ConsoleLoggerOptions
//******************************************************************************

CPPWAMP_INLINE ConsoleLoggerOptions&
ConsoleLoggerOptions::withOriginLabel(std::string originLabel)
{
    originLabel_ = std::move(originLabel);
    return *this;
}

CPPWAMP_INLINE ConsoleLoggerOptions&
ConsoleLoggerOptions::withFlushOnWrite(bool enabled)
{
    flushOnWriteEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE ConsoleLoggerOptions&
ConsoleLoggerOptions::withColor(bool enabled)
{
    colorEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE const std::string& ConsoleLoggerOptions::originLabel() const
{
    return originLabel_;
}

CPPWAMP_INLINE bool ConsoleLoggerOptions::flushOnWriteEnabled() const
{
    return flushOnWriteEnabled_;
}

CPPWAMP_INLINE bool ConsoleLoggerOptions::colorEnabled() const
{
    return colorEnabled_;
}


//******************************************************************************
// ConsoleLogger
//******************************************************************************

//------------------------------------------------------------------------------
struct ConsoleLogger::Impl
{
    explicit Impl(ConsoleLoggerOptions options) : options(std::move(options)) {}

    ConsoleLoggerOptions options;
};

CPPWAMP_INLINE ConsoleLogger::ConsoleLogger(Options options)
    : impl_(std::make_shared<Impl>(std::move(options)))
{}

CPPWAMP_INLINE void ConsoleLogger::operator()(const LogEntry& entry) const
{
    const auto& opts = impl_->options;
    if (entry.severity() < LogLevel::warning)
    {
        if (opts.colorEnabled())
            toColorStream(std::clog, entry, opts.originLabel()) << "\n";
        else
            toStream(std::clog, entry, opts.originLabel()) << "\n";
        if (opts.flushOnWriteEnabled())
            std::clog << std::flush;
    }
    else
    {
        if (opts.colorEnabled())
            toColorStream(std::cerr, entry, opts.originLabel()) << std::endl;
        else
            toStream(std::cerr, entry, opts.originLabel()) << std::endl;
    }
}

CPPWAMP_INLINE void ConsoleLogger::operator()(const AccessLogEntry& entry) const
{
    const auto& opts = impl_->options;

    if (opts.colorEnabled())
        toColorStream(std::clog, entry) << "\n";
    else
        toStream(std::clog, entry) << "\n";

    if (opts.flushOnWriteEnabled())
        std::clog << std::flush;
}

} // namespace utils

} // namespace wamp
