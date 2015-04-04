/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MSGPACK_HPP
#define CPPWAMP_MSGPACK_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the Msgpack serializer specialization. */
//------------------------------------------------------------------------------

#include <istream>
#include <ostream>
#include <string>
#include "codec.hpp"
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Codec specialization for Msgpack serialization.
    This static-only class is used to serialize/deserialize between Msgpack
    payloads and Variant objects.

    @par Type Requirements
    Meets the requirements of the @ref Serializer concept.

    @note The more readable @ref Msgpack alias should be used instead of
          `Codec<CodecId::msgpack>`, wherever possible.
    @see Codec
    @see Msgpack
    @see Json */
//------------------------------------------------------------------------------
template <>
struct Codec<CodecId::msgpack>
{
    /** Returns the @ref CodecId enumerator associated with this
        Codec specialization. */
    static constexpr CodecId id() {return CodecId::msgpack;}

    /** Deserializes from the given transport buffer to the given variant. */
    template <typename TBuffer>
    static void decodeBuffer(const TBuffer& from, Variant& to);

    // msgpack-c does not support stream-oriented deserialization
    //static void decode(std::istream& from, Variant& to);

    /** Deserializes from the given string to the given variant. */
    static void decode(const std::string& from, Variant& to);

    /** Serializes from the given variant to the given transport buffer. */
    template <typename TBuffer>
    static void encodeBuffer(const Variant& from, TBuffer& to);

    /** Serializes from the given variant to the given output stream. */
    static void encode(const Variant& from, std::ostream& to);

    /** Serializes from the given variant to the given string. */
    static void encode(const Variant& from, std::string& to);
};

//------------------------------------------------------------------------------
/** Convenient shorthand for the Msgpack codec.
    @see Codec<CodecId::msgpack> */
//------------------------------------------------------------------------------
using Msgpack = Codec<CodecId::msgpack>;

} // namespace wamp

#include "internal/msgpack.ipp"

#endif // CPPWAMP_MSGPACK_HPP
