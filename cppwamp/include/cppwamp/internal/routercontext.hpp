/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
#define CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP

#include <atomic>
#include <future>
#include <memory>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../erroror.hpp"
#include "../logging.hpp"
#include "../peerdata.hpp"
#include "idgen.hpp"
#include "wampmessage.hpp"

namespace wamp
{

namespace internal
{

class ServerSession;
class RouterSession;
class RouterRealm;
class RouterImpl;

//------------------------------------------------------------------------------
class RouterLogger : public std::enable_shared_from_this<RouterLogger>
{
public:
    using Ptr = std::shared_ptr<RouterLogger>;
    using LogHandler = AnyReusableHandler<void (LogEntry)>;
    using AccessLogHandler = AnyReusableHandler<void (AccessLogEntry)>;

    static Ptr create(IoStrand s, LogHandler lh, LogLevel lv,
                      AccessLogHandler alh)
    {
        return Ptr(new RouterLogger(std::move(s), std::move(lh), lv,
                                    std::move(alh)));
    }

    LogLevel level() const {return logLevel_.load();}

    void log(LogEntry entry)
    {
        if (entry.severity() >= level())
            dispatchVia(strand_, logHandler_, std::move(entry));
    }

    void log(AccessLogEntry entry)
    {
        dispatchVia(strand_, accessLogHandler_, std::move(entry));
    }

private:
    RouterLogger(IoStrand&& s, LogHandler&& lh, LogLevel lv,
                 AccessLogHandler&& alh)
        : strand_(std::move(s)),
          logHandler_(std::move(lh)),
          accessLogHandler_(std::move(alh)),
          logLevel_(lv)
    {}

    void setLevel(LogLevel level) {logLevel_.store(level);}

    IoStrand strand_;
    LogHandler logHandler_;
    AccessLogHandler accessLogHandler_;
    std::atomic<LogLevel> logLevel_;

    friend class RouterImpl;
};

//------------------------------------------------------------------------------
class RealmContext
{
public:
    using RouterSessionPtr = std::shared_ptr<RouterSession>;

    RealmContext() = default;

    RealmContext(std::shared_ptr<RouterRealm> r);

    bool expired() const;

    explicit operator bool() const;

    IoStrand strand() const;

    RouterLogger::Ptr logger() const;

    void reset();

    void join(RouterSessionPtr s);

    void leave(SessionId sid);

    ErrorOr<SubscriptionId> subscribe(Topic t, RouterSessionPtr s);

    ErrorOrDone unsubscribe(SubscriptionId subId, SessionId sessionId);

    ErrorOr<PublicationId> publish(Pub pub, SessionId sid);

    ErrorOr<RegistrationId> enroll(Procedure proc, RouterSessionPtr s);

    ErrorOrDone unregister(RegistrationId rid, SessionId sid);

    ErrorOrDone call(Rpc rpc, SessionId sid);

    bool cancelCall(RequestId rid, SessionId sid);

    void yieldResult(Result r, SessionId sid);

    void yieldError(Error e, SessionId sid);

private:
    std::weak_ptr<RouterRealm> realm_;
};

//------------------------------------------------------------------------------
class RouterContext
{
public:
    static const Object& roles();

    RouterContext(std::shared_ptr<RouterImpl> r);

    RouterLogger::Ptr logger() const;

    ReservedId reserveSessionId();

    RealmContext realmAt(const String& uri) const;

private:
    std::weak_ptr<RouterImpl> router_;
    std::weak_ptr<RandomIdPool> sessionIdPool_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTER_CONTEXT_HPP
