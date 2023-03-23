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

CPPWAMP_INLINE Reason::Reason(Uri uri)
    : Base(internal::MessageKind::goodbye, {0, std::move(uri), Object{}})
{}

CPPWAMP_INLINE Reason::Reason(std::error_code ec)
    : Reason(errorCodeToUri(ec))
{}

CPPWAMP_INLINE Reason::Reason(WampErrc errc)
    : Reason(errorCodeToUri(errc))
{}

CPPWAMP_INLINE Reason& Reason::withHint(String text)
{
    withOption("message", std::move(text));
    return *this;
}

CPPWAMP_INLINE const Uri& Reason::uri() const
{
    return message().at(2).as<String>();
}

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

CPPWAMP_INLINE Reason::Reason(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Reason::setUri(internal::PassKey, Uri uri)
{
    message().at(2) = std::move(uri);
}

CPPWAMP_INLINE void Reason::setKindToAbort(internal::PassKey)
{
    message().setKind(internal::MessageKind::abort);
}


//******************************************************************************
// Realm
//******************************************************************************

CPPWAMP_INLINE Realm::Realm(Uri uri)
    : Base(internal::MessageKind::hello, {0, std::move(uri), Object{}})
{}

CPPWAMP_INLINE Realm& Realm::captureAbort(Reason& reason)
{
    abortReason_ = &reason;
    return *this;
}

CPPWAMP_INLINE const Uri& Realm::uri() const
{
    return message().at(1).as<String>();
}

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

CPPWAMP_INLINE Realm::Realm(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Reason* Realm::abortReason(internal::PassKey)
{
    return abortReason_;
}


//******************************************************************************
// Welcome
//******************************************************************************

CPPWAMP_INLINE Welcome::Welcome()
    : Base(internal::MessageKind::welcome, {0, 0, Object{}})
{}

CPPWAMP_INLINE SessionId Welcome::id() const
{
    return message().at(1).to<SessionId>();
}

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

CPPWAMP_INLINE Welcome::Welcome(internal::PassKey, Uri&& realm,
                                internal::Message&& msg)
    : Base(std::move(msg)),
      realm_(std::move(realm)),
      features_(parseFeatures(options()))
{}


//******************************************************************************
// Authentication
//******************************************************************************

CPPWAMP_INLINE Authentication::Authentication() : Authentication(String{}) {}

CPPWAMP_INLINE Authentication::Authentication(String signature)
    : Base(internal::MessageKind::authenticate,
           {0, std::move(signature), Object{}})
{}

CPPWAMP_INLINE const String& Authentication::signature() const
{
    return message().at(1).as<String>();
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
    : Base(internal::MessageKind::challenge,
           {0, std::move(authMethod), Object{}})
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
    return message().at(1).as<String>();
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
                                    internal::Message&& msg)
    : Base(std::move(msg)),
      challengee_(std::move(challengee))
{}

} // namespace wamp
