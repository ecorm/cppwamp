/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_LOGTEE_HPP
#define CPPWAMP_UTILS_LOGTEE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for splitting log output. */
//------------------------------------------------------------------------------

#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>
#include "../accesslogging.hpp"
#include "../logging.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Forwards a log entry to multiple loggers. */
//------------------------------------------------------------------------------
template <typename TEntry>
class BasicLogTee
{
public:
    using Entry = TEntry;
    using Logger = std::function<void (const Entry&)>;
    using LoggerList = std::vector<Logger>;

    /** Constructor taking a list of loggers. */
    explicit BasicLogTee(LoggerList loggers)
        : loggers_(std::make_shared<LoggerList>(std::move(loggers)))
    {}

    /** Forwards the given log entry to the attached loggers. */
    void operator()(const Entry& entry) const
    {
        for (const auto& logger: loggers_)
            logger(entry);
    }

private:
    // Make this object cheap to copy around
    std::shared_ptr<LoggerList> loggers_;
};

//------------------------------------------------------------------------------
/** Tee for loggers taking LogEntry objects. */
//------------------------------------------------------------------------------
using LogTee = BasicLogTee<LogEntry>;

//------------------------------------------------------------------------------
/** Tee for loggers taking AccessLogEntry objects. */
//------------------------------------------------------------------------------
using AccessLogTee = BasicLogTee<AccessLogEntry>;

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_LOGTEE_HPP
