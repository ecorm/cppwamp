/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
#define CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP

#include <memory>
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
class Subscription;

namespace internal
{

//------------------------------------------------------------------------------
class ClientLike
{
public:
    virtual void unsubscribe(const Subscription& s) = 0;

    virtual void onEventError(Error&& e, SubscriptionId s) = 0;

    virtual void unregister(const Registration& r) = 0;

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
    ClientContext() {}

    ClientContext(const std::shared_ptr<ClientLike>& client)
        : client_(std::move(client))
    {}

    bool expired() const {return client_.expired();}

    void reset() {client_ = {};}

    void unsubscribe(const Subscription& s)
    {
        auto c = client_.lock();
        if (c)
            c->unsubscribe(s);
    }

    void onEventError(Error&& e, SubscriptionId s)
    {
        auto c = client_.lock();
        if (c)
            c->onEventError(std::move(e), s);
    }

    void unregister(const Registration& r)
    {
        auto c = client_.lock();
        if (c)
            c->unregister(r);
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

private:
    std::weak_ptr<ClientLike> client_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
