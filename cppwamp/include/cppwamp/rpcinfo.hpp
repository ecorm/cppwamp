/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RPCINFO_HPP
#define CPPWAMP_RPCINFO_HPP

#include <chrono>
#include <initializer_list>
#include <memory>
#include "accesslogging.hpp"
#include "api.hpp"
#include "anyhandler.hpp"
#include "cancellation.hpp"
#include "config.hpp"
#include "errorcodes.hpp"
#include "errorinfo.hpp"
#include "erroror.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "tagtypes.hpp"
#include "variantdefs.hpp"
#include "wampdefs.hpp"
#include "internal/clientcontext.hpp"
#include "internal/passkey.hpp"
#include "internal/matchpolicyoption.hpp"
#include "internal/message.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides data structures for information exchanged via WAMP
           RPC messages. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provide common properties of procedure-like objects. */
//------------------------------------------------------------------------------
template <typename TDerived>
class ProcedureLike : public Options<TDerived, internal::MessageKind::enroll>
{
public:
    /** Obtains the procedure URI. */
    const Uri& uri() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Pattern-based Registrations
        See [Pattern-based Registrations in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-pattern-based-registrations)
        @{ */

    /** Sets the matching policy to be used for this registration. */
    TDerived& withMatchPolicy(MatchPolicy);

    /** Obtains the matching policy used for this registration. */
    MatchPolicy matchPolicy() const;
    /// @}

protected:
    ProcedureLike(String&& uri);

    ProcedureLike(internal::Message&& msg);

private:
    static constexpr unsigned uriPos_ = 3;

    using Base = Options<TDerived, internal::MessageKind::enroll>;

    TDerived& derived() {return static_cast<TDerived&>(*this);}

public:
    // Internal use only
    Uri&& uri(internal::PassKey);
    void setTrustLevel(internal::PassKey, TrustLevel);
};


//------------------------------------------------------------------------------
/** Contains the procedure URI and other options contained within
    WAMP `REGISTER` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Procedure: public ProcedureLike<Procedure>
{
public:
    /** Converting constructor taking a procedure URI. */
    Procedure(Uri uri);

private:
    using Base = ProcedureLike<Procedure>;

public:
    // Internal use only
    Procedure(internal::PassKey, internal::Message&& msg);
};


