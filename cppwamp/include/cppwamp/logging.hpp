/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LOGGING_HPP
#define CPPWAMP_LOGGING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for reporting and describing errors. */
//------------------------------------------------------------------------------

#include <chrono>
#include <ostream>
#include <string>
#include "api.hpp"


namespace wamp
{

//------------------------------------------------------------------------------
enum class CPPWAMP_API LogLevel
{
    trace,
    debug,
    info,
    warning,
    error,
    critical,
    off
};

//------------------------------------------------------------------------------
const std::string& logLevelLabel(LogLevel lv);

//------------------------------------------------------------------------------
/** Contains error/warning information for logging purposes. */
//------------------------------------------------------------------------------
class CPPWAMP_API LogEntry
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /** Constructor. */
    LogEntry(LogLevel severity, std::string info, std::error_code ec = {});

    /** Obtains the entry's severity level. */
    LogLevel severity() const;

    /** Obtains the entry's information text. */
    const std::string& message() const &;

    /** Moves the entry's information text. */
    std::string&& message() &&;

    /** Obtains the error code associated with this entry, if applicable. */
    const std::error_code& error() const;

    /** Obtains the local host system time when the entry was generated. */
    TimePoint when() const;

    /** Obtains a human-readable message combining all available information. */
    std::string formatted(std::string origin = "cppwamp") const;

private:
    std::string info_;
    std::error_code ec_;
    TimePoint when_;
    LogLevel severity_ = LogLevel::off;
};

//------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const LogEntry& entry);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/logging.ipp"
#endif

#endif // CPPWAMP_LOGGING_HPP_HPP
