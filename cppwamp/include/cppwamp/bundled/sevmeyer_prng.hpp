// prng 0.1.0
// https://github.com/sevmeyer/prng
//
// A simple and efficient pseudorandom number generator for C++11,
// based on the excellent sfc64 (0.94) by Chris Doty-Humphrey.
// http://pracrand.sourceforge.net
//
// NOT SUITABLE FOR SECURITY PURPOSES.
//
// Copyright 2019 Severin Meyer
// Distributed under the Boost Software License 1.0
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.


#ifndef CPPWAMP_BUNDLED_PRNG_PRNG_HPP_INCLUDED
#define CPPWAMP_BUNDLED_PRNG_PRNG_HPP_INCLUDED


#include <cstdint>     // For uint64_t
#include <chrono>      // For high_resolution_clock
#include <functional>  // For hash
#include <limits>      // For numeric_limits
#include <random>      // For random_device
#include <type_traits> // For enable_if, is_floating_point, is_integral


namespace wamp {
namespace bundled {
namespace prng {


using std::uint64_t;


class Generator
{
    public:

        using result_type = uint64_t;

        // Convenience SFINAE types

        template<typename T>
        using IsInt = typename std::enable_if<
            std::is_integral<T>::value, bool>::type;

        template<typename T>
        using IsFloat = typename std::enable_if<
            std::is_floating_point<T>::value, bool>::type;


        // Constructors
        // ------------

        // Tries to initialize the state with system entropy.
        Generator()
        {
            // http://www.pcg-random.org/posts/simple-portable-cpp-seed-entropy.html
            // Collect system entropy. This could be expensive, so
            // it is done only once. This may not be truly random.
            static uint64_t entropy{getSystemEntropy()};

            // Ensure that each instance uses a different seed.
            // Constant from https://en.wikipedia.org/wiki/RC5
            entropy += UINT64_C(0x9e3779b97f4a7c15);
            c_ = entropy;

            // Add possible entropy from the current time.
            using Clock = std::chrono::high_resolution_clock;
            b_ = static_cast<uint64_t>(Clock::now().time_since_epoch().count());

            // Add possible entropy from the address of this object.
            // This is most effective when ASLR is active.
            a_ = static_cast<uint64_t>(std::hash<decltype(this)>{}(this));

            warmup(18);
        }

        // Initializes the state with a custom seed.
        explicit Generator(uint64_t seed) :
            a_{seed},
            b_{seed},
            c_{seed}
        {
            warmup(12);
        }


        // Standard interface
        // ------------------

        static constexpr uint64_t min()
        {
            return std::numeric_limits<uint64_t>::min();
        }

        static constexpr uint64_t max()
        {
            return std::numeric_limits<uint64_t>::max();
        }

        uint64_t operator()()
        {
            const uint64_t tmp{a_ + b_ + counter_++};
            a_ = b_  ^ (b_ >> 11);
            b_ = c_  + (c_ <<  3);
            c_ = tmp + (c_ << 24 | c_ >> 40);
            return tmp;
        }


        // Distributions
        // -------------

        // Returns uniformly distributed integer in [0, bound).
        // A bound outside of [0, 2^64) will produce nonsense.
        // To ensure an efficient and consistent performance,
        // this function does not perform rejection sampling.
        // As a result, it has a tiny bias of bound / 2^64,
        // which should be irrelevant for any bound below 2^32.
        template<typename T, IsInt<T> = true>
        T uniform(T bound)
        {
            // http://pcg-random.org/posts/bounded-rands.html
            const uint64_t range{static_cast<uint64_t>(bound)};
            const uint64_t random{operator()()};
            const uint64_t r0{random & UINT64_C(0xffffffff)};
            const uint64_t r1{random >> 32};

            // range * (random / 2^64)
            // = (range * random) >> 64
            // = (range * (r1*2^32 + r0)) >> 64
            // = ((range*r1 << 32) + range*r0) >> 64
            // = (range*r1 + (range*r0 >> 32)) >> 32
            return static_cast<T>((range*r1 + (range*r0 >> 32)) >> 32);
        }

        // Returns uniformly distributed floating-point in [0, bound).
        // The number of random bits is limited to min(mantissa, 63).
        template<typename T, IsFloat<T> = true>
        T uniform(T bound)
        {
            // http://prng.di.unimi.it
            constexpr int mantissa{min(std::numeric_limits<T>::digits, 63)};
            constexpr T epsilon{static_cast<T>(1) / (UINT64_C(1) << mantissa)};

            const uint64_t random{operator()()};
            return static_cast<T>(random >> (64-mantissa)) * epsilon * bound;
        }

    private:

        uint64_t a_;
        uint64_t b_;
        uint64_t c_;
        uint64_t counter_{1};

        void warmup(int rounds)
        {
            for (int i{0}; i < rounds; i++)
                operator()();
        }

        static uint64_t getSystemEntropy()
        {
            std::random_device device;
            const auto r0{static_cast<uint64_t>(device())};
            const auto r1{static_cast<uint64_t>(device())};
            return (r1 << 32) | (r0 & UINT64_C(0xffffffff));
        }

        // Because C++11 std::min cannot be used for constexpr
        template<typename T>
        static constexpr T min(T a, T b)
        {
            return a < b ? a : b;
        }
};


} // namespace prng
} // namespace bundled
} // namespace wamp

#endif // CPPWAMP_BUNDLED_PRNG_PRNG_HPP_INCLUDED