//------------------------------------------------------------------------------
/** Provides properties common to RPC-like objects. */
//------------------------------------------------------------------------------
template <typename TDerived>
class CPPWAMP_API RpcLike
    : public Payload<TDerived, internal::MessageKind::call>
{
public:
    /** The duration type used for caller-initiated timeouts. */
    using TimeoutDuration = std::chrono::steady_clock::duration;

    /** The duration type used for dealer-initiated timeouts. */
    using DealerTimeoutDuration = std::chrono::duration<UInt, std::milli>;

    /** Specifies the Error object in which to store call errors returned
        by the callee. */
    TDerived& captureError(Error& error);

    /** Obtains the procedure URI. */
    const Uri& uri() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Call Timeouts
        See [Call Timeouts in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-call-timeouts).
        Setting a duration of zero deactivates the timeout.
        @{ */

    /** Requests that the caller cancel the call after the specified
        timeout duration.
        If negative, the given timeout is clamped to zero. */
    TDerived& withCallerTimeout(TimeoutDuration timeout);

    /** Obtains the caller timeout duration. */
    TimeoutDuration callerTimeout() const;

    /** Requests that the dealer cancel the call after the specified
        timeout duration. */
    TDerived& withDealerTimeout(DealerTimeoutDuration timeout);

    /** Obtains the dealer timeout duration. */
    ErrorOr<DealerTimeoutDuration> dealerTimeout() const;

    /// @}

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-caller-identification)
        @{ */

    /** Requests that the identity of the caller be disclosed in the
        call invocation. */
    TDerived& withDiscloseMe(bool disclosed = true);

    /** Determines if caller disclosure was requested. */
    bool discloseMe() const;
    /// @}

    /** @name Call Cancellation
        @{ */

    /** The default cancel mode when none is specified. */
    static constexpr CallCancelMode defaultCancelMode() noexcept
    {
        return CallCancelMode::kill;
    }

    /** Sets the default cancellation mode to use when none is specified. */
    TDerived& withCancelMode(CallCancelMode mode);

    /** Obtains the default cancellation mode associated with this RPC. */
    CallCancelMode cancelMode() const;

    /** Assigns a cancellation slot that can be activated via its associated
        signal. */
    TDerived& withCancellationSlot(CallCancellationSlot slot);
    /// @}

protected:
    RpcLike(Uri&& uri);
    RpcLike(internal::Message&& msg);

private:
    static constexpr unsigned uriPos_  = 3;
    static constexpr unsigned argsPos_ = 4;

    using Base = Payload<TDerived, internal::MessageKind::call>;

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    CallCancellationSlot cancellationSlot_;
    Error* error_ = nullptr;
    TimeoutDuration callerTimeout_ = {};
    TrustLevel trustLevel_ = 0;
    CallCancelMode cancelMode_ = defaultCancelMode();
    bool hasTrustLevel_ = false;
    bool disclosed_ = false;

public:
    // Internal use only
    CallCancellationSlot& cancellationSlot(internal::PassKey);
    Error* error(internal::PassKey);
    void setDisclosed(internal::PassKey, bool disclosed);
    void setTrustLevel(internal::PassKey, TrustLevel trustLevel);
    bool disclosed(internal::PassKey) const;
    bool hasTrustLevel(internal::PassKey) const;
    TrustLevel trustLevel(internal::PassKey) const;
};

//------------------------------------------------------------------------------
/** Contains the procedure URI, options, and payload contained within
    WAMP `CALL` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Rpc : public RpcLike<Rpc>
{
public:
    /** Converting constructor taking a procedure URI. */
    Rpc(Uri uri);

 private:
    using Base = RpcLike<Rpc>;

    bool progressiveResultsEnabled_ = false;
    bool isProgress_ = false;

public:
    // Internal use only
    Rpc(internal::PassKey, internal::Message&& msg);
    bool progressiveResultsAreEnabled(internal::PassKey) const;
    bool isProgress(internal::PassKey) const;
};


//------------------------------------------------------------------------------
/** Contains the remote procedure result options/payload within WAMP
    `RESULT` and `YIELD` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Result : public Payload<Result, internal::MessageKind::result>
{
public:
    /** Default constructor. */
    Result();

    /** Converting constructor taking a braced initializer list of
        positional arguments. */
    Result(std::initializer_list<Variant> list);

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    static constexpr unsigned argsPos_ = 3;

    using Base = Payload<Result, internal::MessageKind::result>;

    Result(RequestId reqId, Object&& details);

