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
CPPWAMP_INLINE Authenticator::Ptr AnonymousAuthenticator::create()
{
    return Ptr(new AnonymousAuthenticator);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AnonymousAuthenticator::setAuthRole(String authRole)
{
    authRole_ = std::move(authRole);
}

//------------------------------------------------------------------------------
/** @details
    The resulting `authid` is the Base64-encoded string of the randomly
    generated ID. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void
AnonymousAuthenticator::setRandomIdGenerator(RandomNumberGenerator rng)
{
    rng_ = std::move(rng);
}

//------------------------------------------------------------------------------
/** @details
    This resets the default generator state with the given seed. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void AnonymousAuthenticator::setRandomIdGenerator(uint64_t seed)
{
    rng_ = internal::DefaultPRNG64{seed};
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void AnonymousAuthenticator::authenticate(AuthExchange::Ptr ex)
{
    auto n = rng_();
    String authId;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* ptr = reinterpret_cast<const char*>(&n);
    internal::Base64::encode(ptr, sizeof(n), authId);
    ex->welcome({std::move(authId), authRole_, "anonymous", "static"});
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AnonymousAuthenticator::AnonymousAuthenticator()
    : authRole_(defaultAuthRole()),
      rng_(internal::DefaultPRNG64{})
{}

} // namespace wamp
