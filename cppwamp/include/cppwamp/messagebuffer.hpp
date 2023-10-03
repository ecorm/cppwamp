/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
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
// TODO: Consider using std::pmr::vector so that user may customize allocator
//------------------------------------------------------------------------------
using MessageBuffer = std::vector<uint8_t>;


} // namespace wamp

#endif // CPPWAMP_MESSAGEBUFFER_HPP
