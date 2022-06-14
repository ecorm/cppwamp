/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP
#define CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP

#include <functional>
#include <memory>
#include <string>
#include "../peerdata.hpp"
#include "../error.hpp"
#include "../peerdata.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "asynctask.hpp"
#include "callee.hpp"
#include "challengee.hpp"
#include "subscriber.hpp"

namespace wamp
{

namespace internal
{

class RegistrationImpl;
class SubscriptionImpl;

//------------------------------------------------------------------------------
// Specifies the interface required for classes that implement wamp::Session.
//------------------------------------------------------------------------------
class ClientInterface :
    public Callee, public Subscriber, public Challengee
{
public:
    using Ptr           = std::shared_ptr<ClientInterface>;
    using WeakPtr       = std::weak_ptr<ClientInterface>;
    using EventSlot     = std::function<void (Event)>;
    using CallSlot      = std::function<Outcome (Invocation)>;
    using InterruptSlot = std::function<Outcome (Interruption)>;

    static const Object& roles();

    virtual ~ClientInterface() {}

    virtual SessionState state() const = 0;

    virtual void join(Realm&& realm, AsyncTask<SessionInfo>&& handler) = 0;

    virtual void leave(Reason&& reason, AsyncTask<Reason>&& handler) = 0;

    virtual void disconnect() = 0;

    virtual void terminate() = 0;

    virtual void subscribe(Topic&& topic, EventSlot&& slot,
                           AsyncTask<Subscription>&& handler) = 0;

    virtual void publish(Pub&& pub) = 0;

    virtual void publish(Pub&& pub, AsyncTask<PublicationId>&& handler) = 0;

    virtual void enroll(Procedure&& procedure, CallSlot&& callSlot,
                        InterruptSlot&& interruptSlot,
                        AsyncTask<Registration>&& handler) = 0;

    virtual RequestId call(Rpc&& rpc, AsyncTask<Result>&& handler) = 0;

    virtual void cancel(Cancellation&& cancellation) = 0;

    virtual void setSessionHandlers(AsyncTask<std::string> warningHandler,
                                    AsyncTask<std::string> traceHandler,
                                    AsyncTask<SessionState> stateChangeHandler,
                                    AsyncTask<Challenge> challengeHandler) = 0;
};

inline const Object& ClientInterface::roles()
{
    static const Object roles =
    {
        {"callee", Object{{"features", Object{{
            {"call_canceling", true},
            {"call_timeout", true},
            {"call_trustlevels", true},
            {"caller_identification", true},
            {"pattern_based_registration", true},
            {"progressive_call_results", true}
        }}}}},
        {"caller", Object{{"features", Object{{
            {"call_canceling", true},
            {"call_timeout", true},
            {"caller_exclusion", true},
            {"caller_identification", true},
            {"progressive_call_results", true}
        }}}}},
        {"publisher", Object{{"features", Object{{
            {"publisher_exclusion", true},
            {"publisher_identification", true},
            {"subscriber_blackwhite_listing", true}
        }}}}},
        {"subscriber", Object{{"features", Object{{
            {"pattern_based_subscription", true},
            {"publication_trustlevels", true},
            {"publisher_identification", true},
        }}}}}
    };

    return roles;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP
