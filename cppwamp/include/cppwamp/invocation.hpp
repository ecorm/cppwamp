/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INVOCATION_HPP
#define CPPWAMP_INVOCATION_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Invocation class. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include <string>
#include "args.hpp"
#include "wampdefs.hpp"
#include "internal/callee.hpp"

namespace wamp
{

// Forward declaration
namespace internal
{
    template <typename, typename> class ClientImpl;
}

//------------------------------------------------------------------------------
/** Provides the means for returning a `YIELD` or `ERROR` result back to
    the RPC caller. */
//------------------------------------------------------------------------------
class Invocation
{
public:
    /** Returns the request ID associated with this RPC invocation. */
    RequestId requestId() const;

    /** Determines if the callee (client) object that dispatched this
        invocation still exists or has expired. */
    bool calleeHasExpired() const;

    /** Sends a `YIELD` result back to the callee. */
    void yield();

    /** Sends a `YIELD` result, with an _Arguments_ payload, back to the
        callee. */
    void yield(Args result);

    /** Sends an `ERROR` result, with optional _Details_ and _Arguments_
        payloads, back to the callee. */
    void fail(std::string reason, Object details = Object(),
              Args args = Args());

    /** Sends an `ERROR` result, with an _Arguments_ payload, back to the
        callee. */
    void fail(std::string reason, Args args);

private:
    using CalleePtr = internal::Callee::WeakPtr;

    Invocation(CalleePtr callee, RequestId id);

    CalleePtr callee_;
    RequestId id_;
    bool hasReturned_;

    template <typename, typename> friend class internal::ClientImpl;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/invocation.ipp"
#endif

#endif // CPPWAMP_INVOCATION_HPP
