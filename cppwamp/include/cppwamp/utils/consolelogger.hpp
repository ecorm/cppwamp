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
#include "../api.hpp"
#include "../accesslogging.hpp"
#include "../logging.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Outputs log entries to the console.
    The format is per wamp::toString(const LogEntry&).
    Entries below LogLevel::warning are output to std::clog, and all others
    are output to std::cerr. Concurrent output operations are not serialized. */
//------------------------------------------------------------------------------
class CPPWAMP_API ConsoleLogger
{
public:
    /** Default constructor. */
    ConsoleLogger();

    /** Constructor taking a custom origin label. */
    explicit ConsoleLogger(std::string originLabel);

    /** Outputs the given log entry to the console. */
    void operator()(const LogEntry& entry) const;

    /** Outputs the given access log entry to the console. */
    void operator()(const AccessLogEntry& entry) const;

private:
    std::string origin_;
};

//------------------------------------------------------------------------------
/** Outputs log entries to the console using ANSI color escape codes that
    depend on severity.
    The format is per wamp::toString(const LogEntry&).
    Entries below LogLevel::warning are output to std::clog, and all others
    are output to std::cerr. Concurrent output operations are not serialized. */
//------------------------------------------------------------------------------
class CPPWAMP_API ColorConsoleLogger
{
public:
    /** Default constructor. */
    ColorConsoleLogger();

    /** Constructor taking a custom origin label. */
    explicit ColorConsoleLogger(std::string originLabel);

    /** Outputs the given log entry to the console. */
    void operator()(const LogEntry& entry) const;

    /** Outputs the given access log entry to the console. */
    void operator()(const AccessLogEntry& entry) const;

private:
    std::string origin_;
};

} // namespace utils

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/consolelogger.ipp"
#endif

#endif // CPPWAMP_UTILS_CONSOLELOGGER_HPP
