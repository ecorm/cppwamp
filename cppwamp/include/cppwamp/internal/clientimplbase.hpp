/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CLIENTIMPLBASE_HPP
#define CPPWAMP_INTERNAL_CLIENTIMPLBASE_HPP

#include <functional>
#include <memory>
#include <string>
#include "../args.hpp"
#include "../asyncresult.hpp"
#include "../error.hpp"
#include "../registration.hpp"
#include "../subscription.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "callee.hpp"
#include "subscriber.hpp"

namespace wamp
{

namespace internal
{

class RegistrationBase;
class SubscriptionBase;

//------------------------------------------------------------------------------
class ClientImplBase : public Callee, public Subscriber
{
public:
    using Ptr        = std::shared_ptr<ClientImplBase>;
    using WeakPtr    = std::weak_ptr<ClientImplBase>;
    using LogHandler = std::function<void (std::string)>;

    virtual ~ClientImplBase() {}

    virtual SessionState state() const = 0;

    virtual const std::string& realm() const = 0;

    virtual const Object& peerInfo() const = 0;

    virtual void join(std::string realm, AsyncHandler<SessionId> handler) = 0;

    virtual void leave(AsyncHandler<std::string>&& handler) = 0;

    virtual void leave(std::string&& reason,
                       AsyncHandler<std::string>&& handler) = 0;

    virtual void disconnect() = 0;

    virtual void terminate() = 0;

    virtual void subscribe(std::shared_ptr<SubscriptionBase> sub,
                           AsyncHandler<Subscription>&& handler) = 0;

    virtual void publish(std::string&& topic) = 0;

    virtual void publish(std::string&& topic, Args&& args) = 0;

    virtual void publish(std::string&& topic,
                         AsyncHandler<PublicationId>&& handler) = 0;

    virtual void publish(std::string&& topic, Args&& args,
                         AsyncHandler<PublicationId>&& handler) = 0;

    virtual void enroll(std::shared_ptr<RegistrationBase> reg,
                        AsyncHandler<Registration>&& handler) = 0;

    virtual void call(std::string&& procedure,
                      AsyncHandler<Args>&& handler) = 0;

    virtual void call(std::string&& procedure, Args&& args,
                      AsyncHandler<Args>&& handler) = 0;

    virtual void setLogHandlers(LogHandler warningHandler,
                                LogHandler traceHandler) = 0;

    virtual void postpone(std::function<void ()> functor) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CLIENTIMPLBASE_HPP
