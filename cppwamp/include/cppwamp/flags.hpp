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

#include <bitset>
#include <climits>
#include <initializer_list>
#include "config.hpp"
#include "tagtypes.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Enables binary bitwise operators on flag enumerators.
    @see Flags

    @par Example Usage
    ```
    enum class Topping
    {
        fudge     = flag_bit(0), // 00000001 in binary
        sprinkles = flag_bit(1), // 00000010 in binary
        peanuts   = flag_bit(2)  // 00000100 in binary
    };

    namespace wamp
    {
        struct is_flag<Topping> : std::true_type {};
    }

    auto tops = Topping::fudge |
                Topping::peanuts; // Set fudge and peanuts flags

    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 1
    std::cout << tops.test(Topping::sprinkles) << "\n"; // Prints 0
    std::cout << tops.test(Topping::peanuts) << "\n";   // Prints 1
    ``` */
//------------------------------------------------------------------------------
template <typename E, typename Enabled = void>
struct is_flag : FalseType {};

//------------------------------------------------------------------------------
/** Metafunction that determines if the given enumeration type is a flag.
    @relates Flags */
//------------------------------------------------------------------------------
template <typename E>
static constexpr bool isFlag() noexcept {return is_flag<E>::value;}


//------------------------------------------------------------------------------
/** Wrapper around an enumeration where its enumerators are intended to be
    ORed together as bit flags.

    The is_flag trait can be specialized for enumerations types to enable
    binary bitwise operators between them.

    @par Example Usage
    ```
    enum class Topping
    {
        fudge     = flag_bit(0), // 00000001 in binary
        sprinkles = flag_bit(1), // 00000010 in binary
        peanuts   = flag_bit(2)  // 00000100 in binary
    };

    Flags<Topping> tops;
    tops |= Topping::fudge;   // Set fudge flag
    tops |= Topping::peanuts; // Set peanuts flag

    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 1
    std::cout << tops.test(Topping::sprinkles) << "\n"; // Prints 0
    std::cout << tops.test(Topping::peanuts) << "\n";   // Prints 1

    tops.clear(Topping::fudge);
    std::cout << tops.test(Topping::fudge) << "\n";     // Prints 0
    ```
    @see is_flag
    @see std::hash<wamp::Flags<E>> */
//------------------------------------------------------------------------------
template <typename E>
class Flags
{
public:
    /// The enumeration type used as flags.
    using flag_type = E;

    /// Unsigned integer type used to store the flags.
    using integer_type = typename std::make_unsigned<
        typename std::underlying_type<E>::type>::type;

    /// std::bitset type capable of storing the flags.
    using bitset_type = std::bitset<sizeof(integer_type) * CHAR_BIT>;

    /** Default constructor which clears all flags. */
    constexpr Flags() noexcept : n_(0) {}

    /** Converting constructor taking an initializer list of enumerators. */
    CPPWAMP_CONSTEXPR14
    Flags(std::initializer_list<flag_type> list) noexcept
        : n_(0)
    {
        for (auto e: list)
            n_ |= static_cast<integer_type>(e);
    }

    /** Converting constructor taking a single enumerator. */
    constexpr Flags(flag_type e) noexcept // NOLINT(google-explicit-constructor)
        : n_(static_cast<integer_type>(e)) {}

    /** Constructor taking a std::bitset. */
    explicit
#if defined(__cpp_lib_constexpr_bitset) || defined(CPPWAMP_FOR_DOXYGEN)
    constexpr
#endif
    Flags(bitset_type b) noexcept : n_(b.to_ullong()) {}

    /** Constructor taking a raw integer. */
    constexpr Flags(in_place_t, integer_type n) noexcept : n_(n) {}

    /** @name Element access
        @{ */

    /** Determines if the given flag is currently set. */
    constexpr bool test(flag_type flag) const noexcept
    {
        return all_of(flag);
    }

    /** Determines if all of the given flags are currently set. */
    constexpr bool all_of(Flags flags) const noexcept
    {
        return (n_ & flags.n_) == flags.n_;
    }

    /** Determines if any of the given flags are currently set. */
    constexpr bool any_of(Flags flags) const noexcept
    {
        return (n_ & flags.n_) != 0;
    }

    /** Determines if any flags are set. */
    constexpr bool any() const noexcept {return n_ != 0;}

    /** Determines if all flags are reset. */
    constexpr bool none() const noexcept {return n_ == 0;}
    /// @}

    /** @name Modifiers
        @{ */

    /** Self-assigning bitwise AND. */
    CPPWAMP_CONSTEXPR14 Flags& operator&=(Flags rhs) noexcept
    {
        n_ &= rhs.n_;
        return *this;
    }

    /** Self-assigning bitwise XOR. */
    CPPWAMP_CONSTEXPR14 Flags& operator^=(Flags rhs) noexcept
    {
        return flip(rhs);
    }

