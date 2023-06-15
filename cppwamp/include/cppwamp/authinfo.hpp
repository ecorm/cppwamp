/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_AUTHINFO_HPP
#define CPPWAMP_AUTHINFO_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for authentication information. */
//------------------------------------------------------------------------------

#include <memory>
#include "any.hpp"
#include "api.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains authentication information associated with a client session. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthInfo
{
public:
    /** Defaul constructor. */
    AuthInfo();

    /** Constructor taking essential information. */
    AuthInfo(String id, String role, String method, String provider);

    /** Adds an `authextra` dictionary to the authentication information. */
    AuthInfo& withExtra(Object extra);

    /** Adds an arbitrary note that can be later accessed by dynamic
        authorizers. */
    AuthInfo& withNote(any note);

    /** Obtains the `authid` string. */
    const String& id() const;

    /** Obtains the `authrole` string. */
    const String& role() const;

    /** Obtains the `authmethod` string. */
    const String& method() const;

    /** Obtains the `authprovider` string. */
    const String& provider() const;

    /** Obtains the note containing arbitrary information set by the
        authenticator. */
    const any& note() const;

private:
    String id_;
    String role_;
    String method_;
    String provider_;
    Object extra_;
    any note_;

public: // Internal use only
    Object welcomeDetails(internal::PassKey);
    void setId(internal::PassKey, String id);
};


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authinfo.ipp"
#endif

#endif // CPPWAMP_AUTHINFO_HPP
