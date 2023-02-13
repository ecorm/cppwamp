/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../router.hpp"
#include "../api.hpp"
#include "../localsession.hpp"
#include "routerimpl.hpp"

namespace wamp
{

//******************************************************************************
// Router
//******************************************************************************

CPPWAMP_INLINE Abort Router::closeRealmReason()
{
    return {"wamp.close.close_realm"};
}

CPPWAMP_INLINE Abort Router::shutdownReason()
{
    return {"wamp.close.system_shutdown"};
}

CPPWAMP_INLINE Router::Router(Executor exec, RouterConfig config)
    : impl_(internal::RouterImpl::create(std::move(exec), std::move(config)))
{}

CPPWAMP_INLINE Router::~Router() {}

CPPWAMP_INLINE bool Router::openRealm(RealmConfig config)
{
    return impl_->addRealm(std::move(config));
}

CPPWAMP_INLINE bool Router::closeRealm(const std::string& name, Abort a)
{
    return impl_->closeRealm(name, std::move(a));
}

CPPWAMP_INLINE bool Router::openServer(ServerConfig config)
{
    return impl_->openServer(std::move(config));
}

CPPWAMP_INLINE void Router::closeServer(const std::string& name, Abort a)
{
    impl_->closeServer(name, std::move(a));
}

CPPWAMP_INLINE LocalSession Router::join(String realmUri, AuthInfo authInfo)
{
    auto s = impl_->localJoin(std::move(realmUri), std::move(authInfo),
                              strand());
    CPPWAMP_LOGIC_CHECK(bool(s), "No such realm '" + realmUri + "'");
    return LocalSession{std::move(s)};
}

CPPWAMP_INLINE LocalSession Router::join(String realmUri, AuthInfo authInfo,
                                         AnyCompletionExecutor fallbackExecutor)
{
    auto s = impl_->localJoin(std::move(realmUri), std::move(authInfo),
                              std::move(fallbackExecutor));
    CPPWAMP_LOGIC_CHECK(bool(s), "No such realm '" + realmUri + "'");
    return LocalSession{std::move(s)};
}

CPPWAMP_INLINE void Router::close(Abort a) {impl_->close(std::move(a));}

CPPWAMP_INLINE const Object& Router::roles()
{
    return internal::RouterContext::roles();
}

CPPWAMP_INLINE const IoStrand& Router::strand() const {return impl_->strand();}

} // namespace wamp
