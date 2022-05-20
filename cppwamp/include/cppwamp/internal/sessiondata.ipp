/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../sessiondata.hpp"
#include <utility>
#include "callee.hpp"
#include "../api.hpp"
#include "../error.hpp"

namespace wamp
{

//******************************************************************************
// Realm
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Realm::Realm(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE const String& Realm::uri() const {return uri_;}

CPPWAMP_INLINE String& Realm::uri(internal::PassKey) {return uri_;}

CPPWAMP_INLINE Realm& Realm::withAuthMethods(std::vector<String> methods)
{
    return withOption("authmethods", std::move(methods));
}

CPPWAMP_INLINE Realm& Realm::withAuthId(String authId)
{
    return withOption("authid", std::move(authId));
}


//******************************************************************************
// SessionInfo
//******************************************************************************

CPPWAMP_INLINE SessionInfo::SessionInfo() {}

CPPWAMP_INLINE SessionId SessionInfo::id() const {return sid_;}

CPPWAMP_INLINE const String& SessionInfo::realm() const {return realm_;}

/** @returns The value of the `HELLO.Details.agent|string`
             detail, or an empty string if it is not available. */
CPPWAMP_INLINE String SessionInfo::agentString() const
{
    return optionOr("agent", String());
}

/** @returns The value of the `HELLO.Details.roles|dict`
             detail, or an empty Object if it is not available. */
CPPWAMP_INLINE Object SessionInfo::roles() const
{
    return optionOr("roles", Object());
}

/** @details
Possible role strings include:
- `broker`
- `dealer`

@par Example
```
bool supported = sessionInfo.supportsRoles({"broker", "dealer"});
```
*/
CPPWAMP_INLINE bool SessionInfo::supportsRoles(const RoleSet& roles) const
{
    if (roles.empty())
        return true;

    auto optionsIter = options().find("roles");
    if (optionsIter == options().end())
        return false;
    const auto& routerRoles = optionsIter->second.as<Object>();

    for (const auto& role: roles)
    {
        if (routerRoles.count(role) == 0)
            return false;
    }

    return true;
}

/** @details
@par Example
```
bool supported = sessionInfo.supportsFeatures(
{
    { "broker", {"publisher_exclusion", "publisher_identification"} },
    { "dealer", {"call_canceling"} }
});
```
*/
CPPWAMP_INLINE bool SessionInfo::supportsFeatures(
        const FeatureMap& features) const
{
    if (features.empty())
        return true;

    auto optionsIter = options().find("roles");
    if (optionsIter == options().end())
        return false;
    const auto& routerRoles = optionsIter->second.as<Object>();

    for (const auto& reqsKv: features)
    {
        const auto& role = reqsKv.first;
        const auto& reqFeatureSet = reqsKv.second;

        auto routerRoleIter = routerRoles.find(role);
        if (routerRoleIter == routerRoles.end())
            return false;
        const auto& routerRoleMap = routerRoleIter->second.as<Object>();

        auto routerFeaturesIter = routerRoleMap.find("features");
        if (routerFeaturesIter == routerRoleMap.end())
            return false;
        const auto& routerFeatureMap =
                routerFeaturesIter->second.as<Object>();

        for (const auto& reqFeature: reqFeatureSet)
        {
            auto routerFeatureIter = routerFeatureMap.find(reqFeature);
            if (routerFeatureIter == routerFeatureMap.end())
                return false;
            if (routerFeatureIter->second != true)
                return false;
        }
    }

    return true;
}

/** @details
    This function returns the value of the `HELLO.Details.authid|string`
    detail.
    @returns A string variant if the authentication ID is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant SessionInfo::authId() const
{
    return optionByKey("authid");
}

/** @details
    This function returns the value of the `HELLO.Details.authrole|string`
    detail. This is not to be confused with the _dealer roles_.
    @returns A string variant if the authentication role is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant SessionInfo::authRole() const
{
    return optionByKey("authrole");
}

/** @details
    This function returns the value of the `HELLO.Details.authmethod|string`
    detail.
    @returns A string variant if the authentication method is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant SessionInfo::authMethod() const
{
    return optionByKey("authmethod");
}

/** @details
    This function returns the value of the `HELLO.Details.authprovider|string`
    detail.
    @returns A string variant if the authentication provider is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant SessionInfo::authProvider() const
{
    return optionByKey("authprovider");
}

CPPWAMP_INLINE SessionInfo::SessionInfo(internal::PassKey, String realm,
                                        SessionId id, Object details)
    : Options<SessionInfo>(std::move(details)),
      realm_(std::move(realm)),
      sid_(id)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const SessionInfo& info)
{
    out << "[ Realm|uri = " << info.realm()
        << ", Session|id = " << info.id();
    if (!info.options().empty())
        out << ", Details|dict = " << info.options();
    return out << " ]";
}


//******************************************************************************
// Topic
//******************************************************************************

CPPWAMP_INLINE Topic::Topic(String uri) : uri_(std::move(uri)) {}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option to `"prefix"`. */
CPPWAMP_INLINE Topic& Topic::usingPrefixMatch()
{
    return withOption("match", "prefix");
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option to `"wildcard"`. */
CPPWAMP_INLINE Topic& Topic::usingWildcardMatch()
{
    return withOption("match", "wildcard");
}

CPPWAMP_INLINE const String& Topic::uri() const {return uri_;}

CPPWAMP_INLINE String& Topic::uri(internal::PassKey) {return uri_;}


//******************************************************************************
// Pub
//******************************************************************************

CPPWAMP_INLINE Pub::Pub(String topic) : topic_(std::move(topic)) {}

/** @details
    This sets the `PUBLISH.Options.exclude|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedSessions(Array sessionIds)
{
    return withOption("exclude", std::move(sessionIds));
}

/** @details
    This sets the `PUBLISH.Options.exclude_authid|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedAuthIds(Array authIds)
{
    return withOption("exclude_authid", std::move(authIds));
}

/** @details
    This sets the `PUBLISH.Options.exclude_authrole|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedAuthRoles(Array authRoles)
{
    return withOption("exclude_authrole", std::move(authRoles));
}

/** @details
    This sets the `PUBLISH.Options.eligible|list` option. */
CPPWAMP_INLINE Pub& Pub::withEligibleSessions(Array sessionIds)
{
    return withOption("eligible", std::move(sessionIds));
}

/** @details
    This sets the `PUBLISH.Options.eligible_authid|list` option. */
CPPWAMP_INLINE Pub& Pub::withEligibleAuthIds(Array authIds)
{
    return withOption("eligible_authid", std::move(authIds));
}

/** @details
    This sets the `PUBLISH.Options.eligible_authrole|list` option.  */
CPPWAMP_INLINE Pub& Pub::withEligibleAuthRoles(Array authRoles)
{
    return withOption("eligible_authrole", std::move(authRoles));
}

/** @details
    This sets the `PUBLISH.Options.exclude_me|bool` option. */
CPPWAMP_INLINE Pub& Pub::withExcludeMe(bool excluded)
{
    return withOption("exclude_me", excluded);
}

/** @details
    This sets the `PUBLISH.Options.disclose_me|bool` option. */
CPPWAMP_INLINE Pub& Pub::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE String&Pub::topic(internal::PassKey) {return topic_;}


//******************************************************************************
// Event
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Event::Event() {}

CPPWAMP_INLINE bool Event::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE SubscriptionId Event::subId() const {return subId_;}

CPPWAMP_INLINE PublicationId Event::pubId() const {return pubId_;}

/** @returns the same object as Session::userExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyExecutor Event::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Event is empty");
    return executor_;
}

/** @details
    This function returns the value of the `EVENT.Details.publisher|integer`
    detail.
    @returns An integer variant if the publisher ID is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::publisher() const
{
    return optionByKey("publisher");
}

/** @details
    This function returns the value of the `EVENT.Details.trustlevel|integer`
    detail.
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::trustLevel() const
{
    return optionByKey("trustlevel");
}

/** @details
    This function checks the value of the `EVENT.Details.topic|uri` detail.
    @returns A string variant if the topic URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::topic() const
{
    return optionByKey("topic");
}

CPPWAMP_INLINE Event::Event(internal::PassKey, SubscriptionId subId,
            PublicationId pubId, AnyExecutor executor, Object&& details)
    : Options<Event>(std::move(details)),
      subId_(subId),
      pubId_(pubId),
      executor_(executor)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Event& event)
{
    out << "[ Publication|id = " << event.pubId();
    if (!event.options().empty())
        out << ", Details|dict = " << event.options();
    if (!event.args().empty())
        out << ", Arguments|list = " << event.args();
    if (!event.kwargs().empty())
        out << ", ArgumentsKw|dict = " << event.kwargs();
    return out << " ]";
}


//******************************************************************************
// Procedure
//******************************************************************************

CPPWAMP_INLINE Procedure::Procedure(String uri) : uri_(std::move(uri)) {}

/** @details
    This sets the `REGISTER.Options.match|string` option to `"prefix"`. */
CPPWAMP_INLINE Procedure& Procedure::usingPrefixMatch()
{
    return withOption("match", "prefix");
}

CPPWAMP_INLINE const String& Procedure::uri() const {return uri_;}

CPPWAMP_INLINE String& Procedure::uri(internal::PassKey) {return uri_;}

/** @details
    This sets the `REGISTER.Options.match|string` option to `"wildcard"`. */
CPPWAMP_INLINE Procedure& Procedure::usingWildcardMatch()
{
    return withOption("match", "wildcard");
}

/** @details
    This sets the `REGISTER.Options.disclose_caller|bool` option. */
CPPWAMP_INLINE Procedure& Procedure::withDiscloseCaller(bool disclosed)
{
    return withOption("disclose_caller", disclosed);
}


//******************************************************************************
// Rpc
//******************************************************************************

CPPWAMP_INLINE Rpc::Rpc(String procedure) : procedure_(std::move(procedure)) {}

CPPWAMP_INLINE Rpc& Rpc::captureError(Error& error)
{
    error_ = &error;
    return *this;
}

/** @details
    This sets the `CALL.Options.receive_progress|bool` option. */
CPPWAMP_INLINE Rpc& Rpc::withProgressiveResults(bool enabled)
{
    progressiveResultsEnabled_ = enabled;
    return withOption("receive_progress", enabled);
}

CPPWAMP_INLINE bool Rpc::progressiveResultsAreEnabled() const
{
    return progressiveResultsEnabled_;
}

/** @details
    This sets the `CALL.Options.timeout|integer` option. */
CPPWAMP_INLINE Rpc& Rpc::withDealerTimeout(UInt milliseconds)
{
    return withOption("timeout", milliseconds);
}

CPPWAMP_INLINE Rpc& Rpc::withCallerTimeout(UInt milliseconds)
{
    return withCallerTimeout(std::chrono::milliseconds(milliseconds));
}

CPPWAMP_INLINE Rpc::CallerTimeoutDuration Rpc::callerTimeout() const
{
    return callerTimeout_;
}

/** @details
    This sets the `CALL.Options.disclose_me|bool` option. */
CPPWAMP_INLINE Rpc& Rpc::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE void Rpc::setCallerTimeout(CallerTimeoutDuration duration)
{
    CPPWAMP_LOGIC_CHECK(duration.count() >= 0,
                        "Timeout duration must be zero or positive");
    callerTimeout_ = duration;
}

CPPWAMP_INLINE String& Rpc::procedure(internal::PassKey) {return procedure_;}

CPPWAMP_INLINE Error* Rpc::error(internal::PassKey) {return error_;}


//******************************************************************************
// Cancellation
//******************************************************************************

CPPWAMP_INLINE Cancellation::Cancellation(RequestId reqId,
                                          CancelMode cancelMode)
    : requestId_(reqId),
      mode_(cancelMode)
{
    String modeStr;
    switch (cancelMode)
    {
    case CancelMode::kill:
        modeStr = "kill";
        break;

    case CancelMode::killNoWait:
        modeStr = "killnowait";
        break;

    case CancelMode::skip:
        modeStr = "skip";
        break;

    default:
        assert(false && "Unexpected CancelMode enumerator");
        break;
    }

    if (!modeStr.empty())
        withOption("mode", std::move(modeStr));
}

CPPWAMP_INLINE RequestId Cancellation::requestId() const {return requestId_;}

CPPWAMP_INLINE CancelMode Cancellation::mode() const {return mode_;}


//******************************************************************************
// Result
//******************************************************************************

CPPWAMP_INLINE Result::Result() {}

CPPWAMP_INLINE Result::Result(std::initializer_list<Variant> list)
{
    withArgList(Array(list));
}

CPPWAMP_INLINE RequestId Result::requestId() const
{
    return reqId_;
}

/** @details
    This sets the `YIELD.Options.progress|bool` option. */
CPPWAMP_INLINE Result& Result::withProgress(bool progressive)
{
    return withOption("progress", progressive);
}

CPPWAMP_INLINE bool Result::isProgressive() const
{
    return optionOr<bool>("progress", false);
}

CPPWAMP_INLINE Result::Result(internal::PassKey, RequestId reqId,
                              Object&& details)
    : Options<Result>(std::move(details)),
      reqId_(reqId)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Result& result)
{
    out << "[ Request|id = " << result.requestId();
    if (!result.options().empty())
        out << ", Details|dict = " << result.options();
    if (!result.args().empty())
        out << ", Arguments|list = " << result.args();
    if (!result.kwargs().empty())
        out << ", ArgumentsKw|dict = " << result.kwargs();
    return out << " ]";
}

//******************************************************************************
// Outcome
//******************************************************************************

CPPWAMP_INLINE  Outcome Outcome::deferred() {return Outcome(nullptr);}

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
CPPWAMP_INLINE Invocation::Invocation() {}

CPPWAMP_INLINE bool Invocation::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE bool Invocation::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Invocation::requestId() const {return id_;}

/** @returns the same object as Session::userExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyExecutor Invocation::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Invocation is empty");
    return executor_;
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Invocation::yield(Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        callee->yield(id_, std::move(result));
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Invocation::yield(Error error) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        callee->yield(id_, std::move(error));
}

/** @details
    This function checks if the `INVOCATION.Details.receive_progress|bool`
    detail is `true`. */
CPPWAMP_INLINE bool Invocation::isProgressive() const
{
    return optionOr("receive_progress", false);
}

/** @details
    This function returns the value of the `INVOCATION.Details.caller|integer`
    detail.
    @returns An integer variant if the caller ID is available. Otherwise,
             a null variant is returned.*/
CPPWAMP_INLINE Variant Invocation::caller() const
{
    return optionByKey("caller");
}

/** @details
    This function returns the value of the `INVOCATION.Details.trustlevel|integer`
    detail.
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Invocation::trustLevel() const
{
    return optionByKey("trustlevel");
}

/** @details
    This function returns the value of the `INVOCATION.Details.procedure|uri`
    detail.
    @returns A string variant if the procedure URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Invocation::procedure() const
{
    return optionByKey("procedure");
}

CPPWAMP_INLINE Invocation::Invocation(internal::PassKey, CalleePtr callee,
        RequestId id, AnyExecutor executor, Object&& details)
    : Options<Invocation>(std::move(details)),
      callee_(callee),
      id_(id),
      executor_(executor)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const Invocation& inv)
{
    out << "[ Request|id = " << inv.requestId();
    if (!inv.options().empty())
        out << ", Details|dict = " << inv.options();
    if (!inv.args().empty())
        out << ", Arguments|list = " << inv.args();
    if (!inv.kwargs().empty())
        out << ", ArgumentsKw|dict = " << inv.kwargs();
    return out << " ]";
}


//******************************************************************************
// Interruption
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Interruption::Interruption() {}

CPPWAMP_INLINE bool Interruption::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE bool Interruption::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Interruption::requestId() const {return id_;}

/** @returns the same object as Session::userExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyExecutor Interruption::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Interruption is empty");
    return executor_;
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Interruption::yield(Result result) const
{
    auto callee = callee_.lock();
    CPPWAMP_LOGIC_CHECK(!!callee, "Client no longer exists");
    callee->yield(id_, std::move(result));
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Interruption::yield(Error error) const
{
    auto callee = callee_.lock();
    CPPWAMP_LOGIC_CHECK(!!callee, "Client no longer exists");
    callee->yield(id_, std::move(error));
}

CPPWAMP_INLINE Interruption::Interruption(internal::PassKey, CalleePtr callee,
        RequestId id, AnyExecutor executor, Object&& details)
    : Options<Interruption>(std::move(details)),
      callee_(callee),
      id_(id),
      executor_(executor)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const Interruption& intr)
{
    out << "[ Request|id = " << intr.requestId();
    if (!intr.options().empty())
        out << ", Details|dict = " << intr.options();
    return out << " ]";
}

} // namespace wamp
