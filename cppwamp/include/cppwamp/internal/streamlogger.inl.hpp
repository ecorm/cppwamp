/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../utils/streamlogger.hpp"
#include "../api.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
struct StreamLogger::Impl
{
    Impl(std::ostream& output, std::string originLabel)
        : origin(std::move(originLabel)),
          output(&output)
    {}

    std::string origin;
    std::ostream* output = nullptr;
    bool flushOnWrite = false;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE StreamLogger::StreamLogger(std::ostream& output)
    : StreamLogger(output, "cppwamp")
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE StreamLogger::StreamLogger(std::ostream& output,
                                          std::string originLabel)
    : impl_(std::make_shared<Impl>(output, std::move(originLabel)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void StreamLogger::operator()(const LogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(*impl.output, entry, impl.origin) << "\n";
    if ((entry.severity() >= LogLevel::warning) || impl.flushOnWrite)
        *impl.output << std::flush;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void StreamLogger::operator()(const AccessLogEntry& entry) const
{
    auto& impl = *impl_;
    toStream(*impl.output, entry) << "\n";
    if (impl.flushOnWrite)
        *impl.output << std::flush;
}

} // namespace utils

} // namespace wamp
