/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_JSON_JSON_HPP
#define CPPWAMP_JSON_JSON_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Json codec. */
//------------------------------------------------------------------------------

#include <istream>
#include <ostream>
#include <string>
#include "../codec.hpp"
#include "../variant.hpp"
#include "../json/api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** JSON codec.
    This static-only class is used to serialize/deserialize between JSON
    payloads and Variant objects. This class uses
    [RapidJSON](https://github.com/miloyip/rapidjson).

    @par Type Requirements
    Meets the requirements of the @ref Codec concept.

    @see Msgpack */
//------------------------------------------------------------------------------
class CPPWAMP_JSON_API Json
{
public:
    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::json();}

    /** Deserializes from the given transport buffer to the given variant. */
    template <typename TBuffer>
    static void decodeBuffer(const TBuffer& from, Variant& to);

    /** Deserializes from the given input stream to the given variant. */
    static void decode(std::istream& from, Variant& to);

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
    #include "internal/json.ipp"
#endif

#endif // CPPWAMP_JSON_JSON_HPP
