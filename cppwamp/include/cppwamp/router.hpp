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
#include "logging.hpp"
#include "routerconfig.hpp"

namespace wamp
{

// Forward declarations
class LocalSession;
namespace internal { class RouterImpl; }


//------------------------------------------------------------------------------
/** %API for a _router peer in WAMP applications. */
//------------------------------------------------------------------------------
class CPPWAMP_API Router
{
public:
    // TODO: Thread-safe operations

    /** Executor type used for I/O operations. */
    using Executor = AnyIoExecutor;

    /** Type-erased wrapper around a log event handler. */
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    static Reason closeRealmReason();

    static Reason shutdownReason();

    /// @name Construction
    /// @{
    /** Constructor taking an executor. */
    explicit Router(Executor exec, RouterConfig config = {});

    /** Constructor taking an execution context. */
    template <typename E, EnableIf<isExecutionContext<E>()> = 0>
    explicit Router(E& context, RouterConfig config = {})
        : Router(context.get_executor(), std::move(config))
    {}

    /** Destructor. */
    ~Router();
    /// @}

    /// @name Move-only
    /// @{
    Router(const Router&) = delete;
    Router(Router&&) = default;
    Router& operator=(const Router&) = delete;
    Router& operator=(Router&&) = default;
    /// @}

    /// @name Operations
    /// @{
    bool openRealm(RealmConfig config);

    bool closeRealm(const std::string& name, bool terminate = false,
                    Reason r = closeRealmReason());

    bool openServer(ServerConfig config);

    void closeServer(const std::string& name, bool terminate = false,
                     Reason r = shutdownReason());

    LocalSession join(String realmUri, AuthInfo authInfo);

    LocalSession join(String realmUri, AuthInfo authInfo,
                      AnyCompletionExecutor fallbackExecutor);

    void close(bool terminate = false, Reason r = shutdownReason());
    /// @}

    /// @name Observers
    /// @{
    /** Obtains a dictionary of roles and features supported by the router. */
    static const Object& roles();

    /** Obtains the execution context in which I/O operations are serialized. */
    const IoStrand& strand() const;
    /// @}

private:
    std::shared_ptr<internal::RouterImpl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/router.ipp"
#endif

#endif // CPPWAMP_ROUTER_HPP