    /** Self-assigning bitwise OR. */
    CPPWAMP_CONSTEXPR14 Flags& operator|=(Flags rhs) noexcept
    {
        return set(rhs);
    }

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
    /// @}

    /** @name Conversions
        @{ */

    /** Obtains the integer representation of the flags. */
    constexpr integer_type to_integer() const noexcept {return n_;}

    /** Obtains a std::bitfield containing the flags. */
#if defined(__cpp_lib_constexpr_bitset) || defined(CPPWAMP_FOR_DOXYGEN)
    constexpr
#endif
    bitset_type to_bitset() const noexcept {return bitset_type(n_);}
    /// @}

private:
    integer_type n_;
};

/** Equality comparison. @relates Flags */
template <typename E>
constexpr bool operator==(Flags<E> lhs, Flags<E> rhs) noexcept
{
    return lhs.to_integer() == rhs.to_integer();
}

/** Equality comparison. @relates Flags */
template <typename E>
constexpr bool operator==(Flags<E> lhs, E rhs) noexcept
{
    return lhs == Flags<E>(rhs);
}

/** Equality comparison. @relates Flags */
template <typename E>
constexpr bool operator==(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) == rhs;
}

/** Inequality comparison. @relates Flags */
template <typename E>
constexpr bool operator!=(Flags<E> lhs, Flags<E> rhs) noexcept
{
    return lhs.to_integer() != rhs.to_integer();
}

/** Inequality comparison. @relates Flags */
template <typename E>
constexpr bool operator!=(Flags<E> lhs, E rhs) noexcept
{
    return lhs != Flags<E>(rhs);
}

/** Inequality comparison. @relates Flags */
template <typename E>
constexpr bool operator!=(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) != rhs;
}

/** Bitwise AND. @relates Flags */
template <typename E>
constexpr Flags<E> operator&(Flags<E> lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(in_place, lhs.to_integer() & rhs.to_integer());
}

/** Bitwise AND. @relates Flags */
template <typename E>
constexpr Flags<E> operator&(Flags<E> lhs, E rhs) noexcept
{
    return lhs & Flags<E>(rhs);
}

/** Bitwise AND. @relates Flags */
template <typename E>
constexpr Flags<E> operator&(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) & rhs;
}

/** Bitwise AND. @relates Flags */
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator&(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) & Flags<E>(rhs);
}

/** Bitwise OR. @relates Flags */
template <typename E>
constexpr Flags<E> operator|(Flags<E> lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(in_place, lhs.to_integer() | rhs.to_integer());
}

/** Bitwise OR. @relates Flags */
template <typename E>
constexpr Flags<E> operator|(Flags<E> lhs, E rhs) noexcept
{
    return lhs | Flags<E>(rhs);
}

/** Bitwise OR. @relates Flags */
template <typename E>
constexpr Flags<E> operator|(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) | rhs;
}

/** Bitwise OR. @relates Flags */
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator|(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) | Flags<E>(rhs);
}

/** Bitwise XOR. @relates Flags */
template <typename E>
constexpr Flags<E> operator^(Flags<E> lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(in_place, lhs.to_integer() ^ rhs.to_integer());
}

/** Bitwise XOR. @relates Flags */
template <typename E>
constexpr Flags<E> operator^(Flags<E> lhs, E rhs) noexcept
{
    return lhs ^ Flags<E>(rhs);
}

/** Bitwise XOR. @relates Flags */
template <typename E>
constexpr Flags<E> operator^(E lhs, Flags<E> rhs) noexcept
{
    return Flags<E>(lhs) ^ rhs;
}

/** Bitwise XOR. @relates Flags */
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator^(E lhs, E rhs) noexcept
{
    return Flags<E>(lhs) ^ Flags<E>(rhs);
}

/** Bitwise inversion. @relates Flags */
template <typename E>
constexpr Flags<E> operator~(Flags<E> f) noexcept
{
    return Flags<E>(in_place, ~(f.to_integer()));
}

/** Bitwise inversion. @relates Flags */
template <typename E, CPPWAMP_NEEDS(isFlag<E>()) = 0>
constexpr Flags<E> operator~(E enumerator) noexcept
{
    return ~Flags<E>(enumerator);
}

/** Convenience function for representing a bit in a flags enumeration.
    @relates Flags */
static constexpr uintmax_t flag_bit(unsigned pos) {return uintmax_t{1u} << pos;}

} // namespace wamp


namespace std
{

//------------------------------------------------------------------------------
/** Hash support for wamp::Flags.
    @relates wamp::Flags */
//------------------------------------------------------------------------------
template <typename E>
struct hash<wamp::Flags<E>>
{
private:
    using Bitset = typename wamp::Flags<E>::bitset_type;

public:
    using argument_type = wamp::Flags<E>;
    using result_type = decltype(hash<Bitset>{}(std::declval<Bitset>()));

    result_type operator()(argument_type key) const
    {
        return hash<Bitset>{}(key.to_bitset());
    }
};

} // namespace std

#endif // CPPWAMP_FLAGS_HPP
