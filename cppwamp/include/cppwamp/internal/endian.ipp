/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "endian.hpp"
#include <boost/endian/conversion.hpp>


namespace wamp
{

namespace internal
{

namespace endian
{

inline uint32_t nativeToBig32(uint32_t native)
{
    return boost::endian::native_to_big(native);
}

inline uint32_t bigToNative32(uint32_t big)
{
    return boost::endian::big_to_native(big);
}

} // namespace endian

} // namespace internal

} // namespace wamp