public:
    // Internal use only
    Result(internal::PassKey, internal::Message&& msg);
    bool isProgress(internal::PassKey) const;
    void setKindToYield(internal::PassKey);
    void setKindToResult(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Tag type that can be passed to wamp::Outcome to construct a
    deferred outcome.
    Use the wamp::deferment constant object to more conveniently pass this tag. */
//------------------------------------------------------------------------------
struct Deferment
{
    constexpr Deferment() noexcept = default;
};

//------------------------------------------------------------------------------
/** Convenient value of the wamp::Deferment tag type that can be passed to
    the wamp::Outcome constructor. */
//------------------------------------------------------------------------------
constexpr CPPWAMP_INLINE_VARIABLE Deferment deferment;

//------------------------------------------------------------------------------
/** Contains the outcome of an RPC invocation.
    @see @ref RpcOutcomes */
//------------------------------------------------------------------------------
class CPPWAMP_API Outcome
{
public:
    /** Enumerators representing the type of outcome being held by
        this object. */
    enum class Type
    {
        deferred, ///< A `YIELD` has been, or will be, sent manually.
        result,   ///< Contains a wamp::Result to be yielded back to the caller.
        error     ///< Contains a wamp::Error to be yielded back to the caller.
    };

    /** Default-constructs an outcome containing an empty Result object. */
    Outcome();

    /** Converting constructor taking a Result object. */
    Outcome(Result result);

    /** Converting constructor taking a braced initializer list of positional
        arguments to be stored in a Result. */
    Outcome(std::initializer_list<Variant> args);

    /** Converting constructor taking an Error object. */
    Outcome(Error error);

    /** Converting constructor taking a deferment. */
    Outcome(Deferment);

    /** Copy constructor. */
    Outcome(const Outcome& other);

    /** Move constructor. */
    Outcome(Outcome&& other);

    /** Destructor. */
    ~Outcome();

    /** Obtains the object type being contained. */
    Type type() const;

    /** Accesses the stored Result object. */
    const Result& asResult() const &;

    /** Steals the stored Result object. */
    Result&& asResult() &&;

    /** Accesses the stored Error object. */
    const Error& asError() const &;

    /** Steals the stored Error object. */
    Error&& asError() &&;

    /** Copy-assignment operator. */
    Outcome& operator=(const Outcome& other);

    /** Move-assignment operator. */
    Outcome& operator=(Outcome&& other);

private:
    CPPWAMP_HIDDEN explicit Outcome(std::nullptr_t);
    CPPWAMP_HIDDEN void copyFrom(const Outcome& other);
    CPPWAMP_HIDDEN void moveFrom(Outcome&& other);
    CPPWAMP_HIDDEN void destruct();

    Type type_;

    union CPPWAMP_HIDDEN Value
    {
        Value() {}
        ~Value() {}

        Result result;
        Error error;
    } value_;
};


//------------------------------------------------------------------------------
/** Contains payload arguments and other options within WAMP `INVOCATION`
    messages.

    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Invocation
    : public Payload<Invocation, internal::MessageKind::invocation>
{
public:
    /** Default constructor */
    Invocation();

    /** Returns `false` if the Invocation has been initialized and is ready
        for use. */
    bool empty() const;

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool calleeHasExpired() const;

    /** Obtains the request ID associated with this RPC invocation. */
    RequestId requestId() const;

    /** Obtains the registration ID associated with this RPC invocation. */
    RegistrationId registrationId() const;

    /** Obtains the executor used to execute user-provided handlers. */
    AnyCompletionExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    void yield(Result result = Result());

    /** Manually sends an `ERROR` result back to the callee. */
    void yield(Error error);

    /** Obtains information for the access log. */
    AccessActionInfo info(Uri topic) const;

    /** @name Caller Identification
        See [Caller Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-caller-identification)
        @{ */

    /** Obtains the session ID integer of the caller. */
    ErrorOr<SessionId> caller() const;
    /// @}

    /** @name Call Trust Levels
        See [Call Trust Levels in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-call-trust-levels)
        @{ */

    /** Obtains the trust level integer. */
    ErrorOr<TrustLevel> trustLevel() const;
    /// @}

    /** @name Pattern-based Registrations
        See [Pattern-based Registrations in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-pattern-based-registrations)
        @{ */

    /** Obtains the original procedure URI string used to make this call. */
    ErrorOr<Uri> procedure() const;
    /// @}

private:
    using Base = Payload<Invocation, internal::MessageKind::invocation>;
    using Context = internal::ClientContext;

    static constexpr unsigned registrationIdPos_ = 2;
    static constexpr unsigned optionsPos_ = 3;

    Context callee_;
    AnyCompletionExecutor executor_ = nullptr;
    RegistrationId registrationId_ = nullId();

    template <typename, typename...> friend class CoroInvocationUnpacker;

public:
    // Internal use only
    Invocation(internal::PassKey, internal::Message&& msg);
    Invocation(internal::PassKey, Rpc&& rpc, RegistrationId regId);
    void setCallee(internal::PassKey, Context callee,
                   AnyCompletionExecutor userExec);
    Context callee(internal::PassKey) const;
    bool isProgress(internal::PassKey) const;
    bool resultsAreProgressive(internal::PassKey) const;
};


