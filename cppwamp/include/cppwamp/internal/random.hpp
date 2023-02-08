/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RANDOM_HPP
#define CPPWAMP_INTERNAL_RANDOM_HPP

#include <memory>
#include "../api.hpp"
#include "../bundled/sevmeyer_prng.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct CPPWAMP_HIDDEN DefaultPRNG64
{
    explicit DefaultPRNG64() : prng_(new Gen) {}

    explicit DefaultPRNG64(uint64_t seed) : prng_(new Gen(seed)) {}

    uint64_t operator()() {return (*prng_)();}

private:
    using Gen = bundled::prng::Generator;

    std::shared_ptr<Gen> prng_; // In heap to avoid losing state in copies.
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RANDOM_HPP
