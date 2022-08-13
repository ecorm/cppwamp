/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ENDIAN_HPP
#define CPPWAMP_ENDIAN_HPP

#include <cstdint>

#if defined(__has_include) && __has_include(<bit>)
#include <bit>
#ifdef __cpp_lib_endian
#define CPPWAMP_HAS_STD_ENDIAN
#endif
#endif

namespace wamp
{

namespace internal
{

namespace endian
{

inline uint32_t flip(uint32_t n)
{
    // This usually optimizes to a single byte swap instruction.
    return ((n & 0xFF000000u) >> 24u) | ((n & 0x00FF0000u) >> 8u) |
           ((n & 0x0000FF00u) << 8u) | ((n & 0x0000000FF) << 24u);
}

constexpr bool nativeIsLittle()
{
#ifdef CPPWAMP_HAS_STD_ENDIAN
    return (std::endian::native == std::endian::little);
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
#elif defined(_WIN32)
    return true;
#elif defined(CPPWAMP_ASSUME_LITTLE_ENDIAN)
    return true;
#elif defined(CPPWAMP_ASSUME_BIG_ENDIAN)
    return false;
#else
#warning Cannot detect endianness; assuming little endian
#warning Please define either CPPWAMP_ASSUME_LITTLE_ENDIAN or CPPWAMP_ASSUME_BIG_ENDIAN
    return little_endian;
#endif
}

inline uint32_t nativeToBig32(uint32_t native)
{
    return nativeIsLittle() ? flip(native) : native;
}

inline uint32_t bigToNative32(uint32_t big)
{
    return nativeIsLittle() ? flip(big) : big;
}


} // namespace endian

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_ENDIAN_HPP
