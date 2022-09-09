/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_IDGEN_HPP
#define CPPWAMP_INTERNAL_IDGEN_HPP

#include <limits>
#include "../wampdefs.hpp"
#include "../variantdefs.hpp"
#include "../bundled/amosnier_sha256.hpp"
#include "../bundled/sevmeyer_prng.hpp"
#include "base64.hpp"

namespace wamp
{


namespace internal
{

//------------------------------------------------------------------------------
class RandomIdGenerator
{
public:
    RandomIdGenerator() {}

    RandomIdGenerator(EphemeralId seed) : prng_(seed) {}

    EphemeralId operator()()
    {
        EphemeralId n = prng_();

        // Apply bit mask to constrain the distribution to consecutive integers
        // that can be represented by a double.
        static constexpr auto digits = std::numeric_limits<Real>::digits;
        static constexpr EphemeralId mask = (1ull << digits) - 1u;
        n &= mask;

        // Zero is reserved according to the WAMP spec.
        if (n == 0)
            n = 1; // Neglibibly biases the 1 value by 1/2^53

        return n;
    }

private:
    wamp::bundled::prng::Generator prng_;
};

//------------------------------------------------------------------------------
class IdAnonymizer
{
public:
    static std::string anonymize(EphemeralId id)
    {
        /*  Compute SHA256 hash of id, then stringify the result with Base64Url.
            Truncate the hash to 128 bits to keep the anonymized ID reasonably
            short in the logs. Truncating only affects the (exceedingly small)
            probability that two ephemeral ids have the same anonymized ID in
            the logs. See https://security.stackexchange.com/a/34797/169835. */
        uint8_t hash[32];
        wamp::bundled::sha256::calc_sha_256(hash, &id, sizeof(id));
        std::string s;
        Base64Url::encode(hash, 16, s);
        return s;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_IDGEN_HPP
