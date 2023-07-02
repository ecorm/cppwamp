/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include "../api.hpp"
#include "routercontext.hpp"
#include "routersession.hpp"

namespace wamp
{

//******************************************************************************
// Authorization
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(bool allowed) : allowed_(allowed) {}

/** If WampErrc::authorizationDenied, WampErrc::authorizationFailed, or
    WampErrc::discloseMeDisallowed is passed, their corresponding URI shall be
    used in the ERROR message returned to the client. Otherwise, the error
    URI shall be `wamp.error.authorization_failed` and the ERROR message will
    contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
CPPWAMP_INLINE Authorization::Authorization(std::error_code ec) : error_(ec) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization& Authorization::withDisclosure(DisclosureRule d)
{
    disclosure_ = d;
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::operator bool() const
{
    return !error_ && allowed_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code Authorization::error() const {return error_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::allowed() const {return allowed_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE DisclosureRule Authorization::disclosure() const
{
    return disclosure_;
}


//******************************************************************************
// AuthorizationRequest
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE const SessionInfo& AuthorizationRequest::info() const
{
    return info_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Topic t, Authorization a)
{
    send(std::move(t), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Pub p, Authorization a)
{
    send(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Procedure p,
                                                    Authorization a)
{
    send(std::move(p), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(Rpc r, Authorization a)
{
    send(std::move(r), a);
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::send(C&& command, Authorization a)
{
    auto originator = originator_.lock();
    if (!originator)
        return;
    realm_.processAuthorization(std::move(originator),
                                std::forward<C>(command), a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, internal::RealmContext r,
    const std::shared_ptr<internal::RouterSession>& s)
    : realm_(std::move(r)),
      originator_(s),
      info_(s->sharedInfo())
{}


//******************************************************************************
// Authorizer
//******************************************************************************

//------------------------------------------------------------------------------
/** @details
    This method makes it so that the `onAuthorize` handler will be posted
    via the given executor. If no executor is set, the `onAuthorize` handler
    is invoked directly from the realm's execution strand. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::bindExecutor(AnyCompletionExecutor e)
{
    executor_ = std::move(e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Topic t, AuthorizationRequest a,
                                          AnyIoExecutor& ioExec)
{
    if (executor_ == nullptr)
    {
        onAuthorize(std::move(t), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Topic t;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(t), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::move(t), std::move(a)}));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a,
                                          AnyIoExecutor& ioExec)
{
    if (executor_ == nullptr)
    {
        onAuthorize(std::move(p), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Pub p;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(p), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::move(p), std::move(a)}));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a,
                                          AnyIoExecutor& ioExec)
{
    if (executor_ == nullptr)
    {
        onAuthorize(std::move(p), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Procedure p;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(p), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::move(p), std::move(a)}));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a,
                                          AnyIoExecutor& ioExec)
{
    if (executor_ == nullptr)
    {
        onAuthorize(std::move(r), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Rpc r;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(r), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::move(r), std::move(a)}));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Topic t, AuthorizationRequest a)
{
    a.authorize(std::move(t), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Pub p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Procedure p, AuthorizationRequest a)
{
    a.authorize(std::move(p), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Rpc r, AuthorizationRequest a)
{
    a.authorize(std::move(r), true);
}

} // namespace wamp