/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "clientcontext.hpp"
#include "client.hpp"

namespace wamp
{

namespace internal
{

inline ClientContext::ClientContext() {}

inline ClientContext::ClientContext(const std::shared_ptr<Client>& client)
    : client_(std::move(client))
{}

inline bool ClientContext::expired() const {return client_.expired();}

inline void ClientContext::reset() {client_ = {};}

inline void ClientContext::safeUnsubscribe(const Subscription& s)
{
    auto c = client_.lock();
    if (c)
        c->safeUnsubscribe(s);
}

inline void ClientContext::onEventError(Error&& e, SubscriptionId s)
{
    auto c = client_.lock();
    if (c)
        c->onEventError(std::move(e), s);
}

inline void ClientContext::safeUnregister(const Registration& r)
{
    auto c = client_.lock();
    if (c)
        c->safeUnregister(r);
}

inline void ClientContext::safeYield(Result&& result, RequestId reqId,
                                     RegistrationId regId)
{
    auto c = client_.lock();
    if (c)
        c->safeYield(std::move(result), reqId, regId);
}

inline void ClientContext::safeYield(Error&& error, RequestId reqId,
                                     RegistrationId regId)
{
    auto c = client_.lock();
    if (c)
        c->safeYield(std::move(error), reqId, regId);
}

inline ErrorOrDone ClientContext::safeYield(CalleeOutputChunk&& chunk,
                                            RequestId reqId,
                                            RegistrationId regId)
{
    auto c = client_.lock();
    if (c)
        c->safeYield(std::move(chunk), reqId, regId);
}

inline void ClientContext::safeCancelCall(RequestId r, CallCancelMode m)
{
    auto c = client_.lock();
    if (c)
        c->safeCancelCall(r, m);
}

inline ErrorOrDone ClientContext::safeSendCallerChunk(CallerOutputChunk&& chunk)
{
    auto c = client_.lock();
    if (!c)
        return false;
    return c->safeSendCallerChunk(std::move(chunk));
}

inline void ClientContext::safeCancelStream(RequestId r)
{
    auto c = client_.lock();
    if (c)
        c->safeCancelStream(r);
}

inline void ClientContext::safeAuthenticate(Authentication&& a)
{
    auto c = client_.lock();
    if (c)
        c->safeAuthenticate(std::move(a));
}

inline void ClientContext::safeFailAuthentication(Reason&& r)
{
    auto c = client_.lock();
    if (c)
        c->safeFailAuthentication(std::move(r));
}

} // namespace internal

} // namespace wamp
