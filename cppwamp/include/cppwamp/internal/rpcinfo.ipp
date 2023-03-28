/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../rpcinfo.hpp"
#include <cassert>
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"
#include "../variant.hpp"
#include "callee.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE String callCancelModeToString(CallCancelMode mode)
{
    CPPWAMP_LOGIC_CHECK(mode != CallCancelMode::unknown,
                        "Cannot specify CallCancelMode::unknown");
    switch (mode)
    {
    case CallCancelMode::kill:       return "kill";
    case CallCancelMode::killNoWait: return "killnowait";
    case CallCancelMode::skip:       return "skip";;
    default: assert(false && "Unexpected CallCancelMode enumerator"); break;
    }
    return {};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE CallCancelMode parseCallCancelModeFromOptions(const Object& opts)
{
    auto found = opts.find("mode");
    if (found != opts.end() && found->second.is<String>())
    {
        const auto& s = found->second.as<String>();
        if (s == "kill")
            return CallCancelMode::kill;
        else if (s == "killnowait")
            return CallCancelMode::killNoWait;
        else if (s == "skip")
            return CallCancelMode::skip;
    }
    return CallCancelMode::unknown;
}

} // namespace internal


//******************************************************************************
// Procedure
//******************************************************************************

CPPWAMP_INLINE Procedure::Procedure(Uri uri)
    : Base(std::move(uri))
{}

