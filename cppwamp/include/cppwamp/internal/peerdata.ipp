/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../peerdata.hpp"
#include <utility>
#include "../api.hpp"
#include "callee.hpp"
#include "challengee.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_INLINE MatchPolicy getMatchPolicyOption(const T& messageData)
{
    const auto& opts = messageData.options();
    auto found = opts.find("match");
    if (found == opts.end())
        return MatchPolicy::exact;
    const auto& opt = found->second;
    if (opt.template is<String>())
    {
        const auto& s = opt.template as<String>();
        if (s == "prefix")
            return MatchPolicy::prefix;
        if (s == "wildcard")
            return MatchPolicy::wildcard;
    }
    return MatchPolicy::unknown;
}

//------------------------------------------------------------------------------
template <typename T>
CPPWAMP_INLINE void setMatchPolicyOption(T& messageData, MatchPolicy policy)
{
    CPPWAMP_LOGIC_CHECK(policy != MatchPolicy::unknown,
                        "Cannot specify unknown match policy");

    switch (policy)
    {
    case MatchPolicy::exact:
        break;

    case MatchPolicy::prefix:
        messageData.withOption("match", "prefix");
        break;

    case MatchPolicy::wildcard:
        messageData.withOption("match", "wildcard");
        break;

    default:
        assert(false && "Unexpected MatchPolicy enumerator");
    }
}

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
// Abort
//******************************************************************************

