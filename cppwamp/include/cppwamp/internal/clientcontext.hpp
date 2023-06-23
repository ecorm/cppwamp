/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
#define CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP

#include <memory>
#include <utility>
#include "../erroror.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

class Authentication;
class CalleeOutputChunk;
class CallerOutputChunk;
class Error;
class Reason;
class Registration;
class Result;

namespace internal
{

//------------------------------------------------------------------------------
struct SubscriptionTag {};

//------------------------------------------------------------------------------
struct RegistrationTag {};

//------------------------------------------------------------------------------
class ClientLike
{
public:
    using SlotId = uint64_t;
    using SubscriptionKey = std::pair<SubscriptionId, SlotId>;
    using RegistrationKey = SlotId;

    virtual ~ClientLike() = default;

    virtual void removeSlot(SubscriptionTag, SubscriptionKey key) = 0;

    virtual void removeSlot(RegistrationTag, RegistrationKey key) = 0;

    virtual void onEventError(Error&& e, SubscriptionId s) = 0;

    virtual void yieldResult(Result&& result, RequestId reqId,
                             RegistrationId regId) = 0;

    virtual void yieldError(Error&& error, RequestId reqId,
                            RegistrationId regId) = 0;

    virtual ErrorOrDone yieldChunk(CalleeOutputChunk&& chunk, RequestId reqId,
                                   RegistrationId regId) = 0;

    virtual void cancelCall(RequestId r, CallCancelMode m) = 0;

    virtual ErrorOrDone sendCallerChunk(CallerOutputChunk&& chunk) = 0;

    virtual void cancelStream(RequestId r) = 0;

    virtual void authenticate(Authentication&& a) = 0;

    virtual void failAuthentication(Reason&& r) = 0;
};

//------------------------------------------------------------------------------
class ClientContext
{
public:
    using SlotId = ClientLike::SlotId;
    using SubscriptionKey = std::pair<SubscriptionId, SlotId>;
    using RegistrationKey = SlotId;

    ClientContext() = default;

    ClientContext(const std::shared_ptr<ClientLike>& client)
        : client_(client)
    {}

    bool expired() const {return client_.expired();}

    void reset() {client_ = {};}

    template <typename TSlotTag, typename TKey>
    void removeSlot(TSlotTag, TKey key)
    {
        auto c = client_.lock();
        if (c)
            c->removeSlot(TSlotTag{}, key);
    }

    void onEventError(Error&& e, SubscriptionId s)
    {
        auto c = client_.lock();
        if (c)
            c->onEventError(std::move(e), s);
    }

    void yieldResult(Result&& result, RequestId reqId, RegistrationId regId)
    {
        auto c = client_.lock();
        if (c)
            c->yieldResult(std::move(result), reqId, regId);
    }

    void yieldError(Error&& error, RequestId reqId, RegistrationId regId)
    {
        auto c = client_.lock();
        if (c)
            c->yieldError(std::move(error), reqId, regId);
    }

    ErrorOrDone yieldChunk(CalleeOutputChunk&& chunk, RequestId reqId,
                           RegistrationId regId)
    {
        auto c = client_.lock();
        if (!c)
            return false;
        return c->yieldChunk(std::move(chunk), reqId, regId);
    }

    void cancelCall(RequestId r, CallCancelMode m)
    {
        auto c = client_.lock();
        if (c)
            c->cancelCall(r, m);
    }

    ErrorOrDone sendCallerChunk(CallerOutputChunk&& chunk)
    {
        auto c = client_.lock();
        if (!c)
            return false;
        return c->sendCallerChunk(std::move(chunk));
    }

    void cancelStream(RequestId r)
    {
        auto c = client_.lock();
        if (c)
            c->cancelStream(r);
    }

    void authenticate(Authentication&& a)
    {
        auto c = client_.lock();
        if (c)
            c->authenticate(std::move(a));
    }

    void failAuthentication(Reason&& r)
    {
        auto c = client_.lock();
        if (c)
            c->failAuthentication(std::move(r));
    }

    bool canRemoveSlot(const ClientLike& client) const
    {
        auto c = client_.lock();
        return c ? (c.get() == &client) : true;
    }

private:
    std::weak_ptr<ClientLike> client_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
