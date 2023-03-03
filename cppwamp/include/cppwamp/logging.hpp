/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LOGGING_HPP
#define CPPWAMP_LOGGING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for logging. */
//------------------------------------------------------------------------------

#include <chrono>
#include <functional>
#include <ostream>
#include <set>
#include <string>
#include <system_error>
#include "api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Log entry severity levels.
    These match the levels of the popular
    [gabime/spdlog](https://github.com/gabime/spdlog) library. */
//------------------------------------------------------------------------------
enum class CPPWAMP_API LogLevel
{
    trace,    ///< For sent/received WAMP message dumps
    debug,    ///< For debugging information intended for developers
    info,     ///< For general events
    warning,  ///< For detected problems that don't affect normal operation
    error,    ///< For failures where operations cannot be completed
    critical, ///< For failures that require shutdown of services
    off       ///< Used to disable all log events
};

//------------------------------------------------------------------------------
const std::string& logLevelLabel(LogLevel lv);

//------------------------------------------------------------------------------
/** Contains logging information. */
//------------------------------------------------------------------------------
class CPPWAMP_API LogEntry
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /** Outputs a timestamp in RFC3339 format. */
    static std::ostream& outputTime(std::ostream& out, TimePoint when);

    /** Constructor. */
    LogEntry(LogLevel severity, std::string message, std::error_code ec = {});

    /** Obtains the entry's severity level. */
    LogLevel severity() const;

    /** Obtains the entry's information text. */
    const std::string& message() const &;

    /** Moves the entry's information text. */
    std::string&& message() &&;

    /** Appends the given text to the entry's information text. */
    LogEntry& append(std::string extra);

    /** Obtains the error code associated with this entry, if applicable. */
    const std::error_code& error() const;

    /** Obtains the entry's timestamp. */
    TimePoint when() const;

private:
    std::string message_;
    std::error_code ec_;
    TimePoint when_;
    LogLevel severity_ = LogLevel::off;
};

/** Obtains a formatted log entry string combining all available information.
    @relates LogEntry */
std::string toString(const LogEntry& entry);

/** Obtains a formatted log entry string with a custom origin field.
    @relates LogEntry */
std::string toString(const LogEntry& entry, const std::string& origin);

/** Outputs a formatted log entry combining all available information.
    @relates LogEntry */
std::ostream& toStream(std::ostream& out, const LogEntry& entry);

/** Outputs a formatted log entry with a custom origin field.
    @relates LogEntry */
std::ostream& toStream(std::ostream& out, const LogEntry& entry,
                       const std::string& origin);

/** Outputs a formatted log entry using ANSI color escape codes.
    @relates LogEntry */
std::ostream& toColorStream(std::ostream& out, const LogEntry& entry);

/** Outputs a formatted, colored log entry with a custom origin field.
    @relates LogEntry */
std::ostream& toColorStream(std::ostream& out, const LogEntry& entry,
                            std::string origin);

/** Outputs a LogEntry to an output stream.
    @relates LogEntry */
std::ostream& operator<<(std::ostream& out, const LogEntry& entry);


namespace internal
{

std::ostream& outputLogEntryTime(std::ostream& out,
                                 std::chrono::system_clock::time_point when);

}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/logging.ipp"
#endif

#endif // CPPWAMP_LOGGING_HPP_HPP
