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
#include "../authinfo.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class CPPWAMP_API AnonymousAuthenticator
{
public:
    using RandomNumberGenerator = std::function<uint64_t ()>;

    AnonymousAuthenticator();

    explicit AnonymousAuthenticator(String authRole);

    AnonymousAuthenticator(String authRole, uint64_t rngSeed);

    AnonymousAuthenticator(String authRole, RandomNumberGenerator rng);

    void operator()(AuthExchange::Ptr ex);

private:
    String authRole_;
    RandomNumberGenerator rng_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../../internal/anonymousauthenticator.ipp"
#endif

#endif // CPPWAMP_AUTHENTICATORS_ANONYMOUSAUTHENTICATOR_HPP
