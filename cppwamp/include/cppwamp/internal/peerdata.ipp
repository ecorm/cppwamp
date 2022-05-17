/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../peerdata.hpp"
#include <utility>
#include "../api.hpp"
#include "challengee.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Reason::Reason(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE const String& Reason::uri() const {return uri_;}

CPPWAMP_INLINE String& Reason::uri(internal::PassKey)
    {return uri_;}


//------------------------------------------------------------------------------
CPPWAMP_INLINE Error::Error() {}

CPPWAMP_INLINE Error::Error(String reason) : reason_(std::move(reason)) {}

CPPWAMP_INLINE Error::Error(const error::BadType& e)
    : reason_("wamp.error.invalid_argument")
{
    withArgs(String{e.what()});
}

CPPWAMP_INLINE Error::~Error() {}

CPPWAMP_INLINE const String& Error::reason() const {return reason_;}

CPPWAMP_INLINE Error::operator bool() const {return !reason_.empty();}

CPPWAMP_INLINE String& Error::reason(internal::PassKey) {return reason_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authentication::Authentication() {}

CPPWAMP_INLINE Authentication::Authentication(String signature)
    : signature_(std::move(signature)) {}

CPPWAMP_INLINE const String& Authentication::signature() const
{return signature_;}

CPPWAMP_INLINE String& Authentication::signature(internal::PassKey)
{return signature_;}

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

//------------------------------------------------------------------------------
CPPWAMP_INLINE Challenge::Challenge() {}

CPPWAMP_INLINE bool Challenge::challengeeHasExpired() const
{
    return challengee_.expired();
}

CPPWAMP_INLINE const String& Challenge::method() const {return method_;}

CPPWAMP_INLINE String& Challenge::method(internal::PassKey) {return method_;}

/** @details
    This function returns the value of the `CHALLENGE.Details.challenge|string`
    detail used by the WAMP-CRA authentication method.
    @returns A string variant if the challenge string is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::challenge() const
{
    return optionByKey("challenge");
}

/** @details
    This function returns the value of the `CHALLENGE.Details.salt|string`
    detail used by the WAMP-CRA authentication method.
    @returns A string variant if the salt is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::salt() const
{
    return optionByKey("salt");
}

/** @details
    This function returns the value of the `CHALLENGE.Details.keylen|integer`
    detail used by the WAMP-CRA authentication method.
    @returns An integer variant if the key length is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::keyLength() const
{
    return optionByKey("keylen");
}

/** @details
    This function returns the value of the `CHALLENGE.Details.iterations|integer`
    detail used by the WAMP-CRA and WAMP-SCRAM authentication methods.
    @returns An integer variant if the iteration count is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::iterations() const
{
    return optionByKey("iterations");
}

/** @details
    This function returns the value of the `CHALLENGE.Details.kdf|string`
    detail used by the WAMP-SCRAM authentication method.
    @returns A string variant if the KDF identifier is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::kdf() const
{
    return optionByKey("kdf");
}

/** @details
    This function returns the value of the `CHALLENGE.Details.memory|integer`
    detail used by the WAMP-SCRAM authentication method for the Argon2 KDF.
    @returns An integer variant if the memory cost factor is available.
             Otherwise, a null variant is returned. */
CPPWAMP_INLINE Variant Challenge::memory() const
{
    return optionByKey("memory");
}

CPPWAMP_INLINE void Challenge::authenticate(Authentication auth)
{
    // Discard the authentication if client no longer exists
    auto challengee = challengee_.lock();
    if (challengee)
        challengee->authenticate(std::move(auth));
}

CPPWAMP_INLINE Challenge::Challenge(internal::PassKey, ChallengeePtr challengee,
                                    String method)
    : challengee_(std::move(challengee)),
      method_(std::move(method))
{}

} // namespace wamp
