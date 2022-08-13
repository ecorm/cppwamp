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
/** JSON encoder.
    This class uses [jsoncons][1] to serialize JSON payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam O The output type in which to encode.
    @tparam C The output category type (deduced). */
//------------------------------------------------------------------------------
template <typename O, typename C = OutputCategoryTypeOf<O>>
class CPPWAMP_API BasicJsonEncoder
{
public:
    using Output = O;
    using OutputCategory = C;

    /** Default constructor. */
    BasicJsonEncoder();

    /** Destructor. */
    ~BasicJsonEncoder();

    /** Serializes from the given variant to the given output
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Output& output);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// JSON encoder type that encodes into a std::string. */
using JsonStringEncoder = BasicJsonEncoder<std::string>;

/// JSON encoder type that encodes into a MessageBuffer. */
using JsonBufferEncoder = BasicJsonEncoder<MessageBuffer>;

/// JSON encoder type that encodes into a std::ostream. */
using JsonStreamEncoder = BasicJsonEncoder<std::ostream>;


//------------------------------------------------------------------------------
/** JSON decoder.
    This class uses [jsoncons][1] to deserialize JSON payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input type from which to decode.
    @tparam C The input category type (deduced). */
//------------------------------------------------------------------------------
template <typename I, typename C = InputCategoryTypeOf<I>>
class CPPWAMP_API BasicJsonDecoder
{
public:
    using Input = I;
    using InputCategory = C;

    /** Default constructor. */
    BasicJsonDecoder();

    /** Destructor. */
    ~BasicJsonDecoder();

    /** Deserializes from the given input to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(const Input& input,
                                             Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** JSON decoder specialization for stream inputs.
    This class uses [jsoncons][1] to deserialize JSON payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input stream type from which to decode. */
//------------------------------------------------------------------------------
template <typename I>
class CPPWAMP_API BasicJsonDecoder<I, StreamInputCategory>
{
public:
    using Input = I;
    using InputCategory = StreamInputCategory;

    /** Default constructor. */
    BasicJsonDecoder();

    /** Destructor. */
    ~BasicJsonDecoder();

    /** Deserializes from the given input stream to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(Input& input, Variant& variant);

private:
    class Impl;
    Impl* impl_;
};

/// JSON decoder type that decodes from a std::string. */
using JsonStringDecoder = BasicJsonDecoder<std::string>;

/// JSON decoder type that decodes from a MessageBuffer. */
using JsonBufferDecoder = BasicJsonDecoder<MessageBuffer>;

/// JSON decoder type that decodes from a std::ostream. */
using JsonStreamDecoder = BasicJsonDecoder<std::istream>;


//------------------------------------------------------------------------------
/** JSON format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct Json
{
    template <typename TOutput,
              typename TOutputCategory = OutputCategoryTypeOf<TOutput>>
    using Encoder = BasicJsonEncoder<TOutput, TOutputCategory>;

    template <typename TInput,
              typename TInputCategory = InputCategoryTypeOf<TInput>>
    using Decoder = BasicJsonDecoder<TInput, TInputCategory>;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::json();}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
    #include "internal/json.ipp"
#endif

#endif // CPPWAMP_JSON_HPP
