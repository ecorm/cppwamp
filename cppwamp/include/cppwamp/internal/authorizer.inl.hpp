/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include <utility>
#include "../api.hpp"
#include "../traits.hpp"
#include "disclosuresetter.hpp"
#include "routersession.hpp"

namespace wamp
{

//******************************************************************************
// Authorization
//******************************************************************************

//------------------------------------------------------------------------------
/** @note Implicit conversions from bool are not enabled to avoid accidental
          conversions from other types convertible to bool.
    @see Authorization::Authorization(AuthorizationGranted)
    @see Authorization::Authorization(AuthorizationDenied) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(bool allowed) : allowed_(allowed) {}

CPPWAMP_INLINE Authorization::Authorization(AuthorizationGranted)
    : Authorization(true)
{}

CPPWAMP_INLINE Authorization::Authorization(AuthorizationDenied)
    : Authorization(false)
{}

//------------------------------------------------------------------------------
/** If WampErrc::authorizationDenied, WampErrc::authorizationFailed, or
    WampErrc::discloseMeDisallowed is passed, their corresponding URI shall be
    used in the ERROR message returned to the client. Otherwise, the error
    URI shall be `wamp.error.authorization_failed` and the ERROR message will
    contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(std::error_code ec) : errorCode_(ec) {}

//------------------------------------------------------------------------------
/** @copydetails Authorization(std::error_code) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(WampErrc errc)
    : Authorization(make_error_code(errc))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization& Authorization::withDisclosure(DisclosureRule d)
{
    disclosure_ = d;
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::good() const {return !errorCode_ && allowed_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code Authorization::error() const {return errorCode_;}

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
CPPWAMP_INLINE void AuthorizationRequest::authorize(
    Topic t, Authorization a, bool cache)
{
    doAuthorize(std::move(t), a, cache);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(
    Pub p, Authorization a, bool cache)
{
    doAuthorize(std::move(p), a, cache);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(
    Procedure p, Authorization a, bool cache)
{
    doAuthorize(std::move(p), a, cache);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AuthorizationRequest::authorize(
    Rpc r, Authorization a, bool cache)
{
    doAuthorize(std::move(r), a, cache);
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::doAuthorize(C&& command, Authorization auth,
                                       bool cache)
{
    auto originator = originator_.lock();
    if (!originator)
        return;
    auto listener = listener_.lock();
    if (!listener)
        return;

    if (cache)
    {
        auto authorizer = authorizer_.lock();
        if (authorizer)
            authorizer->cache(command, originator->sharedInfo(), auth);
    }

    if (auth.good())
    {
        if (internal::DisclosureSetter::applyToCommand(
                command, *originator, realmDisclosure_, auth.disclosure()))
        {
            listener->onAuthorized(originator, std::forward<C>(command));
        }
        return;
    }

    auto ec = make_error_code(WampErrc::authorizationDenied);
    auto authEc = auth.error();
    bool isKnownAuthError = true;

    if (authEc)
    {
        isKnownAuthError =
            authEc == WampErrc::authorizationDenied ||
            authEc == WampErrc::authorizationFailed ||
            authEc == WampErrc::authorizationRequired ||
            authEc == WampErrc::discloseMeDisallowed;

        ec = isKnownAuthError ?
                 authEc :
                 make_error_code(WampErrc::authorizationFailed);
    }

    auto error = Error::fromRequest({}, command, ec);
    if (!isKnownAuthError)
        error.withArgs(briefErrorCodeString(authEc), authEc.message());

    originator->sendRouterCommand(std::move(error), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, ListenerPtr listener,
    const std::shared_ptr<internal::RouterSession>& originator,
    const std::shared_ptr<Authorizer>& authorizer,
    DisclosureRule realmDisclosure)
    : listener_(std::move(listener)),
      originator_(originator),
      authorizer_(authorizer),
      info_(originator->sharedInfo()),
      realmDisclosure_(realmDisclosure)
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
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(t), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(p), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(p), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a,
                                          AnyIoExecutor& e)
{
    doAuthorize(std::move(r), std::move(a), e);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Topic& t, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Pub& p, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Procedure& p, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::cache(const Rpc& r, const SessionInfo& s,
                                      Authorization a)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheSession(const SessionInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheProcedure(const RegistrationInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::uncacheTopic(const SubscriptionInfo&) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorizer::Authorizer() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Topic t, AuthorizationRequest a)
{
    a.authorize(std::move(t), granted);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Pub p, AuthorizationRequest a)
{
    a.authorize(std::move(p), granted);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Procedure p, AuthorizationRequest a)
{
    a.authorize(std::move(p), granted);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::onAuthorize(Rpc r, AuthorizationRequest a)
{
    a.authorize(std::move(r), granted);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnyCompletionExecutor& Authorizer::executor() {return executor_;}

//------------------------------------------------------------------------------
template <typename C>
void Authorizer::doAuthorize(C&& command, AuthorizationRequest&& a,
                             AnyIoExecutor& ioExec)
{
    using Command = ValueTypeOf<C>;

    if (executor_ == nullptr)
    {
        onAuthorize(std::forward<C>(command), std::move(a));
        return;
    }

    struct Posted
    {
        Ptr self;
        Command c;
        AuthorizationRequest a;
        void operator()() {self->onAuthorize(std::move(c), std::move(a));}
    };

    boost::asio::post(
        ioExec,
        boost::asio::bind_executor(
            executor_,
            Posted{shared_from_this(), std::forward<C>(command),
                   std::move(a)}));
}

} // namespace wamp