//------------------------------------------------------------------------------
/** Contains the request ID and options contained within
    WAMP `CANCEL` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API CallCancellation
    : public Options<CallCancellation, internal::MessageKind::cancel>
{
public:
    /** Converting constructor. */
    CallCancellation(RequestId reqId,
                     CallCancelMode cancelMode = Rpc::defaultCancelMode());

    /** Obtains the cancel mode. */
    CallCancelMode mode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

private:
    using Base = Options<CallCancellation, internal::MessageKind::cancel>;

    CallCancelMode mode_ = CallCancelMode::unknown;

public:
    // Internal use only
    CallCancellation(internal::PassKey, internal::Message&& msg);
};

//------------------------------------------------------------------------------
/** Contains details within WAMP `INTERRUPT` messages.

    This class also provides the means for manually sending a `YIELD` or
    `ERROR` result back to the RPC caller. */
//------------------------------------------------------------------------------
class CPPWAMP_API Interruption
    : public Options<Interruption, internal::MessageKind::interrupt>
{
public:
    /** Default constructor */
    Interruption();

    /** Returns `false` if the Interruption has been initialized and is ready
        for use. */
    bool empty() const;

    /** Determines if the Session object that dispatched this
        interruption still exists or has expired. */
    bool calleeHasExpired() const;

    /** Returns the request ID associated with this interruption. */
    RequestId requestId() const;

    /** Obtains the cancellation mode, if available. */
    CallCancelMode cancelMode() const;

    /** Obtains the cancellation reason, if available. */
    ErrorOr<Uri> reason() const;

    /** Obtains the executor used to execute user-provided handlers. */
    AnyCompletionExecutor executor() const;

    /** Manually sends a `YIELD` result back to the callee. */
    void yield(Result result = Result());

    /** Manually sends an `ERROR` result back to the callee. */
    void yield(Error error);

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

private:
    using Base = Options<Interruption, internal::MessageKind::interrupt>;
    using Context = internal::ClientContext;

    static Object makeOptions(CallCancelMode mode, WampErrc reason);

    Context callee_;
    AnyCompletionExecutor executor_ = nullptr;
    RegistrationId registrationId_ = nullId();
    CallCancelMode cancelMode_ = CallCancelMode::unknown;

public:
    // Internal use only
    Interruption(internal::PassKey, internal::Message&& msg);

    Interruption(internal::PassKey, RequestId reqId, CallCancelMode mode,
                 WampErrc reason);

    void setCallee(internal::PassKey, Context callee,
                   AnyCompletionExecutor executor);

    void setRegistrationId(internal::PassKey, RegistrationId regId);

    Context callee(internal::PassKey) const;
};


//******************************************************************************
// ProcedureLike Member Function Definitions
//******************************************************************************

template <typename D>
const Uri& ProcedureLike<D>::uri() const
{
    return this->message().template as<String>(uriPos_);
}

