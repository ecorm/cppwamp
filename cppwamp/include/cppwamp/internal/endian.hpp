/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ENDIAN_HPP
#define CPPWAMP_ENDIAN_HPP

#include <cstdint>


namespace wamp
{

namespace internal
{

namespace endian
{

inline uint32_t nativeToBig32(uint32_t native);

inline uint32_t bigToNative32(uint32_t big);

} // namespace endian

} // namespace internal

} // namespace wamp


#include "endian.ipp"

#endif // CPPWAMP_ENDIAN_HPP
