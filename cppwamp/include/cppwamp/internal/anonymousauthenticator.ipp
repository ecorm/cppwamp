/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../authenticators/anonymousauthenticator.hpp"
#include "../api.hpp"
#include "../internal/base64.hpp"
#include "../internal/random.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnonymousAuthenticator::AnonymousAuthenticator()
    : AnonymousAuthenticator("anonymous")
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnonymousAuthenticator::AnonymousAuthenticator(String authRole)
    : authRole_(std::move(authRole)), rng_(internal::DefaultPRNG64{})
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnonymousAuthenticator::AnonymousAuthenticator(
    String authRole, uint64_t rngSeed)
    : authRole_(std::move(authRole)), rng_(internal::DefaultPRNG64{rngSeed})
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnonymousAuthenticator::AnonymousAuthenticator(
    String authRole, RandomNumberGenerator rng)
    : authRole_(std::move(authRole)), rng_(std::move(rng))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AnonymousAuthenticator::operator()(AuthExchange::Ptr ex)
{
    auto n = rng_();
    String authId;
    auto ptr = reinterpret_cast<const char*>(&n);
    internal::Base64::encode(ptr, sizeof(n), authId);
    ex->welcome({std::move(authId), authRole_, "anonymous", "static"});
}

} // namespace wamp
