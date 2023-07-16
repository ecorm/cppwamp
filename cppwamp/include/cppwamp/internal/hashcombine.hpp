/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HASHCOMBINE_HPP
#define CPPWAMP_INTERNAL_HASHCOMBINE_HPP

#include <climits>
#include <cstdint>
#include <functional>

namespace wamp
{

namespace internal
{

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

//------------------------------------------------------------------------------
// Based on
// https://github.com/boostorg/container_hash/blob/develop/include/boost/container_hash/detail/hash_mix.hpp
//------------------------------------------------------------------------------
template <std::size_t Bits> struct HashMixer;

template <>
struct HashMixer<64>
{
    static uint64_t mix(uint64_t n)
    {
        static constexpr uint64_t m =
            (static_cast<uint64_t>(0xe9846af) << 32) + 0x9b1a615d;

        n ^= n >> 32;
        n *= m;
        n ^= n >> 32;
        n *= m;
        n ^= n >> 28;
        return n;
    }
};

template <>
struct HashMixer<32>
{
    static uint32_t mix(uint32_t x)
    {
        static constexpr uint32_t const m1 = 0x21f0aaad;
        static constexpr uint32_t const m2 = 0x735a2d97;

        x ^= x >> 16;
        x *= m1;
        x ^= x >> 15;
        x *= m2;
        x ^= x >> 15;

        return x;
    }
};

//------------------------------------------------------------------------------
// Based on
// https://github.com/boostorg/container_hash/blob/develop/include/boost/container_hash/hash.hpp
//------------------------------------------------------------------------------
template <typename  T>
void hashCombine(std::size_t& seed, const T& value)
{
    using Mixer = HashMixer<sizeof(std::size_t) * CHAR_BIT>;
    seed = Mixer::mix(seed + 0x9e3779b9 + std::hash<T>{}(value));
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HASHCOMBINE_HPP
