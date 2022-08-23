/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP
#define CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP

#include <memory>
#include <string>
#include "../anyhandler.hpp"
#include "../asiodefs.hpp"
#include "../chits.hpp"
#include "../connector.hpp"
#include "../error.hpp"
#include "../peerdata.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "callee.hpp"
#include "caller.hpp"
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
    public Callee, public Caller, public Subscriber, public Challengee
{
public:
    using Ptr                = std::shared_ptr<ClientInterface>;
    using WeakPtr            = std::weak_ptr<ClientInterface>;
    using EventSlot          = AnyReusableHandler<void (Event)>;
    using CallSlot           = AnyReusableHandler<Outcome (Invocation)>;
    using InterruptSlot      = AnyReusableHandler<Outcome (Interruption)>;
    using LogHandler         = AnyReusableHandler<void(std::string)>;
    using StateChangeHandler = AnyReusableHandler<void(SessionState)>;
    using ChallengeHandler   = AnyReusableHandler<void(Challenge)>;
    using OngoingCallHandler = AnyReusableHandler<void(ErrorOr<Result>)>;

    template <typename TValue>
    using CompletionHandler = AnyCompletionHandler<void(ErrorOr<TValue>)>;

    static const Object& roles();

    virtual ~ClientInterface() {}

    virtual const IoStrand& strand() const = 0;

    virtual const AnyIoExecutor& userExecutor() const = 0;

    virtual SessionState state() const = 0;

    virtual void setWarningHandler(LogHandler) = 0;

    virtual void safeSetWarningHandler(LogHandler) = 0;

    virtual void setTraceHandler(LogHandler) = 0;

    virtual void safeSetTraceHandler(LogHandler) = 0;

    virtual void setStateChangeHandler(StateChangeHandler) = 0;

    virtual void safeSetStateChangeHandler(StateChangeHandler) = 0;

    virtual void setChallengeHandler(ChallengeHandler) = 0;

    virtual void safeSetChallengeHandler(ChallengeHandler) = 0;

    virtual void connect(ConnectionWishList wishes,
                         CompletionHandler<size_t>&& handler) = 0;

    virtual void safeConnect(ConnectionWishList wishes,
                             CompletionHandler<size_t>&& handler) = 0;

    virtual void join(Realm&&, CompletionHandler<SessionInfo>&&) = 0;

    virtual void safeJoin(Realm&&, CompletionHandler<SessionInfo>&&) = 0;

    virtual void authenticate(Authentication&& auth) = 0;

    virtual void leave(Reason&&, CompletionHandler<Reason>&&) = 0;

    virtual void safeLeave(Reason&&, CompletionHandler<Reason>&&) = 0;

    virtual void disconnect() = 0;

    virtual void safeDisconnect() = 0;

    virtual void terminate() = 0;

    virtual void safeTerminate() = 0;

    virtual void subscribe(Topic&&, EventSlot&&,
                           CompletionHandler<Subscription>&&) = 0;

    virtual void safeSubscribe(Topic&&, EventSlot&&,
                               CompletionHandler<Subscription>&&) = 0;

    virtual void unsubscribe(const Subscription&) = 0;

    virtual void unsubscribe(const Subscription&,
                             CompletionHandler<bool>&&) = 0;

    virtual void publish(Pub&&) = 0;

    virtual void safePublish(Pub&&) = 0;

    virtual void publish(Pub&&, CompletionHandler<PublicationId>&&) = 0;

    virtual void safePublish(Pub&&, CompletionHandler<PublicationId>&&) = 0;

    virtual void enroll(Procedure&&, CallSlot&&, InterruptSlot&&,
                        CompletionHandler<Registration>&&) = 0;

    virtual void safeEnroll(Procedure&&, CallSlot&&, InterruptSlot&&,
                            CompletionHandler<Registration>&&) = 0;

    virtual void unregister(const Registration&) = 0;

    virtual void unregister(const Registration&, CompletionHandler<bool>&&) = 0;

    virtual void oneShotCall(Rpc&&, CallChit*, CompletionHandler<Result>&&) = 0;

    virtual void safeOneShotCall(Rpc&&, CallChit*,
                                 CompletionHandler<Result>&&) = 0;

    virtual void ongoingCall(Rpc&&, CallChit*, OngoingCallHandler&&) = 0;

    virtual void safeOngoingCall(Rpc&&, CallChit*, OngoingCallHandler&&) = 0;

    virtual void yield(RequestId, wamp::Result&&) = 0;

    virtual void yield(RequestId, wamp::Error&&) = 0;
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
