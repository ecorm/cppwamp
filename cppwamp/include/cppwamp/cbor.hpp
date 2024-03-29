/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CBOR_HPP
#define CPPWAMP_CBOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the CBOR codec. */
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
/** CBOR format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Cbor
{
    /** Default contructor. */
    constexpr Cbor() = default;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::cbor();}
};

/** Instance of the Cbor tag. */
constexpr CPPWAMP_INLINE Cbor cbor;

//------------------------------------------------------------------------------
/** CBOR encoder.
    This class uses [jsoncons][1] to serialize CBOR payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam TSink The output sink type in which to encode. */
//------------------------------------------------------------------------------
template <typename TSink>
class CPPWAMP_API SinkEncoder<Cbor, TSink>
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

/// Yields the CBOR encoder type for the given output sink type.
template <typename TSink>
using CborEncoder = SinkEncoder<Cbor, TSink>;

/// CBOR encoder type that encodes into a std::string. */
using CborStringEncoder = CborEncoder<StringSink>;

/// CBOR encoder type that encodes into a MessageBuffer. */
using CborBufferEncoder = CborEncoder<BufferSink>;

/// CBOR encoder type that encodes into a std::ostream. */
using CborStreamEncoder = CborEncoder<StreamSink>;


//------------------------------------------------------------------------------
/** CBOR decoder.
    This class uses [jsoncons][1] to deserialize CBOR payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam TSource The input source type from which to decode. */
//------------------------------------------------------------------------------
template <typename TSource>
class CPPWAMP_API SourceDecoder<Cbor, TSource>
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

/// Yields the CBOR decoder type for the given input source type.
template <typename TSource>
using CborDecoder = SourceDecoder<Cbor, TSource>;

/// CBOR decoder type that decodes from a std::string. */
using CborStringDecoder = CborDecoder<StringSource>;

/// CBOR decoder type that decodes from a MessageBuffer. */
using CborBufferDecoder = CborDecoder<BufferSource>;

/// CBOR decoder type that decodes from a std::istream. */
using CborStreamDecoder = CborDecoder<StreamSource>;

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/cbor.ipp"
#endif

#endif // CPPWAMP_CBOR_HPP
