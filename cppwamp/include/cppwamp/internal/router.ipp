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

CPPWAMP_INLINE Server::Ptr Router::addServer(ServerConfig config)
{
    return impl_->addServer(std::move(config));
}

CPPWAMP_INLINE void Router::removeServer(const Server::Ptr& server)
{
    impl_->removeServer(server->name());
}

CPPWAMP_INLINE void Router::removeServer(const std::string& name)
{
    impl_->removeServer(name);
}

CPPWAMP_INLINE const Object& Router::roles()
{
    return internal::RouterImpl::roles();
}

CPPWAMP_INLINE const IoStrand& Router::strand() const {return impl_->strand();}

CPPWAMP_INLINE Server::Ptr Router::server(const std::string& name) const
{
    return impl_->server(name);
}

CPPWAMP_INLINE LocalSession Router::join(const std::string& realmUri,
                                         std::string authId)
{
    auto localSessionImpl = impl_->join(realmUri, std::move(authId), strand());
    return LocalSession{std::move(localSessionImpl)};
}

CPPWAMP_INLINE LocalSession Router::join(const std::string& realmUri,
                                         std::string authId,
                                         AnyCompletionExecutor fallbackExecutor)
{
    auto localSessionImpl = impl_->join(realmUri, std::move(authId),
                                        std::move(fallbackExecutor));
    return LocalSession{std::move(localSessionImpl)};
}

CPPWAMP_INLINE void Router::startAll() {impl_->startAll();}

CPPWAMP_INLINE void Router::stopAll() {impl_->stopAll();}

} // namespace wamp
