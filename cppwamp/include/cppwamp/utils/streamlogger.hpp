/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_STREAMLOGGER_HPP
#define CPPWAMP_UTILS_STREAMLOGGER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for logging to a stream. */
//------------------------------------------------------------------------------

#include <memory>
#include <ostream>
#include "../accesslogging.hpp"
#include "../api.hpp"
#include "../logging.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Outputs log entries to a stream.
    The format is per wamp::toString(const LogEntry&).
    Concurrent output operations are not serialized. */
//------------------------------------------------------------------------------
class CPPWAMP_API StreamLogger
{
public:
    /** Constructor taking a reference to an output stream. */
    explicit StreamLogger(std::ostream& output);

    /** Constructor taking a reference to an output stream
        and a custom origin label. */
    StreamLogger(std::ostream& output, std::string originLabel);

    /** Appends the given log entry to the stream. */
    void operator()(const LogEntry& entry) const;

    /** Appends the given access log entry to the stream. */
    void operator()(const AccessLogEntry& entry) const;

    /** Enables/disables flush-on-write */
    void enableFlushOnWrite(bool enabled = true);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace utils

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/streamlogger.ipp"
#endif

#endif // CPPWAMP_UTILS_STREAMLOGGER_HPP
