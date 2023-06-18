/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_AUTHENTICATORS_ANONYMOUSAUTHENTICATOR_HPP
#define CPPWAMP_AUTHENTICATORS_ANONYMOUSAUTHENTICATOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the anynomous authenticator. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <functional>
#include "../api.hpp"
#include "../authenticator.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class CPPWAMP_API AnonymousAuthenticator : public Authenticator
{
public:
    using RandomNumberGenerator = std::function<uint64_t ()>;

    static constexpr const char* defaultAuthRole() {return "anonymous";}

    /** Instantiates an anomymous authenticator. */
    static Ptr create();

    /** Sets the `authrole` property to be assigned to users. */
    void setAuthRole(String authRole);

    /** Sets the random number generator used to produce the `authid` property
        to be assigned to authenticated users. */
    void setRandomIdGenerator(RandomNumberGenerator rng);

    /** Sets the seed to use with the default random `authid` generator. */
    void setRandomIdGenerator(uint64_t seed);

    void authenticate(AuthExchange::Ptr ex) override;

private:
    AnonymousAuthenticator();

    String authRole_;
    RandomNumberGenerator rng_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/anonymousauthenticator.inl.hpp"
#endif

#endif // CPPWAMP_AUTHENTICATORS_ANONYMOUSAUTHENTICATOR_HPP
