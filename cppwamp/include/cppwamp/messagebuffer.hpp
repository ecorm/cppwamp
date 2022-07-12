/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MESSAGEBUFFER_HPP
#define CPPWAMP_MESSAGEBUFFER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the MessageBuffer definition. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <vector>

namespace wamp
{

//------------------------------------------------------------------------------
/** Container type used for encoded WAMP messages that are sent/received
    over a transport. */
//------------------------------------------------------------------------------
using MessageBuffer = std::vector<uint8_t>;


} // namespace wamp

#endif // CPPWAMP_MESSAGEBUFFER_HPP
