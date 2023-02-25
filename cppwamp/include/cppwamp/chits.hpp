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

#include <future>
#include <memory>
#include "api.hpp"
#include "peerdata.hpp"
#include "tagtypes.hpp"
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

    /** Determines if the call is progressive. */
    bool isProgressive() const;

    /** Determines if a chunk marked as final was sent. */
    bool finalChunkSent() const;

    /** Requests cancellation of the call using the cancel mode that
        was specified in the @ref wamp::Rpc "Rpc". */
    void cancel() const;

    /** Thread-safe cancel. */
    void cancel(ThreadSafe) const;

    /** Requests cancellation of the call using the given mode. */
    void cancel(CallCancelMode mode) const;

    /** Thread-safe cancel with mode. */
    void cancel(ThreadSafe, CallCancelMode mode) const;

    /** Sends the given chunk via a progressive call. */
    ErrorOrDone send(OutputChunk chunk) const;

    /** Thread-safe send. */
    std::future<ErrorOrDone> send(ThreadSafe, OutputChunk chunk) const;

private:
    using CallerPtr = std::weak_ptr<internal::Caller>;

    static constexpr RequestId invalidId_ = 0;
    CallerPtr caller_;
    RequestId reqId_ = invalidId_;
    CallCancelMode cancelMode_ = CallCancelMode::unknown;
    bool isProgressive_ = false;
    bool finalChunkSent_ = false;

public:
    // Internal use only
    CallChit(CallerPtr caller, RequestId reqId, CallCancelMode mode,
             bool progressive, internal::PassKey);

};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/chits.ipp"
#endif

#endif // CPPWAMP_CHITS_HPP
