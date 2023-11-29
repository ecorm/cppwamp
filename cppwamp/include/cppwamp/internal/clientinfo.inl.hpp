/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../clientinfo.hpp"
#include <array>
#include <type_traits>
#include <utility>
#include "../api.hpp"
#include "../variant.hpp"

namespace wamp
{

//******************************************************************************
// Goodbye
//******************************************************************************

/** Uses the `wamp.close.close_realm` reason URI. */
CPPWAMP_INLINE Goodbye::Goodbye()
    : Goodbye(errorCodeToUri(WampErrc::closeRealm))
{}

CPPWAMP_INLINE Goodbye::Goodbye(Uri uri)
    : Base(in_place, Object{}, std::move(uri))
{}

CPPWAMP_INLINE Goodbye::Goodbye(const UriChar* uri) : Goodbye(Uri{uri}) {}

CPPWAMP_INLINE Goodbye::Goodbye(std::error_code ec)
    : Goodbye(errorCodeToUri(ec))
{}

CPPWAMP_INLINE Goodbye::Goodbye(WampErrc errc)
    : Goodbye(errorCodeToUri(errc))
{}

CPPWAMP_INLINE const Uri& Goodbye::uri() const &
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE Uri&& Goodbye::uri() &&
{
    return std::move(message().as<String>(uriPos_));
}

/** @return WampErrc::unknown if the URI is unknown. */
CPPWAMP_INLINE WampErrc Goodbye::errorCode() const
{
    return errorUriToCode(uri());
}

CPPWAMP_INLINE AccessActionInfo Goodbye::info(bool isServer) const
{
    AccessAction action = isServer ? AccessAction::serverGoodbye
                                   : AccessAction::clientGoodbye;
    return {action, uri(), options()};
}

CPPWAMP_INLINE Goodbye::Goodbye(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Goodbye::setUri(internal::PassKey, Uri uri)
{
    message().at(uriPos_) = std::move(uri);
}


//******************************************************************************
// Abort
//******************************************************************************

CPPWAMP_INLINE Abort::Abort() : Abort(in_place, {}, {}) {}

CPPWAMP_INLINE Abort::Abort(const error::BadType& e)
    : Abort(WampErrc::invalidArgument)
{
    withHint(String{e.what()});
}

CPPWAMP_INLINE Abort& Abort::withHint(String text)
{
    // ABORT payload arguments were only recently added to the WAMP spec,
    // so place the hint in the details dictionary for backward compatibility.
    // https://github.com/wamp-proto/wamp-proto/pull/506
    withOption("message", std::move(text));
    return *this;
}

CPPWAMP_INLINE Abort::operator bool() const {return !uri().empty();}

CPPWAMP_INLINE const Uri& Abort::uri() const &
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE Uri&& Abort::uri() &&
{
    return std::move(message().as<String>(uriPos_));
}

CPPWAMP_INLINE ErrorOr<String> Abort::hint() const &
{
    return optionAs<String>("message");
}

CPPWAMP_INLINE ErrorOr<String> Abort::hint() &&
{
    return std::move(*this).optionAs<String>("message");
}

/** @return WampErrc::unknown if the URI is unknown. */
CPPWAMP_INLINE WampErrc Abort::errorCode() const {return errorUriToCode(uri());}

CPPWAMP_INLINE AccessActionInfo Abort::info(bool isServer) const
{
    AccessAction action = isServer ? AccessAction::serverAbort
                                   : AccessAction::clientAbort;
    return {action, uri(), options()};
}

CPPWAMP_INLINE Abort::Abort(in_place_t, Uri uri, Array args)
    : Base(in_place, Object{}, std::move(uri), std::move(args), Object{})
{}

CPPWAMP_INLINE Abort::Abort(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Abort::Abort(internal::PassKey, WampErrc errc, Object opts)
    : Base(in_place, std::move(opts), errorCodeToUri(errc))
{}

CPPWAMP_INLINE Abort::Abort(internal::PassKey, std::error_code ec, Object opts)
    : Base(in_place, std::move(opts), errorCodeToUri(ec))
{}


//******************************************************************************
// Hello
//******************************************************************************

CPPWAMP_INLINE Hello::Hello(Uri realm)
    : Base(in_place, std::move(realm), Object{})
{}

CPPWAMP_INLINE Hello::Hello(const UriChar* realm) : Hello(Uri{realm}) {}

CPPWAMP_INLINE Hello& Hello::captureAbort(Abort& reason)
{
    abortReason_ = &reason;
    return *this;
}

/** @throws error::Logic if the given timeout duration is negative. */
CPPWAMP_INLINE Hello& Hello::withTimeout(Timeout timeout)
{
    timeout_ = internal::checkTimeout(timeout);
    return *this;
}

/** Version::clientAgentString is used by default if no agent string is
    specified.
    @note The transport's agent string, if applicable, is set independently
          via the transport's endpoint settings. */
CPPWAMP_INLINE Hello& Hello::withAgent(String agent)
{
    return this->withOption("agent", std::move(agent));
}

CPPWAMP_INLINE Timeout Hello::timeout() const {return timeout_;}

CPPWAMP_INLINE const Uri& Hello::uri() const
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE ErrorOr<String> Hello::agent() const
{
    return this->optionAs<String>("agent");
}

CPPWAMP_INLINE ErrorOr<Object> Hello::roles() const
{
    return this->optionAs<Object>("roles");
}

CPPWAMP_INLINE ClientFeatures Hello::features() const
{
    auto found = options().find("roles");
    if (found == options().end())
        return {};
    return ClientFeatures{found->second.as<Object>()};
}

CPPWAMP_INLINE AccessActionInfo Hello::info() const
{
    return {AccessAction::clientHello, uri(), options()};
}

CPPWAMP_INLINE Hello& Hello::withAuthMethods(std::vector<String> methods)
{
    return withOption("authmethods", std::move(methods));
}

CPPWAMP_INLINE Hello& Hello::withAuthId(String authId)
{
    return withOption("authid", std::move(authId));
}

CPPWAMP_INLINE ErrorOr<Array> Hello::authMethods() const
{
    return this->optionAs<Array>("authmethods");
}

CPPWAMP_INLINE ErrorOr<String> Hello::authId() const
{
    return this->optionAs<String>("authid");
}

CPPWAMP_INLINE Hello::Hello(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Abort* Hello::abortReason(internal::PassKey)
{
    return abortReason_;
}

CPPWAMP_INLINE Uri& Hello::uri(internal::PassKey)
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE String Hello::agentOrEmptyString(internal::PassKey)
{
    auto agentOrError = std::move(*this).optionAs<String>("agent");
    auto agent = std::move(agentOrError).value_or(String{});
    return agent;
}


//******************************************************************************
// Welcome
//******************************************************************************

CPPWAMP_INLINE Welcome::Welcome() : Base(in_place, 0, Object{}) {}

CPPWAMP_INLINE SessionId Welcome::sessionId() const
{
    return message().to<SessionId>(sessionIdPos_);
}

CPPWAMP_INLINE const Uri& Welcome::realm() const {return realm_;}

CPPWAMP_INLINE AccessActionInfo Welcome::info() const
{
    return {AccessAction::serverWelcome, realm(), options()};
}

/** @returns The value of the `HELLO.Details.agent|string`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Welcome::clientAgentString() const
{
    return optionAs<String>("agent");
}

/** @returns The value of the `HELLO.Details.roles|dict`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Object> Welcome::roles() const
{
    return optionAs<Object>("roles");
}

CPPWAMP_INLINE RouterFeatures Welcome::features() const {return features_;}

/** @details
    This function returns the value of the `HELLO.Details.authid|string`
    detail, or an empty string if not available. */
CPPWAMP_INLINE ErrorOr<String> Welcome::authId() const
{
    return optionAs<String>("authid");
}

/** @details
    This function returns the value of the `HELLO.Details.authrole|string`
    detail, if available, or an error code. Not to be confused with
    the _dealer roles_. */
CPPWAMP_INLINE ErrorOr<String> Welcome::authRole() const
{
    return optionAs<String>("authrole");
}

/** @details
    This function returns the value of the `HELLO.Details.authmethod|string`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Welcome::authMethod() const
{
    return optionAs<String>("authmethod");
}

/** @details
    This function returns the value of the `HELLO.Details.authprovider|string`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Welcome::authProvider() const
{
    return optionAs<String>("authprovider");
}

/** @details
    This function returns the value of the `HELLO.Details.authextra|object`
    detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Object> Welcome::authExtra() const
{
    return optionAs<Object>("authextra");
}

CPPWAMP_INLINE RouterFeatures Welcome::parseFeatures(const Object& opts)
{
    RouterFeatures features;
    auto found = opts.find("roles");
    if (found != opts.end())
    {
        const auto& roles = found->second;
        if (roles.is<Object>())
            features = RouterFeatures{roles.as<Object>()};
    }
    return features;
}

CPPWAMP_INLINE Welcome::Welcome(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg)),
      features_(parseFeatures(options()))
{}

CPPWAMP_INLINE Welcome::Welcome(internal::PassKey, SessionId sid, Object&& opts)
    : Base(in_place, sid, std::move(opts))
{}

CPPWAMP_INLINE void Welcome::setRealm(internal::PassKey, Uri&& realm)
{
    realm_ = std::move(realm);
}


//******************************************************************************
// Authentication
//******************************************************************************

CPPWAMP_INLINE Authentication::Authentication() : Authentication(String{}) {}

CPPWAMP_INLINE Authentication::Authentication(String signature)
    : Base(in_place, std::move(signature), Object{})
{}

CPPWAMP_INLINE const String& Authentication::signature() const
{
    return message().as<String>(signaturePos_);
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

CPPWAMP_INLINE AccessActionInfo Authentication::info() const
{
    return {AccessAction::clientAuthenticate, "", options()};
}

CPPWAMP_INLINE Authentication::Authentication(internal::PassKey,
                                              internal::Message&& msg)
    : Base(std::move(msg))
{}


//******************************************************************************
// Challenge
//******************************************************************************

CPPWAMP_INLINE Challenge::Challenge(String authMethod)
    : Base(in_place, std::move(authMethod), Object{})
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
    return message().as<String>(authMethodPos_);
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

CPPWAMP_INLINE void Challenge::authenticate(Authentication auth)
{
    challengee_.authenticate(std::move(auth));
}

CPPWAMP_INLINE void Challenge::fail(Abort reason)
{
    challengee_.failAuthentication(std::move(reason));
}

CPPWAMP_INLINE AccessActionInfo Challenge::info() const
{
    return {AccessAction::serverChallenge, method(), options()};
}

CPPWAMP_INLINE Challenge::Challenge(internal::PassKey,
                                    internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Challenge::setChallengee(internal::PassKey,
                                             Context challengee)
{
    challengee_ = std::move(challengee);
}

CPPWAMP_INLINE const std::string& incidentDescription(IncidentKind k)
{
    static constexpr auto count = static_cast<unsigned>(IncidentKind::count);

    static const std::array<std::string, count> descriptions{
    {
        /* transportDropped */ "Transport connection dropped",
        /* closedByPeer */     "Session killed by remote peer",
        /* abortedByPeer */    "Session aborted by remote peer",
        /* commFailure */      "Transport failure or protocol error",
        /* challengeFailure */ "Error reported by CHALLENGE handler",
        /* eventError */       "Error reported by EVENT handler",
        /* unknownErrorUri */  "An ERROR with unknown URI was received",
        /* errorHasPayload */  "An ERROR with payload arguments was received",
        /* trouble */          "A non-fatal problem occurred",
        /* trace */            "Message trace"
        }
    };

    using T = std::underlying_type<IncidentKind>::type;
    auto n = static_cast<T>(k);
    assert(n >= 0);
    return descriptions.at(n);
}


//******************************************************************************
// Incident
//******************************************************************************

CPPWAMP_INLINE Incident::Incident(IncidentKind kind, std::string msg)
    : message_(std::move(msg)),
    kind_(kind)
{}

CPPWAMP_INLINE Incident::Incident(IncidentKind kind, std::error_code ec,
                                  std::string msg)
    : message_(std::move(msg)),
      error_(ec),
      kind_(kind)
{}

CPPWAMP_INLINE Incident::Incident(IncidentKind kind, const Goodbye& g)
    : error_(make_error_code(g.errorCode())),
      kind_(kind)
{
    message_ = "With reason URI " + g.uri();
}

CPPWAMP_INLINE Incident::Incident(IncidentKind kind, const Abort& reason)
    : error_(make_error_code(reason.errorCode())),
      kind_(kind)
{
    message_ = "With reason URI " + reason.uri();
    if (!reason.options().empty())
        message_ += " and details " + toString(reason.options());
}

CPPWAMP_INLINE Incident::Incident(IncidentKind kind, const Error& e)
    : error_(make_error_code(e.errorCode())),
      kind_(kind)
{
    message_ = "With error URI=" + e.uri();
    if (!e.args().empty())
        message_ += ", with args=" + toString(e.args());
    if (!e.kwargs().empty())
        message_ += ", with kwargs=" + toString(e.kwargs());
}

CPPWAMP_INLINE IncidentKind Incident::kind() const {return kind_;}

CPPWAMP_INLINE std::error_code Incident::error() const {return error_;}

CPPWAMP_INLINE std::string Incident::message() const {return message_;}

CPPWAMP_INLINE LogEntry Incident::toLogEntry() const
{
    std::string message = incidentDescription(kind_);
    if (!message_.empty())
        message += ": " + message_;

    LogLevel level = {};
    switch (kind_)
    {
    case IncidentKind::eventError:
        level = LogLevel::error;
        break;

    case IncidentKind::trouble:
        level = (error_ == WampErrc::payloadSizeExceeded) ? LogLevel::error
                                                          : LogLevel::warning;
        break;

    case IncidentKind::trace:
        level = LogLevel::trace;
        break;

    case IncidentKind::unknownErrorUri:
    case IncidentKind::errorHasPayload:
        level = LogLevel::warning;
        break;

    default:
        level = LogLevel::critical;
        break;
    }

    return {level, std::move(message), error_};
}

} // namespace wamp
