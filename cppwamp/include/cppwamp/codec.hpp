/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CODEC_HPP
#define CPPWAMP_CODEC_HPP

//------------------------------------------------------------------------------
/** @file
    Contains essential definitions for wamp::Variant serializers. */
//------------------------------------------------------------------------------

#include <stdexcept>
#include <string>

namespace wamp
{

namespace error
{

//------------------------------------------------------------------------------
/** Exception type thrown when codec deserialization fails. */
//------------------------------------------------------------------------------
struct Decode: public std::runtime_error
{
    explicit Decode(const std::string& msg)
        : std::runtime_error("wamp::error::Decode: " + msg)
    {}
};

} // namespace error

} // namespace wamp

#endif // CPPWAMP_CODEC_HPP