template <typename D>
AccessActionInfo ProcedureLike<D>::info() const
{
    return {AccessAction::clientRegister, this->requestId(), uri(),
            this->options()};
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option. */
template <typename D>
D& ProcedureLike<D>::withMatchPolicy(MatchPolicy policy)
{
    internal::setMatchPolicyOption(*this, policy);
    return derived();
}

template <typename D>
MatchPolicy ProcedureLike<D>::matchPolicy() const
{
    return internal::getMatchPolicyOption(*this);
}

template <typename D>
ProcedureLike<D>::ProcedureLike(String&& uri)
    : Base(in_place, 0, Object{}, std::move(uri))
{}

template <typename D>
ProcedureLike<D>::ProcedureLike(internal::Message&& msg)
    : Base(std::move(msg))
{}

template <typename D>
Uri&& ProcedureLike<D>::uri(internal::PassKey)
{
    return std::move(this->message().template as<String>(uriPos_));
}

template <typename D>
void ProcedureLike<D>::setTrustLevel(internal::PassKey, TrustLevel)
{
    // Not applicable; do nothing
}


//******************************************************************************
// RpcLike Member Function Definitions
//******************************************************************************

template <typename D>
D& RpcLike<D>::captureError(Error& error)
{
    error_ = &error;
    return derived();
}

template <typename D>
const Uri& RpcLike<D>::uri() const
{
    return this->message().template as<String>(uriPos_);
}

template <typename D>
AccessActionInfo RpcLike<D>::info() const
{
    return {AccessAction::clientCall, this->message().requestId(), uri(),
            this->options()};
}

/** @details
    If negative, the given timeout is clamped to zero. */
template <typename D>
D& RpcLike<D>::withCallerTimeout(TimeoutDuration timeout)
{
    if (timeout.count() < 0)
        timeout = {};
    callerTimeout_ = timeout;
    return derived();
}

template <typename D>
typename RpcLike<D>::TimeoutDuration RpcLike<D>::callerTimeout() const
{
    return callerTimeout_;
}

/** @details
    This sets the `CALL.Options.timeout|integer` option. */
template <typename D>
D& RpcLike<D>::withDealerTimeout(DealerTimeoutDuration timeout)
{
    return this->withOption("timeout", timeout.count());
}

template <typename D>
ErrorOr<typename RpcLike<D>::DealerTimeoutDuration>
RpcLike<D>::dealerTimeout() const
{
    auto timeout = this->toUnsignedInteger("timeout");
    if (!timeout)
        return makeUnexpected(timeout.error());
    return DealerTimeoutDuration{*timeout};
}

/** @details
    This sets the `CALL.Options.disclose_me|bool` option. */
template <typename D>
D& RpcLike<D>::withDiscloseMe(bool disclosed)
{
    return this->withOption("disclose_me", disclosed);
}

template <typename D>
bool RpcLike<D>::discloseMe() const
{
    return this->template optionOr<bool>("disclose_me", false);
}

template <typename D>
D& RpcLike<D>::withCancelMode(CallCancelMode mode)
{
    cancelMode_ = mode;
    return derived();
}

template <typename D>
CallCancelMode RpcLike<D>::cancelMode() const {return cancelMode_;}

template <typename D>
D& RpcLike<D>::withCancellationSlot(CallCancellationSlot slot)
{
    cancellationSlot_ = std::move(slot);
    return derived();
}

template <typename D>
RpcLike<D>::RpcLike(Uri&& uri)
    : Base(in_place, 0, Object{}, std::move(uri), Array{}, Object{}) {}

template <typename D>
RpcLike<D>::RpcLike(internal::Message&& msg) : Base(std::move(msg)) {}

template <typename D>
CallCancellationSlot& RpcLike<D>::cancellationSlot(internal::PassKey)
{
    return cancellationSlot_;
}

template <typename D>
Error* RpcLike<D>::error(internal::PassKey) {return error_;}

template <typename D>
void RpcLike<D>::setDisclosed(internal::PassKey, bool disclosed)
{
    disclosed_ = disclosed;
}

template <typename D>
void RpcLike<D>::setTrustLevel(internal::PassKey, TrustLevel trustLevel)
{
    trustLevel_ = trustLevel;
    hasTrustLevel_ = true;
}

template <typename D>
bool RpcLike<D>::disclosed(internal::PassKey) const {return disclosed_;}

template <typename D>
bool RpcLike<D>::hasTrustLevel(internal::PassKey) const
{
    return hasTrustLevel_;
}

template <typename D>
TrustLevel RpcLike<D>::trustLevel(internal::PassKey) const
{
    return trustLevel_;
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/rpcinfo.ipp"
#endif

#endif // CPPWAMP_RPCINFO_HPP
