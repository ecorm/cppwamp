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

CPPWAMP_INLINE Router::Router(Executor exec, RouterConfig config)
    : impl_(internal::RouterImpl::create(std::move(exec), std::move(config)))
{}

CPPWAMP_INLINE Router::~Router() {}

CPPWAMP_INLINE bool Router::addRealm(RealmConfig config)
{
    return impl_->addRealm(std::move(config));
}

CPPWAMP_INLINE bool Router::shutDownRealm(const std::string& name)
{
    return impl_->shutDownRealm(name);
}

CPPWAMP_INLINE bool Router::terminateRealm(const std::string& name)
{
    return impl_->terminateRealm(name);
}

CPPWAMP_INLINE bool Router::startServer(ServerConfig config)
{
    return impl_->startServer(std::move(config));
}

CPPWAMP_INLINE void Router::shutDownServer(const std::string& name)
{
    impl_->shutDownServer(name);
}

CPPWAMP_INLINE void Router::terminateServer(const std::string& name)
{
    impl_->terminateServer(name);
}

CPPWAMP_INLINE LocalSession Router::join(String realmUri, AuthInfo authInfo)
{
    auto s = impl_->joinLocal(std::move(realmUri), std::move(authInfo),
                              strand());
    CPPWAMP_LOGIC_CHECK(bool(s), "No such realm '" + realmUri + "'");
    return LocalSession{std::move(s)};
}

CPPWAMP_INLINE LocalSession Router::join(String realmUri, AuthInfo authInfo,
                                         AnyCompletionExecutor fallbackExecutor)
{
    auto s = impl_->joinLocal(std::move(realmUri), std::move(authInfo),
                              std::move(fallbackExecutor));
    CPPWAMP_LOGIC_CHECK(bool(s), "No such realm '" + realmUri + "'");
    return LocalSession{std::move(s)};
}

CPPWAMP_INLINE void Router::shutDown() {impl_->shutDown();}

CPPWAMP_INLINE void Router::terminate() {impl_->terminate();}

CPPWAMP_INLINE const Object& Router::roles()
{
    return internal::RouterContext::roles();
}

CPPWAMP_INLINE const IoStrand& Router::strand() const {return impl_->strand();}

} // namespace wamp
