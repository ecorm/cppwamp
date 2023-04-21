/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../router.hpp"
#include "../api.hpp"
#include "../errorcodes.hpp"
#include "routerimpl.hpp"

namespace wamp
{

//******************************************************************************
// Router
//******************************************************************************

CPPWAMP_INLINE Reason Router::closeRealmReason()
{
    return {"wamp.close.close_realm"};
}

CPPWAMP_INLINE Reason Router::shutdownReason()
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

CPPWAMP_INLINE bool Router::closeRealm(const std::string& name, Reason r)
{
    return impl_->closeRealm(name, std::move(r));
}

CPPWAMP_INLINE bool Router::openServer(ServerConfig config)
{
    return impl_->openServer(std::move(config));
}

CPPWAMP_INLINE void Router::closeServer(const std::string& name, Reason r)
{
    impl_->closeServer(name, std::move(r));
}

CPPWAMP_INLINE void Router::close(Reason r) {impl_->close(std::move(r));}

CPPWAMP_INLINE const IoStrand& Router::strand() const {return impl_->strand();}

CPPWAMP_INLINE std::shared_ptr<internal::RouterImpl>
Router::impl(internal::PassKey)
{
    return impl_;
}

} // namespace wamp
