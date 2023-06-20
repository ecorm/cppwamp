/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REQUESTOR_HPP
#define CPPWAMP_INTERNAL_REQUESTOR_HPP

#include <cassert>
#include <map>
#include <utility>
#include "../asiodefs.hpp"
#include "../anyhandler.hpp"
#include "../callerstreaming.hpp"
#include "../erroror.hpp"
#include "../rpcinfo.hpp"
#include "peer.hpp"
#include "streamchannel.hpp"
#include "timeoutscheduler.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class StreamRecord
{
public:
    using TimeoutDuration = Rpc::TimeoutDuration;

    using CompletionHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;

    explicit StreamRecord(CallerChannelImpl::Ptr c, StreamRequest& i,
                          CompletionHandler&& f = {})
        : handler_(std::move(f)),
        weakChannel_(c),
        errorPtr_(i.error({})),
        timeout_(i.callerTimeout())
    {
        if (handler_)
            channel_ = std::move(c);
    }

    void onReply(Message&& msg, AnyIoExecutor& exec)
    {
        if (msg.kind() == MessageKind::result)
            onResult(std::move(msg), exec);
        else
            onError(std::move(msg), exec);
    }

    void cancel(AnyIoExecutor& exec, WampErrc errc)
    {
        abandon(makeUnexpectedError(errc), exec);
    }

    void abandon(UnexpectedError unex, AnyIoExecutor& exec)
    {
        if (handler_)
            postAny(exec, std::move(handler_), unex);
        else if (channel_)
            channel_->abandon(unex);
        else if (auto ch = weakChannel_.lock())
            ch->abandon(unex);

        handler_ = nullptr;
        channel_.reset();
        weakChannel_.reset();
    }

    bool hasTimeout() const {return timeout_.count() != 0;}

    TimeoutDuration timeout() const {return timeout_;}

private:
    explicit StreamRecord(CallerChannelImpl::Ptr c, Error* e,
                          CompletionHandler&& f)
        : handler_(std::move(f)),
        channel_(std::move(c)),
        weakChannel_(channel_),
        errorPtr_(e)
    {}

    void onResult(Message&& msg, AnyIoExecutor& exec)
    {
        if (channel_)
        {
            if (channel_->expectsRsvp())
                channel_->setRsvp(std::move(msg));

            if (handler_)
            {
                dispatchAny(exec, std::move(handler_),
                            CallerChannel{{}, channel_});
                handler_ = nullptr;
            }

            if (!channel_->expectsRsvp())
                channel_->postResult(std::move(msg));

            channel_.reset();
        }
        else
        {
            auto channel = weakChannel_.lock();
            if (channel)
                channel->postResult(std::move(msg));
        }
    }

    void onError(Message&& msg, AnyIoExecutor& exec)
    {
        if (channel_)
        {
            if (handler_)
            {
                Error error{{}, std::move(msg)};
                auto unex = makeUnexpectedError(error.errorCode());
                if (errorPtr_)
                    *errorPtr_ = std::move(error);
                dispatchAny(exec, std::move(handler_), unex);
                handler_ = nullptr;
            }
            else
            {
                channel_->postError(std::move(msg));
            }
            channel_.reset();
        }
        else
        {
            auto channel = weakChannel_.lock();
            if (channel)
                channel->postError(std::move(msg));
        }
    }

    CompletionHandler handler_;
    CallerChannelImpl::Ptr channel_;
    CallerChannelImpl::WeakPtr weakChannel_;
    Error* errorPtr_ = nullptr;
    TimeoutDuration timeout_ = {};
};

//------------------------------------------------------------------------------
class Requestor
{
public:
    using TimeoutDuration = typename Rpc::TimeoutDuration;
    using RequestHandler = AnyCompletionHandler<void (ErrorOr<Message>)>;
    using StreamRequestHandler =
        AnyCompletionHandler<void (ErrorOr<CallerChannel>)>;
    using ChunkSlot = AnyReusableHandler<void (CallerChannel,
                                               ErrorOr<CallerInputChunk>)>;

    Requestor(Peer* peer, IoStrand strand, AnyIoExecutor exec,
              AnyCompletionExecutor fallbackExec)
        : deadlines_(CallerTimeoutScheduler::create(strand)),
          strand_(std::move(strand)),
          executor_(std::move(exec)),
          fallbackExecutor_(std::move(fallbackExec)),
          peer_(peer)
    {
        deadlines_->listen(
            [this](RequestId reqId)
            {
                cancelCall(reqId, CallCancelMode::killNoWait,
                           WampErrc::timeout);
            });
    }

    template <typename C>
    ErrorOr<RequestId> request(C&& command, RequestHandler&& handler)
    {
        return request(std::move(command), TimeoutDuration{0},
                       std::move(handler));
    }

    template <typename C>
    ErrorOr<RequestId> request(C&& command, TimeoutDuration timeout,
                               RequestHandler&& handler)
    {

        using HasRequestId = MetaBool<ValueTypeOf<C>::hasRequestId({})>;
        return doRequest(HasRequestId{}, command, timeout, std::move(handler));
    }

    ErrorOr<CallerChannel> requestStream(
        bool rsvpExpected, ClientContext caller, StreamRequest&& req,
        ChunkSlot&& onChunk, StreamRequestHandler&& handler = {})
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        assert(nextRequestId_ < 9007199254740992u);
        ChannelId channelId = nextRequestId_ + 1;
        auto uri = req.uri();
        req.setRequestId({}, channelId);

        auto sent = peer_->send(std::move(req));
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        ++nextRequestId_;

