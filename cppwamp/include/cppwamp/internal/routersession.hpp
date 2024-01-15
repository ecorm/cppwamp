/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ROUTERSESSION_HPP
#define CPPWAMP_INTERNAL_ROUTERSESSION_HPP

#include <atomic>
#include <memory>
#include "../clientinfo.hpp"
#include "../connectioninfo.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "../routerlogger.hpp"
#include "../sessioninfo.hpp"
#include "random.hpp"
#include "sessioninfoimpl.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RouterSession
{
public:
    using Ptr = std::shared_ptr<RouterSession>;
    using WeakPtr = std::weak_ptr<RouterSession>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    virtual ~RouterSession() = default;

    bool isJoined() const {return info_ != nullptr;}

    SessionId wampId() const {return wampId_.get();}

    const SessionInfoImpl& info() const
    {
        assert(isJoined());
        return *info_;
    }

    SessionInfo sharedInfo() const
    {
        assert(isJoined());
        return SessionInfo{{}, info_};
    }

    void setWampId(ReservedId&& id)
    {
        assert(info_ != nullptr);
        wampId_ = std::move(id);
        info_->setSessionId(wampId());
    }

    void report(AccessActionInfo&& action)
    {
        if (!logger_)
            return;
        logger_->log(AccessLogEntry{connectionInfo_, SessionInfo{{}, info_},
                                    std::move(action)});
    }

    void abort(Abort reason)
    {
        onRouterAbort(std::move(reason));
    }

    template <typename C, typename... Ts>
    void sendRouterCommand(C&& command, Ts&&... accessInfoArgs)
    {
        if (logger_)
        {
            auto info = command.info(std::forward<Ts>(accessInfoArgs)...);
            logger_->log(AccessLogEntry{connectionInfo_, SessionInfo{{}, info_},
                                        std::move(info)});
        }

        onRouterMessage(std::move(command.message({})));
    }

    template <typename C, typename E, typename... Ts>
    void sendRouterCommandError(const C& command, E errorCodeLike, Ts&&... args)
    {
        auto error = Error::fromRequest({}, command, errorCodeLike)
                         .withArgs(std::forward<Ts>(args)...);
        sendRouterCommand(std::move(error), true);
    }

    void sendEvent(const Event& e)
    {
        // server-event actions are not logged due to the potential large
        // number of observers. Instead, a recipient count is added to the
        // server-published action log.
        onRouterMessage(Message{e.message({})});
    }

    RequestId sendInvocation(Invocation&& inv, Uri&& topic)
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        auto id = ++nextOutboundRequestId_;
        assert(id <= 9007199254740992u);
        inv.setRequestId({}, id);
        sendRouterCommand(std::move(inv), std::move(topic));
        return id;
    }

    RequestId lastInsertedCallRequestId() const
    {
        return lastInsertedCallRequestId_.load();
    }

    void setLastInsertedCallRequestId(RequestId rid)
    {
        lastInsertedCallRequestId_.store(rid);
    }

    RouterSession(const RouterSession&) = delete;
    RouterSession(RouterSession&&) = delete;
    RouterSession& operator=(const RouterSession&) = delete;
    RouterSession& operator=(RouterSession&&) = delete;

protected:
    explicit RouterSession(RouterLogger::Ptr logger = nullptr)
        : logger_(std::move(logger)),
          nextOutboundRequestId_(0),
          lastInsertedCallRequestId_(0)
    {}

    virtual void onRouterAbort(Abort&&) = 0;

    virtual void onRouterMessage(Message&&) = 0;

    void setRouterLogger(RouterLogger::Ptr logger)
    {
        logger_ = std::move(logger);
    }

    LogLevel routerLogLevel() const
    {
        return logger_ ? logger_->level() : LogLevel::off;
    }

    void routerLog(LogEntry e)
    {
        if (!logger_)
            return;
        e.append(logSuffix_);
        logger_->log(std::move(e));
    }

    void connect(ConnectionInfo info)
    {
        logSuffix_ = " [Session " + info.server() + '/' +
                     std::to_string(info.serverSessionNumber()) + ']';
        connectionInfo_ = std::move(info);
    }

    void join(SessionInfoImpl::Ptr info)
    {
        info->setConnection(connectionInfo_);
        info_ = std::move(info);
    }

    void close()
    {
        wampId_.reset();
        info_.reset();
        nextOutboundRequestId_.store(0);
        lastInsertedCallRequestId_.store(0);
    }

    const ConnectionInfo& connectionInfo() const {return connectionInfo_;}

private:
    ConnectionInfo connectionInfo_;
    String logSuffix_;
    ReservedId wampId_;
    RouterLogger::Ptr logger_;
    SessionInfoImpl::Ptr info_;
    std::atomic<RequestId> nextOutboundRequestId_;
    std::atomic<RequestId> lastInsertedCallRequestId_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ROUTERSESSION_HPP
