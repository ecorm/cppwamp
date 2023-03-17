/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../sessioninfo.hpp"
#include <utility>
#include "../api.hpp"
#include "../variant.hpp"
#include "challengee.hpp"

namespace wamp
{

//******************************************************************************
// Reason
//******************************************************************************

CPPWAMP_INLINE Reason::Reason(Uri uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Reason::Reason(std::error_code ec) : Base(errorCodeToUri(ec)) {}

CPPWAMP_INLINE Reason::Reason(WampErrc errc) : Base(errorCodeToUri(errc)) {}

CPPWAMP_INLINE Reason& Reason::withHint(String text)
{
    withOption("message", std::move(text));
    return *this;
}

CPPWAMP_INLINE const Uri& Reason::uri() const {return message().uri();}

CPPWAMP_INLINE ErrorOr<String> Reason::hint() const
{
    return optionAs<String>("message");
}

/** @return WampErrc::unknown if the URI is unknown. */
CPPWAMP_INLINE WampErrc Reason::errorCode() const
{
    return errorUriToCode(uri());
}

CPPWAMP_INLINE AccessActionInfo Reason::info(bool isServer) const
{
    auto action = isServer ? AccessAction::serverGoodbye
                           : AccessAction::clientGoodbye;
    return {action, uri(), options()};
}

CPPWAMP_INLINE Reason::Reason(internal::PassKey, internal::GoodbyeMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Reason::Reason(internal::PassKey, internal::AbortMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Reason::setUri(internal::PassKey, Uri uri)
{
    message().at(2) = std::move(uri);
}

CPPWAMP_INLINE internal::AbortMessage& Reason::abortMessage(internal::PassKey)
{
    return message().transformToAbort();
}


//******************************************************************************
// Realm
//******************************************************************************

CPPWAMP_INLINE Realm::Realm(Uri uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Realm& Realm::captureAbort(Reason& reason)
{
    abortReason_ = &reason;
    return *this;
}

CPPWAMP_INLINE const Uri& Realm::uri() const {return message().uri();}

CPPWAMP_INLINE ErrorOr<String> Realm::agent() const
{
    return this->optionAs<String>("agent");
}

CPPWAMP_INLINE ErrorOr<Object> Realm::roles() const
{
    return this->optionAs<Object>("roles");
}

CPPWAMP_INLINE AccessActionInfo Realm::info() const
{
    return {AccessAction::clientHello, uri(), options()};
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

CPPWAMP_INLINE Reason* Realm::abortReason(internal::PassKey)
{
    return abortReason_;
}


//******************************************************************************
// Welcome
//******************************************************************************

CPPWAMP_INLINE Welcome::Welcome() {}

CPPWAMP_INLINE SessionId Welcome::id() const {return message().sessionId();}

CPPWAMP_INLINE const Uri& Welcome::realm() const {return realm_;}

CPPWAMP_INLINE AccessActionInfo Welcome::info() const
{
    return {AccessAction::serverWelcome, realm(), options()};
}

/** @returns The value of the `HELLO.Details.agent|string`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<String> Welcome::agentString() const
{
    return optionAs<String>("agent");
}

/** @returns The value of the `HELLO.Details.roles|dict`
             detail, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Object> Welcome::roles() const
{
    return optionAs<Object>("roles");
}

CPPWAMP_INLINE ErrorOr<RouterFeatures> Welcome::features() const
{
    auto found = options().find("roles");
    if (found == options().end())
        return makeUnexpectedError(Errc::absent);
    const auto& roles = found->second;
    if (!roles.is<Object>())
        return makeUnexpectedError(Errc::badType);
    return RouterFeatures{roles.as<Object>()};
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
CPPWAMP_INLINE bool Welcome::supportsRoles(const RoleSet& roles) const
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
CPPWAMP_INLINE bool Welcome::supportsFeatures(const FeatureMap& features) const
{
    // TODO: Implement this in RouterFeatures instead
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

CPPWAMP_INLINE Welcome::Welcome(internal::PassKey, Uri&& realm,
                                internal::WelcomeMessage&& msg)
    : Base(std::move(msg)),
      realm_(std::move(realm))
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

CPPWAMP_INLINE AccessActionInfo Authentication::info() const
{
    return {AccessAction::clientAuthenticate, "", options()};
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

CPPWAMP_INLINE ErrorOrDone Challenge::fail(Reason reason)
{
    auto challengee = challengee_.lock();
    if (challengee)
        return challengee->failAuthentication(std::move(reason));
    return false;
}

CPPWAMP_INLINE std::future<ErrorOrDone> Challenge::fail(ThreadSafe,
                                                        Reason reason)
{
    auto challengee = challengee_.lock();
    if (challengee)
        return challengee->safeFailAuthentication(std::move(reason));

    std::promise<ErrorOrDone> p;
    auto f = p.get_future();
    p.set_value(false);
    return f;
}

CPPWAMP_INLINE AccessActionInfo Challenge::info() const
{
    return {AccessAction::serverChallenge, method(), options()};
}

CPPWAMP_INLINE Challenge::Challenge(internal::PassKey, ChallengeePtr challengee,
                                    internal::ChallengeMessage&& msg)
    : Base(std::move(msg)),
      challengee_(std::move(challengee))
{}

} // namespace wamp
