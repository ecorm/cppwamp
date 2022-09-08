/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CHITS_HPP
#define CPPWAMP_CHITS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains lightweight tokens representing pending requests. */
//------------------------------------------------------------------------------

#include <memory>
#include "api.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class Caller; }

//------------------------------------------------------------------------------
/** Lightweight token representing a call request. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallChit
{
public:
    /** Constructs an empty subscription */
    CallChit();

    /** Returns false if the chit is empty. */
    explicit operator bool() const;

    /** Obtains the request ID associated with the call. */
    RequestId requestId() const;

    /** Obtains the default cancel mode associated with the call. */
    CallCancelMode cancelMode() const;

    /** Requests cancellation of the call using the cancel mode that
        was specified in the @ref wamp::Rpc "Rpc". */
    void cancel() const;

    /** Requests cancellation of the call using the given mode. */
    void cancel(CallCancelMode mode) const;

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;

    static constexpr RequestId invalidId_ = 0;
    CallerPtr caller_;
    RequestId reqId_ = invalidId_;
    CallCancelMode cancelMode_ = {};

public:
    // Internal use only
    CallChit(CallerPtr caller, RequestId reqId, CallCancelMode mode,
             internal::PassKey);

};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/chits.ipp"
#endif

#endif // CPPWAMP_CHITS_HPP
