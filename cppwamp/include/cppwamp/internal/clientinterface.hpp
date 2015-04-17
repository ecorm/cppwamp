/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP
#define CPPWAMP_INTERNAL_CLIENTINTERFACE_HPP

#include <functional>
#include <memory>
#include <string>
#include "../asyncresult.hpp"
#include "../dialoguedata.hpp"
#include "../error.hpp"
#include "../registration.hpp"
#include "../sessiondata.hpp"
#include "../subscription.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "callee.hpp"
#include "subscriber.hpp"

namespace wamp
{

class Registration;
class Subscription;

namespace internal
{

//------------------------------------------------------------------------------
// Specifies the interface required for classes that implement wamp::Session.
//------------------------------------------------------------------------------
class ClientInterface : public Callee, public Subscriber
{
public:
    using Ptr        = std::shared_ptr<ClientInterface>;
    using WeakPtr    = std::weak_ptr<ClientInterface>;
    using LogHandler = std::function<void (std::string)>;

    static const Object& roles();

    virtual ~ClientInterface() {}

    virtual SessionState state() const = 0;

    virtual void join(Realm&& realm, AsyncHandler<SessionInfo> handler) = 0;

    virtual void leave(Reason&& reason, AsyncHandler<Reason>&& handler) = 0;

    virtual void disconnect() = 0;

    virtual void terminate() = 0;

    virtual void subscribe(std::shared_ptr<Subscription> sub,
            AsyncHandler<std::shared_ptr<Subscription>>&& handler) = 0;

    virtual void publish(Pub&& pub) = 0;

    virtual void publish(Pub&& pub, AsyncHandler<PublicationId>&& handler) = 0;

    virtual void enroll(std::shared_ptr<Registration> reg,
            AsyncHandler<std::shared_ptr<Registration>>&& handler) = 0;

    virtual void call(Rpc&& rpc, AsyncHandler<Result>&& handler) = 0;

    virtual void setLogHandlers(LogHandler warningHandler,
                                LogHandler traceHandler) = 0;

    virtual void postpone(std::function<void ()> functor) = 0;
};

inline const Object& ClientInterface::roles()
{
    static const Object roles =
    {
        {"callee", Object{{"features", Object{{
            {"call_trustlevels", true},
            {"caller_identification", true},
            {"pattern_based_registration", true},
            {"progressive_call_results", true}
        }}}}},
        {"caller", Object{{"features", Object{{
            {"call_timeout", true},
            {"callee_blackwhite_listing", true},
            {"caller_exclusion", true},
            {"caller_identification", true}
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
