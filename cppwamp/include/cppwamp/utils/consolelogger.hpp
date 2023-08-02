/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_CONSOLELOGGER_HPP
#define CPPWAMP_UTILS_CONSOLELOGGER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for logging to the console. */
//------------------------------------------------------------------------------

#include <iostream>
#include <memory>
#include "../accesslogging.hpp"
#include "../api.hpp"
#include "../logging.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Contains options for use with wamp::ConsoleLogger */
//------------------------------------------------------------------------------
class CPPWAMP_API ConsoleLoggerOptions
{
public:
    /** Sets the origin label. */
    ConsoleLoggerOptions& withOriginLabel(std::string originLabel);

    /** Flush the stream immediately after all log entries, regardless
        of severity. */
    ConsoleLoggerOptions& withFlushOnWrite(bool enabled = true);

    /** Enables color output. */
    ConsoleLoggerOptions& withColor(bool enabled = true);

    /** Obtains the origin label. */
    const std::string& originLabel() const;

    /** Determines if flush-on-write was enabled. */
    bool flushOnWriteEnabled() const;

    /** Determines if color output was enabled. */
    bool colorEnabled() const;

private:
    std::string originLabel_;
    bool flushOnWriteEnabled_;
    bool colorEnabled_;
};

//------------------------------------------------------------------------------
/** Outputs log entries to the console.
    The format is per wamp::toString(const LogEntry&).
    Entries below LogLevel::warning are output to std::clog, and all others
    are output to std::cerr. Concurrent output operations are not serialized. */
//------------------------------------------------------------------------------
class CPPWAMP_API ConsoleLogger
{
public:
    using Options = ConsoleLoggerOptions;

    /** Constructor taking options. */
    explicit ConsoleLogger(Options options = {});

    /** Outputs the given log entry to the console. */
    void operator()(const LogEntry& entry) const;

    /** Outputs the given access log entry to the console. */
    void operator()(const AccessLogEntry& entry) const;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_; // Cheap to copy
};

} // namespace utils

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/consolelogger.inl.hpp"
#endif

#endif // CPPWAMP_UTILS_CONSOLELOGGER_HPP
