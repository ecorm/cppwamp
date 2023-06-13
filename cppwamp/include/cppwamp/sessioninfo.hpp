/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_SESSIONINFO_HPP
#define CPPWAMP_SESSIONINFO_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for authentication information. */
//------------------------------------------------------------------------------

#include <memory>
#include "any.hpp"
#include "api.hpp"
#include "features.hpp"
#include "wampdefs.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

// TODO: Add 'transport' dictionary information

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


//------------------------------------------------------------------------------
/** Contains meta-data associated with a WAMP client session. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo
{
public:
    /// Shared pointer type.
    using Ptr = std::shared_ptr<SessionInfo>;

    /// Immutable shared pointer type.
    using ConstPtr = std::shared_ptr<const SessionInfo>;

    /** Default constructor. */
    SessionInfo();

    /** Obtains the session ID. */
    SessionId sessionId() const;

    /** Obtains the realm URI. */
    const Uri& realmUri() const;

    /** Obtains the authentication information. */
    const AuthInfo& auth() const;

    // TODO: Add agent string

    /** Obtains the `transport` dictionary. */
    const Object& transport() const;

    /** Obtains the client supported features flags. */
    ClientFeatures features() const;

private:
    explicit SessionInfo(AuthInfo&& auth);

    AuthInfo auth_;
    String realmUri_;
    Object transport_;
    ClientFeatures features_;
    SessionId sessionId_ = nullId();

public: // Internal use only
    static Ptr create(internal::PassKey, AuthInfo auth);
    void setSessionId(internal::PassKey, SessionId sid);
    void setTransport(internal::PassKey, Object transport);
    void setFeatures(internal::PassKey, ClientFeatures features);
    Object join(internal::PassKey, Uri uri, Object routerRoles = {});
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/sessioninfo.ipp"
#endif

#endif // CPPWAMP_SESSIONINFO_HPP
