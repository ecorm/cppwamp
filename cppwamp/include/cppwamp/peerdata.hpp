/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PEERDATA_HPP
#define CPPWAMP_PEERDATA_HPP

#include "options.hpp"
#include "payload.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    Contains declarations for data types exchanged with WAMP sessions. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI and other options contained within
    `GOODBYE` messages. */
//------------------------------------------------------------------------------
class Reason : public Options<Reason>
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
class Error : public Options<Error>, public Payload<Error>
{
public:
    /** Constructs an empty error. */
    Error();

    /** Converting constructor taking a reason URI. */
    Error(String reason);

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

//------------------------------------------------------------------------------
/** Provides the _AuthMethod_ and _Extra_ dictionary contained within
    WAMP `CHALLENGE` messages. */
//------------------------------------------------------------------------------
class Challenge : public Options<Challenge>
{
public:
    /** Constructs an empty challenge. */
    Challenge();

    /** Converting constructor taking the authentication method string. */
    explicit Challenge(String method);

    /** Obtains the authentication method string. */
    const String& method() const;

private:
    String method_;

public:
    String& method(internal::PassKey); // Internal use only
};

//------------------------------------------------------------------------------
/** Provides the _Signature_ and _Extra_ dictionary contained within
    WAMP `AUTHENTICATE` messages. */
//------------------------------------------------------------------------------
class Authentication : public Options<Authentication>
{
public:
    /** Constructs an empty challenge. */
    Authentication();

    /** Converting constructor taking the authentication signature. */
    Authentication(String signature);

    /** Obtains the authentication signature. */
    const String& signature() const;

private:
    String signature_;

public:
    String& signature(internal::PassKey); // Internal use only
};


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/peerdata.ipp"
#endif

#endif // CPPWAMP_PEERDATA_HPP
