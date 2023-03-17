/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_FLAGS_HPP
#define CPPWAMP_FLAGS_HPP

//------------------------------------------------------------------------------
/** @file
    Provides facilities for using enumerators as bit flags. */
//------------------------------------------------------------------------------

#include "config.hpp"
#include "tagtypes.hpp"
#include "traits.hpp"
#include <initializer_list>

// TODO: Constexpr

namespace wamp
{

//------------------------------------------------------------------------------
/** Wrapper around an enumeration where its enumerators are intended to be
    ORed together as bit flags.

    @par Example Usage
    ```c++
    enum class Topping
    {
        fudge     = 0x01, // 00000001 in binary
        sprinkles = 0x02, // 00000010 in binary
        peanuts   = 0x04  // 00000100 in binary
    };

    Flags<Topping> tops;
    tops |= Topping::fudge;   // Set fudge flag
    tops |= Topping::peanuts; // Set peanuts flag

    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 1
    std::cout << tops.test(Topping::sprinkles) << "\n"; // Prints 0
    std::cout << tops.test(Topping::peanuts) << "\n";   // Prints 1

    tops.clear(Topping::fudge);
    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 0
    ``` */
//------------------------------------------------------------------------------
template <typename E>
class Flags
{
public:
    /// The enumeration type being wrapped.
    using Enum = E;

    /// The underlying integer type of the wrapped enum.
    using Integer = typename std::underlying_type<E>::type;

    /** Default constructor which clears all flags. */
    constexpr Flags() noexcept : n_(0) {}

    /** Converting constructor taking a single enumerator. */
    constexpr Flags(Enum e) noexcept : n_(static_cast<Integer>(e)) {}

    /** Converting constructor taking an initializer list of enumerators. */
    CPPWAMP_CONSTEXPR14 Flags(std::initializer_list<Enum> list) noexcept
        : n_(0)
    {
        for (auto e: list)
            n_ |= static_cast<Integer>(e);
    }

    /** Constructor taking a raw integer. */
    constexpr Flags(in_place_t, Integer n) noexcept : n_(n) {}

    /** Obtains the integer value of the flags. */
    constexpr Integer value() const noexcept {return n_;}

    /** Bool conversion operator that returns false iff all flags are reset. */
    constexpr explicit operator bool() const noexcept {return !none();}

    /** Equality comparison. */
    constexpr bool operator==(Flags rhs) const noexcept {return n_ == rhs.n_;}

    /** Inequality comparison. */
    constexpr bool operator!=(Flags rhs) const noexcept {return n_ != rhs.n_;}

    /** Determines if all of the given flags are currently set. */
    constexpr bool test(Flags flags) const noexcept
    {
        return (n_ & flags.n_) == flags.n_;
    }

    /** Determines if any of the given flags are currently set. */
    constexpr bool any(Flags flags) const noexcept
    {
        return (n_ & flags.n_) != 0;
    }

    /** Determines if all flags are reset. */
    constexpr bool none() const noexcept {return n_ == 0;}

    /** Sets the given flags. */
    CPPWAMP_CONSTEXPR14 Flags& set(Flags flags) noexcept
    {
        n_ |= flags.n_;
        return *this;
    }

    /** Sets the given flag bits to the given value. */
    CPPWAMP_CONSTEXPR14 Flags& set(Flags flags, bool value) noexcept
    {
        n_ = value ? (n_ | flags.n_) : (n_ & ~flags.n_);
        return *this;
    }

    /** Resets all flags. */
    CPPWAMP_CONSTEXPR14 Flags& reset() noexcept {n_ = 0; return *this;}

    /** Resets the given flags. */
    CPPWAMP_CONSTEXPR14 Flags& reset(Flags flags) noexcept
    {
        n_ &= ~flags.n_;
        return *this;
    }

    /** Toggles the given flags. */
    CPPWAMP_CONSTEXPR14 Flags& flip(Flags flags) noexcept
    {
        n_ ^= flags.n_;
        return *this;
    }