CPPWAMP_INLINE Procedure::Procedure(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// Rpc
//******************************************************************************

CPPWAMP_INLINE Rpc::Rpc(Uri uri)
    : Base(std::move(uri)),
      progressiveResultsEnabled_(optionOr<bool>("receive_progress", false)),
      isProgress_(this->optionOr<bool>("progress", false))
{}

CPPWAMP_INLINE Rpc::Rpc(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE bool Rpc::progressiveResultsAreEnabled(internal::PassKey) const
{
    return progressiveResultsEnabled_;
}

CPPWAMP_INLINE bool Rpc::isProgress(internal::PassKey) const
{
    return isProgress_;
}

//******************************************************************************
// Result
//******************************************************************************

CPPWAMP_INLINE Result::Result() : Base(in_place, 0, Object{}) {}

CPPWAMP_INLINE Result::Result(std::initializer_list<Variant> list)
    : Base(in_place, 0, Object{}, Array{list})
{}

CPPWAMP_INLINE AccessActionInfo Result::info(bool isServer) const
{
    auto action = isServer ? AccessAction::serverResult
                           : AccessAction::clientYield;
    return {action, requestId(), {}, options()};
}

CPPWAMP_INLINE Result::Result(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE bool Result::isProgress(internal::PassKey) const
{
    return optionOr<bool>("progress", false);
}

CPPWAMP_INLINE void Result::setKindToYield(internal::PassKey)
{
    message().setKind(internal::MessageKind::yield);
}


//******************************************************************************
// Outcome
//******************************************************************************

/** @post `this->type() == Type::result` */
CPPWAMP_INLINE Outcome::Outcome() : Outcome(Result()) {}

/** @post `this->type() == Type::result` */
CPPWAMP_INLINE Outcome::Outcome(Result result) : type_(Type::result)
{
    new (&value_.result) Result(std::move(result));
}

/** @post `this->type() == Type::result` */
CPPWAMP_INLINE Outcome::Outcome(std::initializer_list<Variant> args)
    : Outcome(Result(args))
{}

/** @post `this->type() == Type::error` */
CPPWAMP_INLINE Outcome::Outcome(Error error) : type_(Type::error)
{
    new (&value_.error) Error(std::move(error));
}

/** @post `this->type() == Type::deferred` */
CPPWAMP_INLINE Outcome::Outcome(Deferment) : type_(Type::deferred)
{}

/** @post `this->type() == other.type()` */
CPPWAMP_INLINE Outcome::Outcome(const Outcome& other) {copyFrom(other);}

/** @post `this->type() == other.type()`
    @post `other.type() == Type::deferred` */
CPPWAMP_INLINE Outcome::Outcome(wamp::Outcome&& other)
{
    moveFrom(std::move(other));
}

CPPWAMP_INLINE Outcome::~Outcome()
{
    destruct();
    type_ = Type::deferred;
}

CPPWAMP_INLINE Outcome::Type Outcome::type() const {return type_;}

/** @pre this->type() == Type::result */
CPPWAMP_INLINE const Result& Outcome::asResult() const &
{
    assert(type_ == Type::result);
    return value_.result;
}

/** @pre this->type() == Type::result */
CPPWAMP_INLINE Result&& Outcome::asResult() &&
{
    assert(type_ == Type::result);
    return std::move(value_.result);
}

/** @pre this->type() == Type::error */
CPPWAMP_INLINE const Error& Outcome::asError() const &
{
    assert(type_ == Type::error);
    return value_.error;
}

/** @pre this->type() == Type::error */
CPPWAMP_INLINE Error&& Outcome::asError() &&
{
    assert(type_ == Type::error);
    return std::move(value_.error);
}

/** @post `this->type() == other.type()` */
CPPWAMP_INLINE Outcome& Outcome::operator=(const Outcome& other)
{
    if (type_ != other.type_)
    {
        destruct();
        copyFrom(other);
    }
    else switch (type_)
        {
        case Type::result:
            value_.result = other.value_.result;
            break;

        case Type::error:
            value_.error = other.value_.error;
            break;

        default:
            // Do nothing
            break;
        }

    return *this;
}

/** @post `this->type() == other.type()`
    @post `other.type() == Type::deferred` */
CPPWAMP_INLINE Outcome& Outcome::operator=(Outcome&& other)
{
    if (type_ != other.type_)
    {
        destruct();
        moveFrom(std::move(other));
    }
    else switch (type_)
        {
        case Type::result:
            value_.result = std::move(other.value_.result);
            break;

        case Type::error:
            value_.error = std::move(other.value_.error);
            break;

        default:
            // Do nothing
            break;
        }

    return *this;
}

CPPWAMP_INLINE Outcome::Outcome(std::nullptr_t) : type_(Type::deferred) {}

CPPWAMP_INLINE void Outcome::copyFrom(const Outcome& other)
{
    type_ = other.type_;
    switch(type_)
    {
    case Type::result:
        new (&value_.result) Result(other.value_.result);
        break;

    case Type::error:
        new (&value_.error) Error(other.value_.error);
        break;

    default:
        // Do nothing
        break;
    }
}

CPPWAMP_INLINE void Outcome::moveFrom(Outcome&& other)
{
    type_ = other.type_;
    switch(type_)
    {
    case Type::result:
        new (&value_.result) Result(std::move(other.value_.result));
        break;

    case Type::error:
        new (&value_.error) Error(std::move(other.value_.error));
        break;

    default:
        // Do nothing
        break;
    }

    other.destruct();
    other.type_ = Type::deferred;
}

CPPWAMP_INLINE void Outcome::destruct()
{
    switch(type_)
    {
    case Type::result:
        value_.result.~Result();
        break;

    case Type::error:
        value_.error.~Error();
        break;

    default:
        // Do nothing
        break;
    }
}


//******************************************************************************
// Invocation
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Invocation::Invocation() : Base(in_place, 0, 0, Object{}) {}

CPPWAMP_INLINE bool Invocation::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE bool Invocation::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Invocation::requestId() const {return requestId_;}

CPPWAMP_INLINE RegistrationId Invocation::registrationId() const
{
    return registrationId_;
}

/** @returns the same object as Session::fallbackExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyCompletionExecutor Invocation::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Invocation is empty");
    return executor_;
}

CPPWAMP_INLINE ErrorOrDone Invocation::yield(Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return false;
    return callee->yield(std::move(result), requestId_);
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Invocation::yield(ThreadSafe, Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return futureValue(false);

    return callee->safeYield(std::move(result), requestId_);
}

CPPWAMP_INLINE ErrorOrDone Invocation::yield(Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return false;
    return callee->yield(std::move(error), requestId_);
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Invocation::yield(ThreadSafe, Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return futureValue(false);
    return callee->safeYield(std::move(error), requestId_);
}

CPPWAMP_INLINE AccessActionInfo Invocation::info() const
{
    return {AccessAction::serverInvocation, requestId(), {}, options()};
}

/** @details
    This function returns the value of the `INVOCATION.Details.caller|integer`
    detail.
    @returns The caller ID, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<SessionId> Invocation::caller() const
{
    return toUnsignedInteger("caller");
}

/** @details
    This function returns the value of the `INVOCATION.Details.trustlevel|integer`
    detail.
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE ErrorOr<TrustLevel> Invocation::trustLevel() const
{
    return toUnsignedInteger("trustlevel");
}

/** @details
    This function returns the value of the `INVOCATION.Details.procedure|uri`
    detail.
    @returns A string variant if the procedure URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE ErrorOr<Uri> Invocation::procedure() const
{
    return optionAs<String>("procedure");
}

CPPWAMP_INLINE std::future<ErrorOrDone> Invocation::futureValue(bool value)
{
    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(value);
    return f;
}

CPPWAMP_INLINE Invocation::Invocation(
    internal::PassKey, internal::Message&& msg, CalleePtr callee,
    AnyCompletionExecutor userExec)
    : Base(std::move(msg)),
      callee_(std::move(callee)),
      executor_(std::move(userExec)),
      requestId_(Base::requestId()),
      registrationId_(message().to<RegistrationId>(registrationIdPos_))
{}

CPPWAMP_INLINE Invocation::Invocation(internal::PassKey, Rpc&& rpc,
                                      RegistrationId regId)
    : Base(std::move(rpc))
{
    message().setKind(internal::MessageKind::invocation);
    message().at(2) = regId;
    message().at(3) = Object{};
}

CPPWAMP_INLINE Invocation::CalleePtr Invocation::callee(internal::PassKey) const
{
    return callee_;
}

CPPWAMP_INLINE bool Invocation::isProgress(internal::PassKey) const
{
    return optionOr<bool>("progress", false);
}

CPPWAMP_INLINE bool Invocation::resultsAreProgressive(internal::PassKey) const
{
    return optionOr<bool>("receive_progress", false);
}

//******************************************************************************
// CallCancellation
//******************************************************************************

CPPWAMP_INLINE CallCancellation::CallCancellation(RequestId reqId,
                                                  CallCancelMode cancelMode)
    : Base(in_place, reqId, Object{}),
      requestId_(reqId),
      mode_(cancelMode)
{
    withOption("mode", internal::callCancelModeToString(cancelMode));
}

CPPWAMP_INLINE RequestId CallCancellation::requestId() const
{
    return requestId_;
}

CPPWAMP_INLINE CallCancelMode CallCancellation::mode() const {return mode_;}

CPPWAMP_INLINE AccessActionInfo CallCancellation::info() const
{
    return {AccessAction::clientCancel, requestId(), {}, options()};
}

CPPWAMP_INLINE CallCancellation::CallCancellation(internal::PassKey,
                                                  internal::Message&& msg)
    : Base(std::move(msg)),
      requestId_(Base::requestId()),
      mode_(internal::parseCallCancelModeFromOptions(options()))
{}


//******************************************************************************
// Interruption
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Interruption::Interruption() : Base(in_place, 0, Object{}) {}

CPPWAMP_INLINE bool Interruption::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE bool Interruption::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Interruption::requestId() const {return requestId_;}

CPPWAMP_INLINE CallCancelMode Interruption::cancelMode() const
{
    return cancelMode_;
}

CPPWAMP_INLINE ErrorOr<Uri> Interruption::reason() const
{
    return optionAs<String>("reason");
}

/** @returns the same object as Session::fallbackExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyCompletionExecutor Interruption::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Interruption is empty");
    return executor_;
}

CPPWAMP_INLINE ErrorOrDone Interruption::yield(Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return false;
    return callee->yield(std::move(result), requestId_);
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Interruption::yield(ThreadSafe, Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return futureValue(false);

    return callee->safeYield(std::move(result), requestId_);
}

CPPWAMP_INLINE ErrorOrDone Interruption::yield(Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return false;

    return callee->yield(std::move(error), requestId_);
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Interruption::yield(ThreadSafe, Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (!callee)
        return futureValue(false);

    return callee->safeYield(std::move(error), requestId_);
}

CPPWAMP_INLINE AccessActionInfo Interruption::info() const
{
    return {AccessAction::serverInterrupt, requestId(), {}, options()};
}

CPPWAMP_INLINE std::future<ErrorOrDone> Interruption::futureValue(bool value)
{
    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(value);
    return f;
}

CPPWAMP_INLINE Object Interruption::makeOptions(CallCancelMode mode,
                                                WampErrc reason)
{
    // Interrupt reason: proposed in
    // https://github.com/wamp-proto/wamp-proto/issues/156
    return Object{{"mode", internal::callCancelModeToString(mode)},
                  {"reason", errorCodeToUri(reason)}};
}

CPPWAMP_INLINE Interruption::Interruption(
    internal::PassKey, internal::Message&& msg, CalleePtr callee,
    AnyCompletionExecutor executor)
    : Base(std::move(msg)),
      callee_(callee),
      executor_(executor),
      requestId_(Base::requestId()),
      cancelMode_(internal::parseCallCancelModeFromOptions(options()))
{}

CPPWAMP_INLINE Interruption::Interruption(
    internal::PassKey, RequestId reqId, CallCancelMode mode, WampErrc reason)
    : Base(in_place, reqId, makeOptions(mode, reason)),
      requestId_(reqId),
      cancelMode_(mode)
{}

CPPWAMP_INLINE Interruption::CalleePtr
Interruption::callee(internal::PassKey) const
{
    return callee_;
}

} // namespace wamp
