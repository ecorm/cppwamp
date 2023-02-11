/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_AUTHENTICATOR_HPP
#define CPPWAMP_AUTHENTICATOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for authentication. */
//------------------------------------------------------------------------------

#include <memory>
#include "any.hpp"
#include "anyhandler.hpp"
#include "authinfo.hpp"
#include "api.hpp"
#include "error.hpp"
#include "peerdata.hpp"
#include "internal/passkey.hpp"
#include "internal/challenger.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains information on an authentication exchange with a router.  */
//------------------------------------------------------------------------------
class CPPWAMP_API AuthExchange
{
public:
    /// Shared pointer type.
    using Ptr = std::shared_ptr<AuthExchange>;

    /** Accesses the HELLO information provided by the client. */
    const Realm& realm() const;

    /** Accesses the CHALLENGE information sent by the router. */
    const Challenge& challenge() const;

    /** Accesses the AUTHENTICATE information sent by the client. */
    const Authentication& authentication() const;

    /** Obtains the number of times a CHALLENGE has been issued. */
    unsigned challengeCount() const;

    /** Accesses the temporary information stored by the authenticator. */
    const any& note() const &;

    /** Moves the temporary information stored by the authenticator. */
    any&& note() &&;

    /** Sends a CHALLENGE message to the client and stores the given note for
        future reference. */
    void challenge(Challenge challenge, any note = {});

    /** Thread-safe challenge to use when the authenticator does not run on
        the same thread as the server. */
    void challenge(ThreadSafe, Challenge challenge, any note = {});

    /** Sends a WELCOME message to the client with the given authentication
        information. */
    void welcome(AuthInfo info);

    /** Thread-safe welcome to use when the authenticator does not run on
        the same thread as the server. */
    void welcome(ThreadSafe, AuthInfo info);

    /** Rejects the authentication request by sending an ABORT message to
        the client. */
    void reject(Abort a = {SessionErrc::cannotAuthenticate});

    /** Thread-safe abort to use when the authenticator does not run on
        the same thread as the server. */
    void reject(ThreadSafe, Abort a = {SessionErrc::cannotAuthenticate});

public:
    // Internal use only
    using ChallengerPtr = std::weak_ptr<internal::Challenger>;
    static Ptr create(internal::PassKey, Realm&& r, ChallengerPtr c);
    void setAuthentication(internal::PassKey, Authentication&& a);

private:
    AuthExchange(Realm&& r, ChallengerPtr c);

    Realm realm_;
    ChallengerPtr challenger_;
    Challenge challenge_;
    Authentication authentication_;
    any note_; // Keeps the authenticator stateless
    unsigned challengeCount_ = 0;
};

//------------------------------------------------------------------------------
using Authenticator = AnyReusableHandler<void (AuthExchange::Ptr)>;

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authenticator.ipp"
#endif

#endif // CPPWAMP_AUTHENTICATOR_HPP