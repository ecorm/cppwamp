/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authorizer.hpp"
#include <utility>
#include "../api.hpp"
#include "../traits.hpp"
#include "disclosuremode.hpp"
#include "routersession.hpp"

namespace wamp
{

//******************************************************************************
// Authorization
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization Authorization::granted(Disclosure d)
{
    return Authorization{Decision::granted, d, {}};
}

//------------------------------------------------------------------------------
/** If the given error code is not equivent to WampErrc::authorizationDenied,
    the returned ERROR message will contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization Authorization::denied(std::error_code ec)
{
    return Authorization{Decision::denied, Disclosure::preset, ec};
}

//------------------------------------------------------------------------------
/** @copydetails Authorization::denied(std::error_code) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization Authorization::denied(WampErrc errc)
{
    return denied(make_error_code(errc));
}

//------------------------------------------------------------------------------
/** If the given error code is not equivent to WampErrc::authorizationFailed,
    the returned ERROR message will contain two positional arguments:
    - A string formatted as `<ec.category().name()>:<ec.value()`
    - A string containing `ec.message()` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization Authorization::failed(std::error_code ec)
{
    return Authorization{Decision::failed, Disclosure::preset, ec};
}

//------------------------------------------------------------------------------
/** @copydetails Authorization::failed(std::error_code) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization Authorization::failed(WampErrc errc)
{
    return failed(make_error_code(errc));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Authorization::good() const {return !errorCode_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationDecision Authorization::decision() const
{
    return decision_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Disclosure Authorization::disclosure() const
{
    return disclosure_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::error_code Authorization::error() const {return errorCode_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authorization::Authorization(
    Decision decision, Disclosure disclosure, std::error_code ec)
    : errorCode_(ec),
      decision_(decision),
      disclosure_(disclosure)
{}


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

    switch (auth.decision())
    {
    case AuthorizationDecision::granted:
        grantAuthorization(std::forward<C>(command), auth, originator,
                           *listener);
        break;

    case AuthorizationDecision::denied:
        rejectAuthorization(std::forward<C>(command), auth,
                            WampErrc::authorizationDenied, *originator);
        break;

    case AuthorizationDecision::failed:
        rejectAuthorization(std::forward<C>(command), auth,
                            WampErrc::authorizationFailed, *originator);
        break;
    }

    originator_ = {};
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::grantAuthorization(
    C&& command, Authorization auth,
    const std::shared_ptr<Originator>& originator,
    internal::AuthorizationListener& listener)
{
    bool producerDisclosure = command.disclosed(internal::PassKey{});
    internal::DisclosureMode disclosureMode{auth.disclosure()};
    bool disclosed = disclosureMode.compute(producerDisclosure,
                                            consumerDisclosure_,
                                            realmDisclosure_);
    command.setDisclosed({}, disclosed);
    listener.onAuthorized(originator, std::forward<C>(command));
}

//------------------------------------------------------------------------------
template <typename C>
void AuthorizationRequest::rejectAuthorization(
    C&& command, Authorization auth, WampErrc errc, Originator& originator)
{
    auto authEc = auth.error();

    if (authEc == WampErrc::discloseMeDisallowed)
        errc = WampErrc::discloseMeDisallowed;

    auto error = Error::fromRequest({}, command, errc);
    if (authEc != errc)
        error.withArgs(briefErrorCodeString(authEc), authEc.message());
    originator.sendRouterCommand(std::move(error), true);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AuthorizationRequest::AuthorizationRequest(
    internal::PassKey, ListenerPtr listener,
    const std::shared_ptr<Originator>& originator,
    const std::shared_ptr<Authorizer>& authorizer,
    Disclosure realmDisclosure, bool consumerDisclosure)
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
        a.authorize(std::move(t), Authorization::granted());
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Pub p, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(p), std::move(a));
    else
        a.authorize(std::move(p), Authorization::granted());
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Procedure p, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(p), std::move(a));
    else
        a.authorize(std::move(p), Authorization::granted());
}

//------------------------------------------------------------------------------
// NOLINTNEXTLINE(misc-no-recursion)
CPPWAMP_INLINE void Authorizer::authorize(Rpc r, AuthorizationRequest a)
{
    if (chained_)
        chained_->authorize(std::move(r), std::move(a));
    else
        a.authorize(std::move(r), Authorization::granted());
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
