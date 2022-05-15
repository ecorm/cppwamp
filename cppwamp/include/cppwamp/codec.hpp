/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CODEC_HPP
#define CPPWAMP_CODEC_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains essential definitions for wamp::Variant codecs. */
//------------------------------------------------------------------------------

#include <stdexcept>
#include <string>
#include "api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** IDs used by rawsocket transports to negotiate the serializer.
    As described in section Advanced Profile / Other Advanced Features /
    Alternative Transports / RawSocket Transport of the WAMP spec. */
//------------------------------------------------------------------------------
struct CPPWAMP_API KnownCodecIds
{
    static constexpr int json() {return 1;}
    static constexpr int msgpack() {return 2;}
};

namespace error
{

//------------------------------------------------------------------------------
/** Exception type thrown when codec deserialization fails. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Decode: public std::runtime_error
{
    explicit Decode(const std::string& msg)
        : std::runtime_error("wamp::error::Decode: " + msg)
    {}
};

} // namespace error

} // namespace wamp

#endif // CPPWAMP_CODEC_HPP
