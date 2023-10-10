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
class ServerLogger
{
public:
    using Ptr = std::shared_ptr<ServerLogger>;

    ServerLogger(RouterLogger::Ptr routerLogger, const std::string& serverName)
        : logger_(std::move(routerLogger)),
          serverName_(serverName),
          logSuffix_(" [Server " + serverName + ']')
    {}

    const std::string& serverName() const {return serverName_;}

    LogLevel level() const {return logger_->level();}

    void log(LogEntry entry)
    {
        entry.append(logSuffix_);
        logger_->log(std::move(entry));
    }

    void log(AccessLogEntry entry)
    {
        logger_->log(std::move(entry));
    }

private:
    RouterLogger::Ptr logger_;
    std::string serverName_;
    std::string logSuffix_;
};

} // namespace wamp

#endif // CPPWAMP_ROUTER_LOGGER_HPP
