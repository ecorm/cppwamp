/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_JSON_HPP
#define CPPWAMP_JSON_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the JSON codec. */
//------------------------------------------------------------------------------

#include <istream>
#include <memory>
#include <ostream>
#include <string>
#include "api.hpp"
#include "codec.hpp"
#include "config.hpp"
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** JSON format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Json
{
    /** Default contructor. */
    constexpr Json() = default;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::json();}
};

/** Instance of the Json tag. */
constexpr CPPWAMP_INLINE Json json;

//------------------------------------------------------------------------------
/** JSON encoder.
    This class uses [jsoncons][1] to serialize JSON payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam TSink The output sink type in which to encode. */
//------------------------------------------------------------------------------
template <typename TSink>
class CPPWAMP_API SinkEncoder<Json, TSink>
{
public:
    using Sink = TSink;
    using Output = typename Sink::Output;

    /** Default constructor. */
    SinkEncoder();

    /** Destructor. */
    ~SinkEncoder();

    /** Serializes from the given variant to the given output sink
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Sink sink);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Yields the JSON encoder type for the given output sink type.
template <typename TSink>
using JsonEncoder = SinkEncoder<Json, TSink>;

/// JSON encoder type that encodes into a std::string. */
using JsonStringEncoder = JsonEncoder<StringSink>;

/// JSON encoder type that encodes into a MessageBuffer. */
using JsonBufferEncoder = JsonEncoder<BufferSink>;

/// JSON encoder type that encodes into a std::ostream. */
using JsonStreamEncoder = JsonEncoder<StreamSink>;


//------------------------------------------------------------------------------
/** JSON decoder.
    This class uses [jsoncons][1] to deserialize JSON payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam TSource The input source type from which to decode. */
//------------------------------------------------------------------------------
template <typename TSource>
class CPPWAMP_API SourceDecoder<Json, TSource>
{
public:
    using Source = TSource;
    using Input = typename Source::Input;

    /** Default constructor. */
    SourceDecoder();

    /** Destructor. */
    ~SourceDecoder();

    /** Deserializes from the given input source to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(Source source, Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Yields the JSON decoder type for the given input source type.
template <typename TSource>
using JsonDecoder = SourceDecoder<Json, TSource>;

/// JSON decoder type that decodes from a std::string. */
using JsonStringDecoder = JsonDecoder<StringSource>;

/// JSON decoder type that decodes from a MessageBuffer. */
using JsonBufferDecoder = JsonDecoder<BufferSource>;

/// JSON decoder type that decodes from a std::ostream. */
using JsonStreamDecoder = JsonDecoder<StreamSource>;


} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
    #include "internal/json.inl.hpp"
#endif

#endif // CPPWAMP_JSON_HPP
