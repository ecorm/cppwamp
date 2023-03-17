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

#include "traits.hpp"
#include <initializer_list>

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

    /** Constructs a Flags object from a raw integer. */
    static Flags from(Integer n) {return Flags(static_cast<Enum>(n));}

    /** Default constructor which clears all flags. */
    Flags() : n_(0) {}

    /** Converting constructor taking a single enumerator. */
    Flags(Enum e) : n_(static_cast<Integer>(e)) {}

    /** Converting constructor taking an initializer list of enumerators. */
    Flags(std::initializer_list<Enum> list)
        : n_(0)
    {
        for (auto e: list)
            n_ |= static_cast<Integer>(e);
    }

    /** Obtains the integer value of the flags. */
    Integer value() const {return n_;}

    /** Bool conversion operator that returns false iff all flags are reset. */
    explicit operator bool() const {return !none();}

    /** Equality comparison. */
    bool operator==(Flags rhs) const {return n_ == rhs.n_;}

    /** Returns true if all of the given flags are currently set. */
    bool test(Flags flags) const {return (n_ & flags.n_) == flags.n_;}

    /** Returns true if any of the given flags are currently set. */
    bool any(Flags flags) const {return (n_ & flags.n_) != 0;}

    /** Returns true iff all flags are reset. */
    bool none() const {return n_ == 0;}

    /** Resets all flags. */
    Flags& clear() {n_ = 0; return *this;}

    /** Sets the given flags. */
    Flags& set(Flags flags) {n_ |= flags.n_; return *this;}

    /** Sets the given flag bits to the given value. */
    Flags& set(Flags flags, bool value)
    {
        n_ = value ? (n_ | flags.n_) : (n_ & ~flags.n_);
        return *this;
    }

    /** Resets the given flags. */
    Flags& reset(Flags flags) {n_ &= ~flags.n_; return *this;}

    /** Toggles the given flags. */
    Flags& flip(Flags flags) {n_ ^= flags.n_; return *this;}

    /** Performs a bitwise AND with the given flags and self-assigns
        the result. */
    Flags& operator&=(Flags rhs) {n_ &= rhs.n_; return *this;}

    /** Performs a bitwise AND with the given flags. */
    Flags operator&(Flags rhs) const {return rhs &= *this;}

    /** Performs a bitwise OR with the given flags and self-assigns
        the result. */
    Flags& operator|=(Flags rhs) {return set(rhs);}

    /** Performs a bitwise OR with the given flags. */
    Flags operator|(Flags rhs) const {return rhs.set(*this);}

    /** Performs a bitwise XOR with the given flags and self-assigns
        the result. */
    Flags& operator^=(Flags rhs) {return flip(rhs);}

    /** Performs a bitwise XOR with the given flags. */
    Flags operator^(Flags rhs) {return rhs.flip(*this);}

    /** Obtains the bitwise inversion of the current flags. */
    Flags operator~() const {return Flags::from(~n_);}

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
/** Bitwise AND of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(IsFlag<E>::value) = 0>
Flags<E> operator&(E lhs, E rhs) {return Flags<E>(lhs) & rhs;}

//------------------------------------------------------------------------------
/** Bitwise OR of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(IsFlag<E>::value) = 0>
Flags<E> operator|(E lhs, E rhs) {return Flags<E>(lhs) | rhs;}

//------------------------------------------------------------------------------
/** Bitwise XOR of two flag enumerators.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(IsFlag<E>::value) = 0>
Flags<E> operator^(E lhs, E rhs) {return Flags<E>(lhs) ^ rhs;}

//------------------------------------------------------------------------------
/** Bitwise inversion of a flag enumerator.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E, CPPWAMP_NEEDS(IsFlag<E>::value) = 0>
Flags<E> operator~(E enumerator) {return ~Flags<E>(enumerator);}

} // namespace wamp

#endif // CPPWAMP_FLAGS_HPP
