/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTER_LOGGER_HPP
#define CPPWAMP_ROUTER_LOGGER_HPP

#include <atomic>
#include <memory>
#include <utility>
#include "accesslogging.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "logging.hpp"

namespace wamp
{

namespace internal { class RouterImpl; }

//------------------------------------------------------------------------------
class RouterLogger
{
public:
    using Ptr = std::shared_ptr<RouterLogger>;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    RouterLogger(AnyIoExecutor e, LogHandler lh, LogLevel lv,
                 AccessLogHandler alh)
        : executor_(std::move(e)),
          logHandler_(std::move(lh)),
          accessLogHandler_(std::move(alh)),
          logLevel_(lv)
    {}

    LogLevel level() const {return logLevel_.load();}

    void log(LogEntry entry)
    {
        if (logHandler_ && entry.severity() >= level())
            postAny(executor_, logHandler_, std::move(entry));
    }

    void log(AccessLogEntry entry)
    {
        if (accessLogHandler_)
            postAny(executor_, accessLogHandler_, std::move(entry));
    }

private:
    void setLevel(LogLevel level) {logLevel_.store(level);}

    AnyIoExecutor executor_;
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    std::atomic<LogLevel> logLevel_;

    friend class internal::RouterImpl;
};

//------------------------------------------------------------------------------
class AdmissionLogger
{
public:
    using Ptr = std::shared_ptr<AdmissionLogger>;

    AdmissionLogger() = default;

    explicit AdmissionLogger(ConnectionInfo i, RouterLogger::Ptr l)
        : info_(std::move(i)),
          logger_(std::move(l))
    {}

    LogLevel level() const {return !logger_ ? LogLevel::off : logger_->level();}

    void report(AccessActionInfo action)
    {
        if (logger_)
            logger_->log(AccessLogEntry{info_, {}, action});
    }

private:
    ConnectionInfo info_;
    RouterLogger::Ptr logger_;
};

} // namespace wamp

#endif // CPPWAMP_ROUTER_LOGGER_HPP
