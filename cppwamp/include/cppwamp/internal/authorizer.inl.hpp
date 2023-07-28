/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include <utility>
#include "../api.hpp"
#include "../traits.hpp"
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
CPPWAMP_INLINE Authorization& Authorization::withDisclosure(DisclosurePolicy d)
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
CPPWAMP_INLINE DisclosurePolicy Authorization::disclosure() const
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

    std::error_code ec;

    if (auth.good())
    {
        auto disclosed = auth.disclosure().computeDisclosure(
            command.disclosed(internal::PassKey{}),
            consumerDisclosure_,
            realmDisclosure_);

        if (disclosed.has_value())
        {
            command.setDisclosed({}, *disclosed);
            listener->onAuthorized(originator, std::forward<C>(command));
            return;
        }

        ec = disclosed.error();
    }
    else
    {
        ec = make_error_code(WampErrc::authorizationDenied);;
    }

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
    DisclosurePolicy realmDisclosure, bool consumerDisclosure)
    : listener_(std::move(listener)),
      originator_(originator),
      authorizer_(authorizer),
      info_(originator->sharedInfo()),
      realmDisclosure_(realmDisclosure),
      consumerDisclosure_(consumerDisclosure)
{}


//******************************************************************************
// Authorizer
//******************************************************************************

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Topic t, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(t), std::move(a));
    else
        a.authorize(std::move(t), granted);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(p), std::move(a));
    else
        a.authorize(std::move(p), granted);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(p), std::move(a));
    else
        a.authorize(std::move(p), granted);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(r), std::move(a));
    else
        a.authorize(std::move(r), granted);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::cache(const Topic& t, const SessionInfo& s,
                                      Authorization a)
{
    if (chained_)
        chained_->cache(t, s, a);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::cache(const Pub& p, const SessionInfo& s,
                                      Authorization a)
{
    if (chained_)
        chained_->cache(p, s, a);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::cache(const Procedure& p, const SessionInfo& s,
                                      Authorization a)
{
    if (chained_)
        chained_->cache(p, s, a);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::cache(const Rpc& r, const SessionInfo& s,
                                      Authorization a)
{
    if (chained_)
        chained_->cache(r, s, a);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::uncacheSession(const SessionInfo& s)
{
    if (chained_)
        chained_->uncacheSession(s);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::uncacheProcedure(const RegistrationInfo& r)
{
    if (chained_)
        chained_->uncacheProcedure(r);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::uncacheTopic(const SubscriptionInfo& s)
{
    if (chained_)
        chained_->uncacheTopic(s);
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::setIoExecutor(const AnyIoExecutor& exec)
{
    if (chained_)
        chained_->setIoExecutor(exec);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorizer::Authorizer(Ptr chained)
    : chained_(std::move(chained))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Authorizer::bind(Ptr chained)
{
    chained_ = std::move(chained);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Authorizer::Ptr& Authorizer::chained() const
{
    return chained_;
}


//******************************************************************************
// PostingAuthorizer
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE PostingAuthorizer::Ptr
PostingAuthorizer::create(Authorizer::Ptr chained, Executor e)
{
    return Ptr{new PostingAuthorizer(std::move(chained), std::move(e))};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const PostingAuthorizer::Executor&
PostingAuthorizer::executor() const
{
    return executor_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void PostingAuthorizer::authorize(Topic t,
                                                 AuthorizationRequest a)
{
    postAuthorization(t, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void PostingAuthorizer::authorize(Pub p, AuthorizationRequest a)
{
    postAuthorization(p, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void PostingAuthorizer::authorize(Procedure p,
                                                 AuthorizationRequest a)
{
    postAuthorization(p, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void PostingAuthorizer::authorize(Rpc r, AuthorizationRequest a)
{
    postAuthorization(r, a);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE PostingAuthorizer::PostingAuthorizer(Authorizer::Ptr chained,
                                                    AnyCompletionExecutor e)
    : Base(std::move(chained)),
      executor_(std::move(e))
{}

//------------------------------------------------------------------------------
template <typename C>
void PostingAuthorizer::postAuthorization(C& command, AuthorizationRequest& req)
{
    using Command = ValueTypeOf<C>;

    struct Posted
    {
        Ptr self;
        Command c;
        AuthorizationRequest r;

        void operator()()
        {
            self->chained()->authorize(std::move(c), std::move(r));
        }
    };

    auto self =
        std::dynamic_pointer_cast<PostingAuthorizer>(shared_from_this());

    boost::asio::post(
        ioExecutor_,
        boost::asio::bind_executor(
            executor_,
            Posted{self, std::move(command), std::move(req)}));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void PostingAuthorizer::setIoExecutor(const AnyIoExecutor& exec)
{
    ioExecutor_ = exec;
    Base::chained()->setIoExecutor(exec);
}

} // namespace wamp