    /** Performs a bitwise AND with the given flags and self-assigns
        the result. */
    CPPWAMP_CONSTEXPR14 Flags& operator&=(Flags rhs) noexcept
    {
        n_ &= rhs.n_;
        return *this;
    }

    /** Performs a bitwise AND with the given flags. */
    constexpr Flags operator&(Flags rhs) const noexcept
    {
        return Flags(in_place, n_ & rhs.n_);
    }

    /** Performs a bitwise OR with the given flags and self-assigns
        the result. */
    CPPWAMP_CONSTEXPR14 Flags& operator|=(Flags rhs) noexcept {return set(rhs);}

    /** Performs a bitwise OR with the given flags. */
    constexpr Flags operator|(Flags rhs) const noexcept
    {
        return Flags(in_place, n_ | rhs.n_);
    }

    /** Performs a bitwise XOR with the given flags and self-assigns
        the result. */
    CPPWAMP_CONSTEXPR14 Flags& operator^=(Flags rhs) noexcept
    {
        return flip(rhs);
    }

    /** Performs a bitwise XOR with the given flags. */
    constexpr Flags operator^(Flags rhs) const noexcept
    {
        return Flags(in_place, n_ ^ rhs.n_);
    }

    /** Obtains the bitwise inversion of the current flags. */
    constexpr Flags operator~() const noexcept {return Flags(in_place, ~n_);}

private:
    // Store the flag bits in an integer to avoid having to perform
    // type casts everywhere.
    Integer n_;
};


//------------------------------------------------------------------------------
/** Enables bitwise operatiors that automatically convert an enumerator `E` to
    a Flags<E>.

    @par Example Usage
    ```c++
    enum class Topping
    {
        fudge     = 0x01, // 00000001 in binary
        sprinkles = 0x02, // 00000010 in binary
        peanuts   = 0x04  // 00000100 in binary
    };

    namespace wamp
    {
        struct IsFlag<Topping> : std::true_type {};
    }

    auto tops = Topping::fudge |
                Topping::peanuts; // Set fudge and peanuts flags

    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 1
    std::cout << tops.test(Topping::sprinkles) << "\n"; // Prints 0
    std::cout << tops.test(Topping::peanuts) << "\n";   // Prints 1
    ``` */
//------------------------------------------------------------------------------
template <typename E, typename Enabled = void>
struct IsFlag : FalseType {};

//------------------------------------------------------------------------------
/** Metafunction that determines if the given enumeration type is a flag. */
template <typename E>
static constexpr bool isFlag() noexcept {return IsFlag<E>::value;}


//------------------------------------------------------------------------------
/** Bitwise AND of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator&(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) & rhs;
}

//------------------------------------------------------------------------------
/** Bitwise AND of an enumerator and some flags.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
constexpr Flags<E> operator&(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) & rhs;
}

//------------------------------------------------------------------------------
/** Bitwise OR of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator|(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) | rhs;
}

//------------------------------------------------------------------------------
/** Bitwise OR of an enumerator and some flags.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
constexpr Flags<E> operator|(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) | rhs;
}

//------------------------------------------------------------------------------
/** Bitwise XOR of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator^(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) ^ rhs;
}

//------------------------------------------------------------------------------
/** Bitwise XOR of an enumerator and some flags.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
constexpr Flags<E> operator^(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) ^ rhs;
}

//------------------------------------------------------------------------------
/** Bitwise inversion of a flag enumerator.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator~(E enumerator) noexcept
{
    return ~Flags<E>(enumerator);
}

//------------------------------------------------------------------------------
/** Equality comparison of an enumerator and some flags.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
constexpr bool operator==(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) == rhs;
}

//------------------------------------------------------------------------------
/** Inequality comparison of an enumerator and some flags.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
constexpr bool operator!=(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) != rhs;
}

} // namespace wamp

#endif // CPPWAMP_FLAGS_HPP
