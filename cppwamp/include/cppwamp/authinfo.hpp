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
#include "wampdefs.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

// TODO: Add 'transport' dictionary information

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains authentication information associated with a
    WAMP client session. */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthInfo
{
public:
    /// Shared pointer type.
    using Ptr = std::shared_ptr<AuthInfo>;

    /** Default constructor. */
    AuthInfo();

    /** Constructor taking essential information. */
    AuthInfo(String id, String role, String method, String provider);

    /** Adds an `authextra` dictionary to the authentication information. */
    AuthInfo& withExtra(Object extra);

    /** Adds an arbitrary note that can be later accessed by dynamic
        authorizers. */
    AuthInfo& withNote(any note);

    /** Obtains the session ID. */
    SessionId sessionId() const;

    /** Obtains the realm URI. */
    const Uri& realmUri() const;

    /** Obtains the `authid` string. */
    const String& id() const;

    /** Obtains the `authrole` string. */
    const String& role() const;

    /** Obtains the `authmethod` string. */
    const String& method() const;

    /** Obtains the `authprovider` string. */
    const String& provider() const;

    /** Obtains the `authextra` dictionary. */
    const Object& extra() const;

    /** Determines whether the client session is DirectSession or one that
        connected via a server. */
    bool isDirect() const;

    /** Accesses the note containing arbitrary information set by the
        authenticator. */
    const any& note() const;

    /** Resets the instance as if it were default-constructed. */
    void clear();

private:
    String realmUri_;
    String id_;
    String role_;
    String method_;
    String provider_;
    Object extra_;
    any note_;
    SessionId sessionId_ = nullId();
    bool isDirect_ = false;

public:
    // Internal use only
    void join(internal::PassKey, Uri realm, SessionId sessionId, bool isDirect);
    Object join(internal::PassKey, Uri uri, SessionId sessionId,
                Object routerRoles);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authinfo.ipp"
#endif

#endif // CPPWAMP_AUTHINFO_HPP
