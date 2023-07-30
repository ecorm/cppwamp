/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../router.hpp"
#include <utility>
#include "../api.hpp"
#include "../errorcodes.hpp"
#include "routerimpl.hpp"

namespace wamp
{

//******************************************************************************
// Router
//******************************************************************************

CPPWAMP_INLINE const Reason& Router::shutdownReason()
{
    static const Reason reason{WampErrc::systemShutdown};
    return reason;
}

CPPWAMP_INLINE Router::Router(Executor exec, RouterOptions options)
    : impl_(internal::RouterImpl::create(std::move(exec), std::move(options)))
{}

CPPWAMP_INLINE ErrorOr<Realm> Router::openRealm(RealmOptions options)
{
    auto impl = impl_->addRealm(std::move(options));
    if (!impl)
        return makeUnexpectedError(MiscErrc::alreadyExists);
    return Realm{std::move(impl), impl_->executor()};
}

CPPWAMP_INLINE ErrorOr<Realm> Router::openRealm(RealmOptions options,
                                                FallbackExecutor fe)
{
    auto impl = impl_->addRealm(std::move(options));
    if (!impl)
        return makeUnexpectedError(MiscErrc::alreadyExists);
    return Realm{std::move(impl), std::move(fe)};
}

CPPWAMP_INLINE ErrorOr<Realm> Router::realm(const Uri& uri) const
{
    auto realmImpl = impl_->realmAt(uri);
    if (!realmImpl)
        return makeUnexpectedError(WampErrc::noSuchRealm);
    return Realm{std::move(realmImpl), impl_->executor()};
}

CPPWAMP_INLINE ErrorOr<Realm> Router::realm(const Uri& uri,
                                            FallbackExecutor fe) const
{
    auto realmImpl = impl_->realmAt(uri);
    if (!realmImpl)
        return makeUnexpectedError(WampErrc::noSuchRealm);
    return Realm{std::move(realmImpl), std::move(fe)};
}

CPPWAMP_INLINE bool Router::openServer(ServerOptions config)
{
    return impl_->openServer(std::move(config));
}

CPPWAMP_INLINE void Router::closeServer(const std::string& name, Reason r)
{
    impl_->closeServer(name, std::move(r));
}

CPPWAMP_INLINE void Router::close(Reason r) {impl_->close(std::move(r));}

CPPWAMP_INLINE LogLevel Router::logLevel() const {return impl_->logLevel();}

CPPWAMP_INLINE void Router::setLogLevel(LogLevel level)
{
    impl_->setLogLevel(level);
}

CPPWAMP_INLINE void Router::log(LogEntry entry) {impl_->log(std::move(entry));}

CPPWAMP_INLINE const Router::Executor& Router::executor()
{
    return impl_->executor();
}

CPPWAMP_INLINE std::shared_ptr<internal::RouterImpl>
Router::impl(internal::PassKey)
{
    return impl_;
}

//******************************************************************************
// DirectRouterLink
//******************************************************************************

CPPWAMP_INLINE DirectRouterLink::DirectRouterLink(Router& router)
    : authInfo_({}, {}, "x_cppwamp_direct", "direct"),
      router_(router.impl({}))
{
}

CPPWAMP_INLINE DirectRouterLink&
DirectRouterLink::withAuthInfo(AuthInfo info)
{
    authInfo_ = std::move(info);
    return *this;
}

CPPWAMP_INLINE DirectRouterLink&
DirectRouterLink::withEndpointLabel(std::string endpointLabel)
{
    endpointLabel_ = std::move(endpointLabel);
    return *this;
}

CPPWAMP_INLINE DirectRouterLink::RouterImplPtr
DirectRouterLink::router(internal::PassKey)
{
    return router_;
}

CPPWAMP_INLINE AuthInfo& DirectRouterLink::authInfo(internal::PassKey)
{
    return authInfo_;
}

CPPWAMP_INLINE std::string& DirectRouterLink::endpointLabel(internal::PassKey)
{
    return endpointLabel_;
}

} // namespace wamp
