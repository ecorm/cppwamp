/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CODECID_HPP
#define CPPWAMP_CODECID_HPP

//------------------------------------------------------------------------------
/** @file
    Contains essential definitions for wamp::Variant serializers. */
//------------------------------------------------------------------------------

#include <set>
#include <stdexcept>

namespace wamp
{

//------------------------------------------------------------------------------
/** Integer type used to identify a codec. */
//------------------------------------------------------------------------------
enum class CodecId
{
    json,   ///< JSON serializer
    msgpack ///< Msgpack serializer
};

//------------------------------------------------------------------------------
/** Collection of supported serializers IDs. */
//------------------------------------------------------------------------------
using CodecIds = std::set<CodecId>;

//------------------------------------------------------------------------------
/** Empty default implementation of the Codec template.
    Codecs are static-only classes that can perform serialization from, and
    deserialization to Variant objects. Codec is a template class that is
    specialized for each @ref CodecId enumerator. Specializations must conform
    to the @ref Serializer concept.
    @see Codec<CodecId::json>
    @see Codec<CodecId::msgpack> */
//------------------------------------------------------------------------------
template <CodecId id> class Codec
{
    // Required for specializations:

    // Deserializes from the given transport buffer to the given variant.
    // template <typename TBuffer>
    // static void decodeBuffer(const TBuffer& from, Variant& to);

    // Serializes from the given variant to the given transport buffer.
    // template <typename TBuffer>
    // static void encodeBuffer(const Variant& from, TBuffer& to);
};


namespace error
{

//------------------------------------------------------------------------------
/** Exception type thrown when Codec deserialization fails. */
//------------------------------------------------------------------------------
struct Decode: public std::runtime_error
{
    explicit Decode(const std::string& msg)
        : std::runtime_error("wamp::error::Decode: " + msg)
    {}
};

} // namespace error

} // namespace wamp

#endif // CPPWAMP_CODECID_HPP
