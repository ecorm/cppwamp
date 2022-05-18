/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PEERDATA_HPP
#define CPPWAMP_PEERDATA_HPP

#include <memory>
#include "api.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Contains declarations for data types exchanged with WAMP sessions. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `GOODBYE` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Reason : public Options<Reason>
{
public:
    /** Converting constructor taking an optional reason URI. */
    Reason(String uri = "");

    /** Obtains the reason URI. */
    const String& uri() const;

private:
    String uri_;

public:
    String& uri(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Provides the _reason_ URI, options, and payload arguments contained
    within WAMP `ERROR` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Error : public Options<Error>, public Payload<Error>
{
public:
    /** Constructs an empty error. */
    Error();

    /** Converting constructor taking a reason URI. */
    Error(String reason);

    /** Constructor taking an error::BadType exception and
        interpreting it as a `wamp.error.invalid_argument` reason URI. */
    explicit Error(const error::BadType& e);

    /** Destructor. */
    virtual ~Error();

    /** Conversion to bool operator, returning false if the error is empty. */
    explicit operator bool() const;

    /** Obtains the reason URI. */
    const String& reason() const;

private:
    String reason_;

public:
    String& reason(internal::PassKey); // Internal use only
};

namespace internal { class Challengee; } // Forward declaration

//------------------------------------------------------------------------------
/** Provides the _Signature_ and _Extra_ dictionary contained within
    WAMP `AUTHENTICATE` messages.

    See [Authentication in the WAMP specification]
    (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2) */
//------------------------------------------------------------------------------
class CPPWAMP_API Authentication : public Options<Authentication>
{
public:
    /** Constructs an authentication with an empty signature. */
    Authentication();

    /** Converting constructor taking the authentication signature. */
    Authentication(String signature);

    /** Obtains the authentication signature. */
    const String& signature() const;

    /** Sets the client-server nonce used with the WAMP-SCRAM
        authentication method. */
    Authentication& withNonce(std::string nonce);

    /** Sets the channel binding information used with the WAMP-SCRAM
        authentication method. */
    Authentication& withChannelBinding(std::string type, std::string data);

private:
    String signature_;

public:
    String& signature(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Provides the _AuthMethod_ and _Extra_ dictionary contained within
    WAMP `CHALLENGE` messages.

    See [Authentication in the WAMP specification]
    (https://wamp-proto.org/_static/gen/wamp_latest_ietf.html#rfc.section.14.5.2) */
//------------------------------------------------------------------------------
class CPPWAMP_API Challenge : public Options<Challenge>
{
public:
    /** Constructs an empty challenge. */
    Challenge();

    /** Determines if the Session object that dispatched this
        invocation still exists or has expired. */
    bool challengeeHasExpired() const;

    /** Obtains the authentication method string. */
    const String& method() const;

    /** Returns an optional challenge string. */
    Variant challenge() const;

    /** Returns an optional salt string. */
    Variant salt() const;

    /** Returns an optional key length. */
    Variant keyLength() const;

    /** Returns an optional iteration count. */
    Variant iterations() const;

    /** Returns an optional key derivation function (KDF) identifier. */
    Variant kdf() const;

    /** Returns an optional KDF memory cost factor integer. */
    Variant memory() const;

    /** Sends an `AUTHENTICATE` message back in response to the challenge. */
    void authenticate(Authentication auth);

public:
    // Internal use only
    using ChallengeePtr = std::weak_ptr<internal::Challengee>;
    Challenge(internal::PassKey, ChallengeePtr challengee, String method);

private:
    ChallengeePtr challengee_;
    String method_;

public:
    CPPWAMP_HIDDEN String& method(internal::PassKey); // Internal use only
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/peerdata.ipp"
#endif

#endif // CPPWAMP_PEERDATA_HPP