        auto channel = std::make_shared<CallerChannelImpl>(
            channelId, std::move(uri), req.mode(), req.cancelMode(),
            rsvpExpected, std::move(caller), std::move(onChunk), executor_,
            fallbackExecutor_);
        auto emplaced = channels_.emplace(
            channelId, StreamRecord{channel, req, std::move(handler)});
        assert(emplaced.second);

        if (req.callerTimeout().count() != 0)
            deadlines_->insert(channel->id(), req.callerTimeout());

        return CallerChannel{{}, std::move(channel)};
    }

    bool onReply(Message&& msg) // Returns true if request was found
    {
        assert(msg.isReply());
        auto key = msg.requestKey();

        {
            auto kv = requests_.find(key);
            if (kv != requests_.end())
            {
                auto handler = std::move(kv->second);
                requests_.erase(kv);
                if (key.first == MessageKind::call)
                    deadlines_->erase(key.second);
                completeRequest(handler, std::move(msg));
                return true;
            }
        }

        {
            if (key.first != MessageKind::call)
                return false;
            auto kv = channels_.find(key.second);
            if (kv == channels_.end())
                return false;

            if (msg.isProgress())
            {
                StreamRecord& rec = kv->second;
                rec.onReply(std::move(msg), executor_);
                deadlines_->update(key.second, rec.timeout());
            }
            else
            {
                StreamRecord rec{std::move(kv->second)};
                deadlines_->erase(key.second);
                channels_.erase(kv);
                rec.onReply(std::move(msg), executor_);
            }
        }

        return true;
    }

    // Returns true if request was found
    ErrorOrDone cancelCall(RequestId requestId, CallCancelMode mode,
                           WampErrc errc = WampErrc::cancelled)
    {
        // If the cancel mode is not 'kill', don't wait for the router's
        // ERROR message and post the request handler immediately
        // with a WampErrc::cancelled error code.

        auto unex = makeUnexpectedError(errc);

        {
            RequestKey key{MessageKind::call, requestId};
            auto kv = requests_.find(key);
            if (kv != requests_.end())
            {
                deadlines_->erase(requestId);
                if (mode != CallCancelMode::kill)
                {
                    auto handler = std::move(kv->second);
                    requests_.erase(kv);
                    completeRequest(handler, unex);
                }
                return peer_->send(CallCancellation{requestId, mode});
            }
        }

        {
            auto kv = channels_.find(requestId);
            if (kv == channels_.end())
                return false;

            deadlines_->erase(requestId);
            if (mode != CallCancelMode::kill)
            {
                StreamRecord req{std::move(kv->second)};
                channels_.erase(kv);
                req.cancel(executor_, errc);
            }
            return peer_->send(CallCancellation{requestId, mode});
        }

        return false;
    }

    ErrorOrDone sendCallerChunk(CallerOutputChunk&& chunk)
    {
        auto key = chunk.requestKey({});
        if (requests_.count(key) == 0 && channels_.count(key.second) == 0)
            return false;
        return peer_->send(std::move(chunk));
    }

    void abandonAll(std::error_code ec)
    {
        UnexpectedError unex{ec};
        for (auto& kv: requests_)
            completeRequest(kv.second, unex);
        for (auto& kv: channels_)
            kv.second.abandon(unex, executor_);
        clear();
    }

    void clear()
    {
        deadlines_->clear();
        requests_.clear();
        channels_.clear();
        nextRequestId_ = nullId();
    }

private:
    using RequestKey = typename Message::RequestKey;
    using CallerTimeoutScheduler = TimeoutScheduler<RequestId>;

    template <typename C>
    ErrorOr<RequestId> doRequest(TrueType, C& command, TimeoutDuration timeout,
                                 RequestHandler&& handler)
    {
        // Will take 285 years to overflow 2^53 at 1 million requests/sec
        assert(nextRequestId_ < 9007199254740992u);
        RequestId requestId = nextRequestId_ + 1;
        command.setRequestId({}, requestId);

        auto sent = peer_->send(std::move(command));
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        ++nextRequestId_;
        if (!handler)
            return requestId;

        auto emplaced = requests_.emplace(command.requestKey({}),
                                          std::move(handler));
        assert(emplaced.second);

        if (timeout.count() != 0)
            deadlines_->insert(requestId, timeout);

        return requestId;
    }

    template <typename C>
    ErrorOr<RequestId> doRequest(FalseType, C& command, TimeoutDuration timeout,
                                 RequestHandler&& handler)
    {
        RequestId requestId = nullId();

        auto sent = peer_->send(std::move(command));
        if (!sent)
        {
            auto unex = makeUnexpected(sent.error());
            completeRequest(handler, unex);
            return unex;
        }

        if (!handler)
            return requestId;

        auto emplaced = requests_.emplace(command.requestKey({}),
                                          std::move(handler));
        assert(emplaced.second);

        if (timeout.count() != 0)
            deadlines_->insert(requestId, timeout);

        return requestId;
    }

    template <typename F, typename... Ts>
    void completeRequest(F& handler, Ts&&... args)
    {
        if (!handler)
            return;
        boost::asio::post(
            strand_,
            // NOLINTNEXTLINE(modernize-avoid-bind)
            std::bind(std::move(handler), std::forward<Ts>(args)...));
    }

    std::map<RequestKey, RequestHandler> requests_;
    std::map<ChannelId, StreamRecord> channels_;
    CallerTimeoutScheduler::Ptr deadlines_;
    IoStrand strand_;
    AnyIoExecutor executor_;
    AnyCompletionExecutor fallbackExecutor_;
    Peer* peer_ = nullptr;
    RequestId nextRequestId_ = nullId();
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REQUESTOR_HPP
