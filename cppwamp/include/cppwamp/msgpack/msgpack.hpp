/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MSGPACK_MSGPACK_HPP
#define CPPWAMP_MSGPACK_MSGPACK_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Msgpack codec. */
//------------------------------------------------------------------------------

#include <istream>
#include <ostream>
#include <string>
#include "../codec.hpp"
#include "../variant.hpp"
#include "../msgpack/api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Msgpack codec.
    This static-only class is used to serialize/deserialize between Msgpack
    payloads and Variant objects. This class uses
    [msgpack-c](https://github.com/msgpack/msgpack-c).

    @par Type Requirements
    Meets the requirements of the @ref Codec concept.

    @see Json */
//------------------------------------------------------------------------------
class CPPWAMP_MSGPACK_API Msgpack
{
public:
    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::msgpack();}

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

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
    #include "internal/msgpack.ipp"
#endif

#endif // CPPWAMP_MSGPACK_MSGPACK_HPP
