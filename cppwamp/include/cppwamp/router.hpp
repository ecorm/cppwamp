/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTER_HPP
#define CPPWAMP_ROUTER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "erroror.hpp"
#include "logging.hpp"
#include "realm.hpp"
#include "routerconfig.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

// Forward declarations
namespace internal { class RouterImpl; }

//------------------------------------------------------------------------------
/** %API for a _router peer in WAMP applications. */
//------------------------------------------------------------------------------
class CPPWAMP_API Router
{
public:
    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Fallback executor type for user-provided handlers passed via
        the Realm interface. */
    using FallbackExecutor = AnyCompletionExecutor;

    /** Type-erased wrapper around a log event handler. */
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    static const Reason& shutdownReason();

    /** Constructor taking an executor. */
    explicit Router(Executor exec, RouterConfig config = {});

    /** Constructor taking an execution context. */
    template <typename E, CPPWAMP_NEEDS(isExecutionContext<E>()) = 0>
    explicit Router(E& context, RouterConfig config = {})
        : Router(context.get_executor(), std::move(config))
    {}

    /// @name Move-only
    /// @{
    Router(const Router&) = delete;
    Router(Router&&) = default;
    Router& operator=(const Router&) = delete;
    Router& operator=(Router&&) = default;
    /// @}

    ErrorOr<Realm> openRealm(RealmConfig config);

    ErrorOr<Realm> openRealm(RealmConfig config, FallbackExecutor fe);

    bool closeRealm(const Uri uri, Reason r = shutdownReason());

    ErrorOr<Realm> realmAt(const Uri& uri) const;

    ErrorOr<Realm> realmAt(const Uri& uri, FallbackExecutor fe) const;

    bool openServer(ServerConfig config);

    void closeServer(const std::string& name, Reason r = shutdownReason());

    void close(Reason r = shutdownReason());

    LogLevel logLevel() const;

    void setLogLevel(LogLevel level);

    const Executor& executor();

private:
    std::shared_ptr<internal::RouterImpl> impl_;

public: // Internal use only
    std::shared_ptr<internal::RouterImpl> impl(internal::PassKey);
};

//------------------------------------------------------------------------------
class DirectRouterLink
{
public:
    DirectRouterLink(Router& router);
    
    DirectRouterLink& withAuthInfo(AuthInfo info);

    DirectRouterLink& withEndpointLabel(std::string endpointLabel);

private:
    using RouterImplPtr = std::shared_ptr<internal::RouterImpl>;
    
    AuthInfo authInfo_;
    std::string endpointLabel_;
    RouterImplPtr router_;

public: // Internal use only
    RouterImplPtr router(internal::PassKey);
    AuthInfo& authInfo(internal::PassKey);
    std::string& endpointLabel(internal::PassKey);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/router.inl.hpp"
#endif

#endif // CPPWAMP_ROUTER_HPP
