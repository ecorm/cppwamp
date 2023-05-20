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

    void unsubscribe(const Subscription& s);

    void onEventError(Error&& e, SubscriptionId s);

    void unregister(const Registration& r);

    void yieldResult(Result&& result, RequestId reqId, RegistrationId regId);

    void yieldError(Error&& error, RequestId reqId, RegistrationId regId);

    ErrorOrDone yieldChunk(CalleeOutputChunk&& chunk, RequestId reqId,
                           RegistrationId regId);

    void cancelCall(RequestId r, CallCancelMode m);

    ErrorOrDone sendCallerChunk(CallerOutputChunk&& chunk);

    void cancelStream(RequestId r);

    void authenticate(Authentication&& a);

    void failAuthentication(Reason&& r);

private:
    std::weak_ptr<Client> client_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTCONTEXT_HPP
