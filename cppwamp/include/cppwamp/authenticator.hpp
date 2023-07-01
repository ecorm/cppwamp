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
#include "asiodefs.hpp"
#include "any.hpp"
#include "anyhandler.hpp"
#include "api.hpp"
#include "clientinfo.hpp"
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
    const Petition& hello() const;

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

    /** Sends a WELCOME message to the client with the given authentication
        information. */
    void welcome(AuthInfo info);

    /** Rejects the authentication request by sending an ABORT message to
        the client. */
    void reject(Reason r = {WampErrc::authenticationDenied});

private:
    using ChallengerPtr = std::weak_ptr<internal::Challenger>;

    AuthExchange(Petition&& p, ChallengerPtr c);

    Petition hello_;
    ChallengerPtr challenger_;
    Challenge challenge_;
    Authentication authentication_;
    any note_; // Keeps the authenticator stateless
    unsigned challengeCount_ = 0;

public: // Internal use only
    static Ptr create(internal::PassKey, Petition&& p, ChallengerPtr c);
    void setAuthentication(internal::PassKey, Authentication&& a);
    Petition& hello(internal::PassKey);
};


//------------------------------------------------------------------------------
/** Interface for user-defined authenticators. */
//------------------------------------------------------------------------------
class CPPWAMP_API Authenticator : std::enable_shared_from_this<Authenticator>
{
public:
    using Ptr = std::shared_ptr<Authenticator>;

    virtual ~Authenticator() = default;

    void authenticate(AuthExchange::Ptr exchange, AnyIoExecutor& exec)
    {
        if (executor_ == nullptr)
        {
            onAuthenticate(std::move(exchange));
        }
        else
        {
            auto self = shared_from_this();
            boost::asio::post(
                exec,
                boost::asio::bind_executor(
                    executor_,
                    [this, self, exchange]() {onAuthenticate(exchange);} ));
        }
    }

    void bindExecutor(AnyCompletionExecutor e) {executor_ = std::move(e);}

protected:
    virtual void onAuthenticate(AuthExchange::Ptr) = 0;

private:
    AnyCompletionExecutor executor_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/authenticator.inl.hpp"
#endif

#endif // CPPWAMP_AUTHENTICATOR_HPP
