/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_LOGSEQUENCER_HPP
#define CPPWAMP_UTILS_LOGSEQUENCER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for serializing concurrent logger operations. */
//------------------------------------------------------------------------------

#include <functional>
#include "../accesslogging.hpp"
#include "../asiodefs.hpp"
#include "../logging.hpp"
#include "../traits.hpp"

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
/** Wrapper around a logger which serializes concurrent log operations. */
//------------------------------------------------------------------------------
template <typename TEntry>
class BasicLogSequencer
{
public:
    using Entry = TEntry;
    using Logger = std::function<void (const Entry&)>;

    /** Constructor taking an executor and a logger. */
    BasicLogSequencer(const AnyIoExecutor& executor, Logger logger)
        : strand_(boost::asio::make_strand(executor)),
          logger_(std::move(logger))
    {}

    /** Constructor taking an execution context and a logger. */
    template <typename E, CPPWAMP_NEEDS(isExecutionContext<E>()) = 0>
    BasicLogSequencer(E& executionContext, Logger logger)
        : strand_(boost::asio::make_strand(executionContext)),
          logger_(std::move(logger))
    {}

    /** Enqueues the given log entry. */
    void operator()(const Entry& entry) const
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(logger_, entry));
    }

private:
    IoStrand strand_;
    Logger logger_;
};

//------------------------------------------------------------------------------
/** Log sequencer for logger taking LogEntry objects. */
//------------------------------------------------------------------------------
using LogSequencer = BasicLogSequencer<LogEntry>;

//------------------------------------------------------------------------------
/** Log sequencer for logger taking AccessLogEntry objects. */
//------------------------------------------------------------------------------
using AccessLogSequencer = BasicLogSequencer<AccessLogEntry>;

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_LOGSEQUENCER_HPP
