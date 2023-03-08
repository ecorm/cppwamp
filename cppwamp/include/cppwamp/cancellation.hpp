/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CANCELLATION_HPP
#define CPPWAMP_CANCELLATION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for cancelling requests. */
//------------------------------------------------------------------------------

#include <memory>
#include "api.hpp"
#include "tagtypes.hpp"
#include "wampdefs.hpp"
#include "internal/caller.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Slot associated with a CallCancellationSignal.
    Emulates Boost.Asio's [Per-Operation Cancellation][1] mechanism. Use a
    CallCancellationSignal to generate cancellation slots that can be passed
    to Rpc::withCancellationSlot.
    [1]: https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/cancellation.html */
//------------------------------------------------------------------------------
class CPPWAMP_API CallCancellationSlot
{
public:
    /// Handler type that can be assigned to the slot.
    class Handler
    {
    public:
        Handler();
        Handler(internal::Caller::WeakPtr caller, RequestId requestId);
        explicit operator bool();
        void operator()(CallCancelMode cancelMode);
        void operator()(ThreadSafe, CallCancelMode cancelMode);

    private:
        internal::Caller::WeakPtr caller_;
        RequestId requestId_ = nullId();
    };

    /** Constructs a disconnected slot. */
    CallCancellationSlot();

    /** Assigns the given handler to the slot. */
    Handler& assign(Handler f);

    /** Constructs the handler in-place with the given arguments. */
    Handler& emplace(internal::Caller::WeakPtr caller, RequestId reqId);

    /** Clears the handler from the slot. */
    void clear();

    /** Determines if a handler is currently assigned. */
    bool has_handler() const;

    /** Determines if the slot is currently connected to a signal. */
    bool is_connected() const;

    /** Determines if the given slot has identical effects to this one. */
    bool operator==(const CallCancellationSlot& rhs) const;

    /** Determines if the given slot does not have identical effects to
        this one. */
    bool operator!=(const CallCancellationSlot& rhs) const;

private:
    struct Impl;

    CallCancellationSlot(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

    std::shared_ptr<Impl> impl_;

    friend class CallCancellationSignal;
};

//------------------------------------------------------------------------------
/** Lightweight token used to cancel remote procedure calls.
    Emulates Boost.Asio's [Per-Operation Cancellation][1] mechanism.
    [1]: https://www.boost.org/doc/libs/release/doc/html/boost_asio/overview/core/cancellation.html */
//------------------------------------------------------------------------------
class CPPWAMP_API CallCancellationSignal
{
public:
    /** Default constructor. */
    CallCancellationSignal();

    /** Executes the handler assigned to the connected slot. */
    void emit(CallCancelMode cancelMode);

    /** Thread-safe emit. */
    void emit(ThreadSafe, CallCancelMode cancelMode);

    /** Obtains the slot that is connected to this signal. */
    CallCancellationSlot slot() const;

private:
    std::shared_ptr<CallCancellationSlot::Impl> slotImpl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/cancellation.ipp"
#endif

#endif // CPPWAMP_CANCELLATION_HPP
