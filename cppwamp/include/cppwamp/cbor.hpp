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
/** CBOR encoder.
    This class uses [jsoncons][1] to serialize CBOR payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam O The output type in which to encode.
    @tparam C The output category type (deduced). */
//------------------------------------------------------------------------------
template <typename O, typename C = OutputCategoryTypeOf<O>>
class CPPWAMP_API BasicCborEncoder
{
public:
    using Output = O;
    using OutputCategory = C;

    /** Default constructor. */
    BasicCborEncoder();

    /** Destructor. */
    ~BasicCborEncoder();

    /** Serializes from the given variant to the given output
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Output& output);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** CBOR encoder specialization for streams.
    This class uses [jsoncons][1] to serialize CBOR payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam O The output type in which to encode. */
//------------------------------------------------------------------------------
template <typename O>
class CPPWAMP_API BasicCborEncoder<O, StreamOutputCategory>
{
public:
    using Output = O;
    using OutputCategory = StreamOutputCategory;

    /** Default constructor. */
    BasicCborEncoder();

    /** Destructor. */
    ~BasicCborEncoder();

    /** Serializes from the given variant to the given output
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Output& output);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// CBOR encoder type that encodes into a std::string. */
using CborStringEncoder = BasicCborEncoder<std::string>;

/// CBOR encoder type that encodes into a MessageBuffer. */
using CborBufferEncoder = BasicCborEncoder<MessageBuffer>;

/// CBOR encoder type that encodes into a std::ostream. */
using CborStreamEncoder = BasicCborEncoder<std::ostream>;


//------------------------------------------------------------------------------
/** CBOR decoder.
    This class uses [jsoncons][1] to deserialize CBOR payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input type from which to decode.
    @tparam C The input category type (deduced). */
//------------------------------------------------------------------------------
template <typename I, typename C = InputCategoryTypeOf<I>>
class CPPWAMP_API BasicCborDecoder
{
public:
    using Input = I;
    using InputCategory = C;

    /** Default constructor. */
    BasicCborDecoder();

    /** Destructor. */
    ~BasicCborDecoder();

    /** Deserializes from the given input to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(const Input& input,
                                             Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** CBOR decoder specialization for stream inputs.
    This class uses [jsoncons][1] to deserialize CBOR payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input stream type from which to decode. */
//------------------------------------------------------------------------------
template <typename I>
class CPPWAMP_API BasicCborDecoder<I, StreamInputCategory>
{
public:
    using Input = I;
    using InputCategory = StreamInputCategory;

    /** Default constructor. */
    BasicCborDecoder();

    /** Destructor. */
    ~BasicCborDecoder();

    /** Deserializes from the given input stream to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(Input& input, Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// CBOR decoder type that decodes from a std::string. */
using CborStringDecoder = BasicCborDecoder<std::string>;

/// CBOR decoder type that decodes from a MessageBuffer. */
using CborBufferDecoder = BasicCborDecoder<MessageBuffer>;

/// CBOR decoder type that decodes from a std::istream. */
using CborStreamDecoder = BasicCborDecoder<std::istream>;

//------------------------------------------------------------------------------
/** CBOR format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct Cbor
{
    template <typename TOutput,
             typename TOutputCategory = OutputCategoryTypeOf<TOutput>>
    using Encoder = BasicCborEncoder<TOutput, TOutputCategory>;

    template <typename TInput,
             typename TInputCategory = InputCategoryTypeOf<TInput>>
    using Decoder = BasicCborDecoder<TInput, TInputCategory>;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::cbor();}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/cbor.ipp"
#endif

#endif // CPPWAMP_CBOR_HPP
