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

class Client;

//------------------------------------------------------------------------------
class ClientContext
{
public:
    ClientContext();

    ClientContext(const std::shared_ptr<Client>& client);

    bool expired() const;

    void reset();

    void safeUnsubscribe(const Subscription& s);

    void onEventError(Error&& e, SubscriptionId s);

    void safeUnregister(const Registration& r);

    void safeYield(Result&& result, RequestId reqId, RegistrationId regId);

    void safeYield(Error&& error, RequestId reqId, RegistrationId regId);

    ErrorOrDone safeYield(CalleeOutputChunk&& chunk, RequestId reqId,
                          RegistrationId regId);

    void safeCancelCall(RequestId r, CallCancelMode m);

    ErrorOrDone safeSendCallerChunk(CallerOutputChunk&& chunk);

    void safeCancelStream(RequestId r);

    void safeAuthenticate(Authentication&& a);

    void safeFailAuthentication(Reason&& r);

private:
    std::weak_ptr<Client> client_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
