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
/** Contains authentication and other information associated with a
    WAMP client session. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo
{
public:
    /// Shared pointer type.
    using Ptr = std::shared_ptr<SessionInfo>;

    /** Default constructor. */
    SessionInfo();

    /** Constructor taking essential information. */
    // TODO: Bundle these in AuthInfo class struct
    SessionInfo(String id, String role, String method, String provider);

    /** Adds an `authextra` dictionary to the authentication information. */
    SessionInfo& withExtra(Object extra);

    /** Adds an arbitrary note that can be later accessed by dynamic
        authorizers. */
    SessionInfo& withNote(any note);

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

    // TODO: Add agent string
    // TODO: Add feature flags
    // TODO: Pass by shared pointer to const via RealmObserver

    /** Obtains the `authextra` dictionary. */
    const Object& extra() const;

    /** Obtains the `transport` dictionary. */
    const Object& transport() const;

    /** Obtains the note containing arbitrary information set by the
        authenticator. */
    const any& note() const;

    /** Obtains the client supported features flags. */
    ClientFeatures features() const;

    /** Resets the instance as if it were default-constructed. */
    void reset();

private:
    String realmUri_;
    String id_;
    String role_;
    String method_;
    String provider_;
    Object extra_;
    Object transport_;
    any note_;
    ClientFeatures features_;
    SessionId sessionId_ = nullId();

public: // Internal use only
    void setId(internal::PassKey, String id);
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