CPPWAMP_INLINE Abort::Abort(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Abort::Abort(SessionErrc errc) : Base(errcToUri(errc)) {}

CPPWAMP_INLINE Abort& Abort::withHint(String text)
{
    withOption("message", std::move(text));
    return *this;
}

CPPWAMP_INLINE const String& Abort::uri() const
{
    return message().reasonUri();
}

CPPWAMP_INLINE ErrorOr<String> Abort::hint() const
{
    return optionAs<String>("message");
}

CPPWAMP_INLINE String Abort::errcToUri(SessionErrc errc)
{
    auto uri = errorCodeToUri(errc);
    assert(!uri.empty() && "Error code must map to URI");
    return uri;
}

CPPWAMP_INLINE Abort::Abort(internal::PassKey, internal::AbortMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE internal::AbortMessage& Abort::abortMessage(internal::PassKey)
{
    return message();
}


//******************************************************************************
// Realm
//******************************************************************************

CPPWAMP_INLINE Realm::Realm(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Realm& Realm::captureAbort(Abort& abort)
{
    abort_ = &abort;
    return *this;
}

CPPWAMP_INLINE const String& Realm::uri() const
{
    return message().realmUri();
}

CPPWAMP_INLINE ErrorOr<String> Realm::agent() const
{
    return this->optionAs<String>("agent");
}

CPPWAMP_INLINE ErrorOr<Object> Realm::roles() const
{
    return this->optionAs<Object>("roles");
}

CPPWAMP_INLINE Object Realm::sanitizedOptions() const
{
    auto filtered = options();
    filtered.erase("authextra");
    return filtered;
}

CPPWAMP_INLINE Realm& Realm::withAuthMethods(std::vector<String> methods)
{
    return withOption("authmethods", std::move(methods));
}

CPPWAMP_INLINE Realm& Realm::withAuthId(String authId)
{
    return withOption("authid", std::move(authId));
}

CPPWAMP_INLINE ErrorOr<Array> Realm::authMethods() const
{
    return this->optionAs<Array>("authmethods");
}

CPPWAMP_INLINE ErrorOr<String> Realm::authId() const
{
    return this->optionAs<String>("authid");
}

CPPWAMP_INLINE Realm::Realm(internal::PassKey, internal::HelloMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Abort* Realm::abort(internal::PassKey) {return abort_;}


//******************************************************************************
// SessionInfo
//******************************************************************************

CPPWAMP_INLINE SessionInfo::SessionInfo() {}

CPPWAMP_INLINE SessionId SessionInfo::id() const
{
    return message().sessionId();
}

CPPWAMP_INLINE const String& SessionInfo::realm() const
{
    return realm_;
}

/** @returns The value of the `HELLO.Details.agent|string`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> SessionInfo::agentString() const
{
    return optionAs<String>("agent");
}

/** @returns The value of the `HELLO.Details.roles|dict`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Object> SessionInfo::roles() const
{
    return optionAs<Object>("roles");
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
    detail, or an empty string if not available. */
CPPWAMP_INLINE ErrorOr<String> SessionInfo::authId() const
{
    return optionAs<String>("authid");
}

/** @details
    This function returns the value of the `HELLO.Details.authrole|string`
    detail, if available, or an error code. Not to be confused with
    the _dealer roles_. */
CPPWAMP_INLINE ErrorOr<String> SessionInfo::authRole() const
{
    return optionAs<String>("authrole");
}

/** @details
    This function returns the value of the `HELLO.Details.authmethod|string`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> SessionInfo::authMethod() const
{
    return optionAs<String>("authmethod");
}

/** @details
    This function returns the value of the `HELLO.Details.authprovider|string`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> SessionInfo::authProvider() const
{
    return optionAs<String>("authprovider");
}

/** @details
    This function returns the value of the `HELLO.Details.authextra|object`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Object> SessionInfo::authExtra() const
{
    return optionAs<Object>("authextra");
}

CPPWAMP_INLINE SessionInfo::SessionInfo(internal::PassKey, String&& realm,
                                        internal::WelcomeMessage&& msg)
    : Base(std::move(msg)),
      realm_(std::move(realm))
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& o, const SessionInfo& i)
{
    o << "[ Realm|uri = " << i.realm()
      << ", Session|id = " << i.id();
    if (!i.options().empty())
        o << ", Details|dict = " << i.options();
    return o << " ]";
}


//******************************************************************************
// Reason
//******************************************************************************

CPPWAMP_INLINE Reason::Reason(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Reason& Reason::withHint(String text)
{
    withOption("message", std::move(text));
    return *this;
}

CPPWAMP_INLINE const String& Reason::uri() const
{
    return message().reasonUri();
}

CPPWAMP_INLINE ErrorOr<String> Reason::hint() const
{
    return optionAs<String>("message");
}

CPPWAMP_INLINE Reason::Reason(internal::PassKey, internal::GoodbyeMessage&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// Authentication
//******************************************************************************

CPPWAMP_INLINE Authentication::Authentication() : Base(String{}) {}

CPPWAMP_INLINE Authentication::Authentication(String signature)
    : Base(std::move(signature)) {}

CPPWAMP_INLINE const String& Authentication::signature() const
{
    return message().signature();
}

/** @details
    This function sets the value of the `AUTHENTICATION.Details.nonce|string`
    detail used by the WAMP-SCRAM authentication method. */
CPPWAMP_INLINE Authentication& Authentication::withNonce(std::string nonce)
{
    return withOption("nonce", std::move(nonce));
}

/** @details
    This function sets the values of the
    `AUTHENTICATION.Details.channel_binding|string` and
    `AUTHENTICATION.Details.cbind_data|string`
    details used by the WAMP-SCRAM authentication method. */
CPPWAMP_INLINE Authentication&
Authentication::withChannelBinding(std::string type, std::string data)
{
    withOption("channel_binding", std::move(type));
    return withOption("cbind_data", std::move(data));
}

CPPWAMP_INLINE Authentication::Authentication(
    internal::PassKey, internal::AuthenticateMessage&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// Challenge
//******************************************************************************

CPPWAMP_INLINE Challenge::Challenge(String authMethod)
    : Base(std::move(authMethod))
{}

CPPWAMP_INLINE Challenge& Challenge::withChallenge(String challenge)
{
    return withOption("challenge", std::move(challenge));
}

CPPWAMP_INLINE Challenge& Challenge::withSalt(String salt)
{
    return withOption("salt", std::move(salt));
}

CPPWAMP_INLINE Challenge& Challenge::withKeyLength(UInt keyLength)
{
    return withOption("keylen", keyLength);
}

CPPWAMP_INLINE Challenge& Challenge::withIterations(UInt iterations)
{
    return withOption("iterations", iterations);
}

CPPWAMP_INLINE Challenge& Challenge::withKdf(String kdf)
{
    return withOption("kdf", std::move(kdf));
}

CPPWAMP_INLINE Challenge& Challenge::withMemory(UInt memory)
{
    return withOption("memory", memory);
}

CPPWAMP_INLINE bool Challenge::challengeeHasExpired() const
{
    return challengee_.expired();
}

CPPWAMP_INLINE const String& Challenge::method() const
{
    return message().authMethod();
}

/** @returns The value of the `CHALLENGE.Details.challenge|string` detail used
    by the WAMP-CRA authentication method, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Challenge::challenge() const
{
    return optionAs<String>("challenge");
}

/** @returns The value of the `CHALLENGE.Details.salt|string` detail used by
    the WAMP-CRA authentication method, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Challenge::salt() const
{
    return optionAs<String>("salt");
}

/** @returns The value of the `CHALLENGE.Details.keylen|integer`detail used by
    the WAMP-CRA authentication method, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Challenge::keyLength() const
{
    return toUnsignedInteger("keylen");
}

/** @returns The value of the `CHALLENGE.Details.iterations|integer` detail
    used by the WAMP-CRA and WAMP-SCRAM authentication methods, if available,
    or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Challenge::iterations() const
{
    return toUnsignedInteger("iterations");
}

/** @returns The value of the `CHALLENGE.Details.kdf|string` detail used by
    the WAMP-SCRAM authentication method, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Challenge::kdf() const
{
    return optionAs<String>("kdf");
}

/** @returns The value of the `CHALLENGE.Details.memory|integer` detail used by
    the WAMP-SCRAM authentication method for the Argon2 KDF, if available,
    or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Challenge::memory() const
{
    return toUnsignedInteger("memory");
}

CPPWAMP_INLINE ErrorOrDone Challenge::authenticate(Authentication auth)
{
    // Discard the authentication if client no longer exists
    auto challengee = challengee_.lock();
    if (challengee)
        return challengee->authenticate(std::move(auth));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Challenge::authenticate(ThreadSafe, Authentication auth)
{
    // Discard the authentication if client no longer exists
    auto challengee = challengee_.lock();
    if (challengee)
        return challengee->safeAuthenticate(std::move(auth));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

CPPWAMP_INLINE Challenge::Challenge(internal::PassKey, ChallengeePtr challengee,
                                    internal::ChallengeMessage&& msg)
    : Base(std::move(msg)),
      challengee_(std::move(challengee))
{}


//******************************************************************************
// Error
//******************************************************************************

CPPWAMP_INLINE Error::Error() {}

CPPWAMP_INLINE Error::Error(String reason) : Base(std::move(reason)) {}

CPPWAMP_INLINE Error::Error(std::error_code ec) : Base(toUri(ec)) {}

CPPWAMP_INLINE Error::Error(const error::BadType& e)
    : Base("wamp.error.invalid_argument")
{
    withArgs(String{e.what()});
}

CPPWAMP_INLINE Error::~Error() {}

CPPWAMP_INLINE Error::operator bool() const {return !reason().empty();}

CPPWAMP_INLINE RequestId Error::requestId() const
{
    return message().requestId();
}

CPPWAMP_INLINE const String& Error::reason() const
{
    return message().reasonUri();
}

CPPWAMP_INLINE String Error::toUri(std::error_code ec)
{
    String uri;
    if (ec.category() == wampCategory())
        uri = errorCodeToUri(static_cast<SessionErrc>(ec.value()));
    if (uri.empty())
        uri = "cppwamp.error." + ec.message();
    return uri;
}

CPPWAMP_INLINE Error::Error(internal::PassKey, internal::ErrorMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Error::setRequestId(internal::PassKey, RequestId rid)
{
    message().setRequestId(rid);
}

CPPWAMP_INLINE internal::ErrorMessage&
Error::errorMessage(internal::PassKey, internal::WampMsgType reqType,
                    RequestId reqId)
{
    auto& msg = message();
    msg.setRequestInfo(reqType, reqId);
    return msg;
}


//******************************************************************************
// Topic
//******************************************************************************

CPPWAMP_INLINE Topic::Topic(String uri) : Base(std::move(uri)) {}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option. */
CPPWAMP_INLINE Topic& Topic::withMatchPolicy(MatchPolicy policy)
{
    internal::setMatchPolicyOption(*this, policy);
    return *this;
}

/** Obtains the matching policy used for this subscription. */
CPPWAMP_INLINE MatchPolicy Topic::matchPolicy() const
{
    return internal::getMatchPolicyOption(*this);
}

CPPWAMP_INLINE const String& Topic::uri() const {return message().topicUri();}

CPPWAMP_INLINE Topic::Topic(internal::PassKey, internal::SubscribeMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE String&& Topic::uri(internal::PassKey) &&
{
    return std::move(message()).topicUri();
}

//******************************************************************************
// Pub
//******************************************************************************

CPPWAMP_INLINE Pub::Pub(String topic) : Base(std::move(topic)) {}

CPPWAMP_INLINE const String& Pub::topic() const {return message().topicUri();}

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


//******************************************************************************
// Event
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Event::Event() {}

CPPWAMP_INLINE Event::Event(PublicationId pubId, Object opts)
    : Base(pubId, std::move(opts))
{}

CPPWAMP_INLINE Event& Event::withSubscriptionId(SubscriptionId subId)
{
    Base::message().setSubscriptionId(subId);
    return *this;
}

CPPWAMP_INLINE bool Event::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE SubscriptionId Event::subId() const
{
    return message().subscriptionId();
}

CPPWAMP_INLINE PublicationId Event::pubId() const
{
    return message().publicationId();
}

/** @returns the same object as Session::fallbackExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE AnyCompletionExecutor Event::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Event is empty");
    return executor_;
}

/** @details
    This function returns the value of the `EVENT.Details.publisher|integer`
    detail.
    @returns The publisher ID, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Event::publisher() const
{
    return toUnsignedInteger("publisher");
}

/** @details
    This function returns the value of the `EVENT.Details.trustlevel|integer`
    detail.
    @returns The trust level, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Event::trustLevel() const
{
    return toUnsignedInteger("trustlevel");
}

/** @details
    This function checks the value of the `EVENT.Details.topic|uri` detail.
    @returns The topic URI, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Event::topic() const
{
    return optionAs<String>("topic");
}

CPPWAMP_INLINE Event::Event(internal::PassKey, AnyCompletionExecutor executor,
                            internal::EventMessage&& msg)
    : Base(std::move(msg)),
      executor_(executor)
{}

CPPWAMP_INLINE Event::Event(internal::PassKey, Pub&& pub, SubscriptionId sid,
                            PublicationId pid)
    : Base(std::move(pub.message({})).fields(), sid, pid)
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

CPPWAMP_INLINE Procedure::Procedure(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE const String& Procedure::uri() const &
{
    return message().procedureUri();
}

CPPWAMP_INLINE String&& Procedure::uri() &&
{
    return std::move(message()).procedureUri();
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option. */
CPPWAMP_INLINE Procedure& Procedure::withMatchPolicy(MatchPolicy policy)
{
    internal::setMatchPolicyOption(*this, policy);
    return *this;
}

/** Obtains the matching policy used for this subscription. */
CPPWAMP_INLINE MatchPolicy Procedure::matchPolicy() const
{
    return internal::getMatchPolicyOption(*this);
}

CPPWAMP_INLINE Procedure::Procedure(internal::PassKey,
                                    internal::RegisterMessage&& msg)
    : Base(std::move(msg))
{}

//******************************************************************************
// Rpc
//******************************************************************************

CPPWAMP_INLINE Rpc::Rpc(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE const String& Rpc::procedure() const
{
    return message().procedureUri();
}

CPPWAMP_INLINE Rpc& Rpc::captureError(Error& error)
{
    error_ = &error;
    return *this;
}

/** @details
    This sets the `CALL.Options.receive_progress|bool` option.
    @note this is automatically set by Session::ongoingCall. */
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

CPPWAMP_INLINE void Rpc::setCallerTimeout(CallerTimeoutDuration duration)
{
    CPPWAMP_LOGIC_CHECK(duration.count() >= 0,
                        "Timeout duration must be zero or positive");
    callerTimeout_ = duration;
}

/** @details
    This sets the `CALL.Options.disclose_me|bool` option. */
CPPWAMP_INLINE Rpc& Rpc::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE Rpc& Rpc::withCancelMode(CallCancelMode mode)
{
    cancelMode_ = mode;
    return *this;
}

CPPWAMP_INLINE CallCancelMode Rpc::cancelMode() const {return cancelMode_;}

CPPWAMP_INLINE Rpc::Rpc(internal::PassKey, internal::CallMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Error* Rpc::error(internal::PassKey) {return error_;}

CPPWAMP_INLINE RequestId Rpc::requestId(internal::PassKey) const
{
    return message().fields().at(1).to<RequestId>();
}


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
    return message().requestId();
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

CPPWAMP_INLINE Result::Result(internal::PassKey, internal::ResultMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Result::Result(internal::PassKey, internal::YieldMessage&& msg)
{
    withArgs(std::move(msg).args());
    withKwargs(std::move(msg).kwargs());
}

CPPWAMP_INLINE void Result::setRequestId(internal::PassKey, RequestId rid)
{
    message().setRequestId(rid);
}

CPPWAMP_INLINE internal::YieldMessage& Result::yieldMessage(internal::PassKey,
                                                            RequestId reqId)
{
    message().setRequestId(reqId);
    return message().transformToYield();
}

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
CPPWAMP_INLINE Invocation::Invocation() {}

CPPWAMP_INLINE bool Invocation::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE bool Invocation::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Invocation::requestId() const
{
    return message().requestId();
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
    if (callee)
        return callee->yield(requestId(), std::move(result));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Invocation::yield(ThreadSafe, Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->safeYield(requestId(), std::move(result));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

CPPWAMP_INLINE ErrorOrDone Invocation::yield(Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->yield(requestId(), std::move(error));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Invocation::yield(ThreadSafe, Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->safeYield(requestId(), std::move(error));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

/** @details
    This function checks if the `INVOCATION.Details.receive_progress|bool`
    detail is `true`. */
CPPWAMP_INLINE bool Invocation::isProgressive() const
{
    return optionOr<bool>("receive_progress", false);
}

/** @details
    This function returns the value of the `INVOCATION.Details.caller|integer`
    detail.
    @returns The caller ID, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<UInt> Invocation::caller() const
{
    return toUnsignedInteger("caller");
}

/** @details
    This function returns the value of the `INVOCATION.Details.trustlevel|integer`
    detail.
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE ErrorOr<UInt> Invocation::trustLevel() const
{
    return toUnsignedInteger("trustlevel");
}

/** @details
    This function returns the value of the `INVOCATION.Details.procedure|uri`
    detail.
    @returns A string variant if the procedure URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE ErrorOr<String> Invocation::procedure() const
{
    return optionAs<String>("procedure");
}

CPPWAMP_INLINE Invocation::Invocation(internal::PassKey, CalleePtr callee,
                                      AnyCompletionExecutor executor,
                                      internal::InvocationMessage&& msg)
    : Base(std::move(msg)),
      callee_(callee),
      executor_(executor)
{}

CPPWAMP_INLINE Invocation::Invocation(internal::PassKey, Rpc&& rpc,
                                      RegistrationId regId)
    : Base(std::move(rpc.message({})).fields(), regId)
{}

CPPWAMP_INLINE void Invocation::setRequestId(internal::PassKey, RequestId rid)
{
    message().setRequestId(rid);
}

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
// CallCancellation
//******************************************************************************

CPPWAMP_INLINE CallCancellation::CallCancellation(RequestId reqId,
                                                  CallCancelMode cancelMode)
    : Base(reqId),
      requestId_(reqId),
      mode_(cancelMode)
{
    withOption("mode", internal::callCancelModeToString(cancelMode));
}

CPPWAMP_INLINE RequestId CallCancellation::requestId() const {return requestId_;}

CPPWAMP_INLINE CallCancelMode CallCancellation::mode() const {return mode_;}

CPPWAMP_INLINE CallCancellation::CallCancellation(internal::PassKey,
                                                  internal::CancelMessage&& msg)
    : Base(std::move(msg))
{
    mode_ = internal::parseCallCancelModeFromOptions(options());
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

CPPWAMP_INLINE RequestId Interruption::requestId() const
{
    return message().requestId();
}

CPPWAMP_INLINE CallCancelMode Interruption::cancelMode() const
{
    return cancelMode_;
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
    if (callee)
        return callee->yield(requestId(), std::move(result));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Interruption::yield(ThreadSafe, Result result) const
{
    // Discard the result if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->safeYield(requestId(), std::move(result));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

CPPWAMP_INLINE ErrorOrDone Interruption::yield(Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->yield(requestId(), std::move(error));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone>
Interruption::yield(ThreadSafe, Error error) const
{
    // Discard the error if client no longer exists
    auto callee = callee_.lock();
    if (callee)
        return callee->safeYield(requestId(), std::move(error));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

CPPWAMP_INLINE Interruption::Interruption(internal::PassKey, CalleePtr callee,
                                          AnyCompletionExecutor executor,
                                          internal::InterruptMessage&& msg)
    : Base(std::move(msg)),
      callee_(callee),
      executor_(executor)
{
    cancelMode_ = internal::parseCallCancelModeFromOptions(options());
}

CPPWAMP_INLINE Interruption::Interruption(internal::PassKey, RequestId reqId,
                                          CallCancelMode mode)
    : Base(reqId, Object{{"mode", internal::callCancelModeToString(mode)}}),
      cancelMode_(mode)
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
