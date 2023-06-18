/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_FILELOGGER_HPP
#define CPPWAMP_UTILS_FILELOGGER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for logging to a stream. */
//------------------------------------------------------------------------------

#include <memory>
#include "../accesslogging.hpp"
#include "../api.hpp"
#include "../logging.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Outputs log entries to a file.
    The format is per wamp::toString(const LogEntry&).
    Concurrent output operations are not serialized. */
//------------------------------------------------------------------------------
class CPPWAMP_API FileLogger
{
public:
    /** Constructor taking a filepath. */
    explicit FileLogger(const std::string& filepath, bool truncate = false);

    /** Constructor taking a path and a custom origin label. */
    FileLogger(const std::string& filepath, std::string originLabel,
               bool truncate = false);

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
#include "../internal/filelogger.inl.hpp"
#endif

#endif // CPPWAMP_UTILS_FILELOGGER_HPP
