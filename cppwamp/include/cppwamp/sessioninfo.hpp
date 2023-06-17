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
#include "api.hpp"
#include "authinfo.hpp"
#include "features.hpp"
#include "wampdefs.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

namespace internal {class SessionInfoImpl;}

//------------------------------------------------------------------------------
/** Contains meta-data associated with a WAMP client session.

    This is a reference-counted lightweight proxy to the actual object
    containing the information. */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionInfo
{
public:
    /** Default constructor. */
    SessionInfo();

    /** Obtains the session ID. */
    SessionId sessionId() const;

    /** Obtains the realm URI. */
    const Uri& realmUri() const;

    /** Obtains the authentication information. */
    const AuthInfo& auth() const;

    /** Obtains the `transport` dictionary. */
    // TODO: TransportInfo struct
    const Object& transport() const;

    /** Obtains the client agent string. */
    const String& agent() const;

    /** Obtains the client supported features flags. */
    ClientFeatures features() const;

    /** Returns true if this proxy object points to an actual
        information object. */
    explicit operator bool() const;

private:
    std::shared_ptr<const internal::SessionInfoImpl> impl_;

public: // Internal use only
    SessionInfo(internal::PassKey,
                std::shared_ptr<const internal::SessionInfoImpl> impl);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/sessioninfo.ipp"
#endif

#endif // CPPWAMP_SESSIONINFO_HPP
