/*------------------------------------------------------------------------------
                    Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MSGPACK_HPP
#define CPPWAMP_MSGPACK_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the Msgpack codec. */
//------------------------------------------------------------------------------

#include <memory>
#include <istream>
#include <ostream>
#include <string>
#include "api.hpp"
#include "codec.hpp"
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Msgpack encoder.
    This class uses [jsoncons][1] to serialize Msgpack payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam O The output type in which to encode.
    @tparam C The output category type (deduced). */
//------------------------------------------------------------------------------
template <typename O, typename C = OutputCategoryTypeOf<O>>
class CPPWAMP_API BasicMsgpackEncoder
{
public:
    using Output = O;
    using OutputCategory = C;

    /** Default constructor. */
    BasicMsgpackEncoder();

    /** Destructor. */
    ~BasicMsgpackEncoder();

    /** Serializes from the given variant to the given output
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Output& output);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Msgpack encoder specialization for streams.
    This class uses [jsoncons][1] to serialize Msgpack payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam O The output type in which to encode. */
//------------------------------------------------------------------------------
template <typename O>
class CPPWAMP_API BasicMsgpackEncoder<O, StreamOutputCategory>
{
public:
    using Output = O;
    using OutputCategory = StreamOutputCategory;

    /** Default constructor. */
    BasicMsgpackEncoder();

    /** Destructor. */
    ~BasicMsgpackEncoder();

    /** Serializes from the given variant to the given output
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Output& output);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Msgpack encoder type that encodes into a std::string. */
using MsgpackStringEncoder = BasicMsgpackEncoder<std::string>;

/// Msgpack encoder type that encodes into a MessageBuffer. */
using MsgpackBufferEncoder = BasicMsgpackEncoder<MessageBuffer>;

/// Msgpack encoder type that encodes into a std::ostream. */
using MsgpackStreamEncoder = BasicMsgpackEncoder<std::ostream>;


//------------------------------------------------------------------------------
/** Decoder specialization for Msgpack.
    This class uses [jsoncons][1] to deserialize Msgpack payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input type from which to decode.
    @tparam C The input category type (deduced). */
//------------------------------------------------------------------------------
template <typename I, typename C = InputCategoryTypeOf<I>>
class CPPWAMP_API BasicMsgpackDecoder
{
public:
    using Input = I;
    using InputCategory = C;

    /** Default constructor. */
    BasicMsgpackDecoder();

    /** Destructor. */
    ~BasicMsgpackDecoder();

    /** Deserializes from the given input to the given variant. */
    void decode(const Input& input, Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Decoder specialization for Msgpack and stream inputs.
    This class uses [jsoncons][1] to deserialize Msgpack payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam I The input stream type from which to decode. */
//------------------------------------------------------------------------------
template <typename I>
class CPPWAMP_API BasicMsgpackDecoder<I, StreamInputCategory>
{
public:
    using Input = I;
    using InputCategory = StreamInputCategory;

    /** Default constructor. */
    BasicMsgpackDecoder();

    /** Destructor. */
    ~BasicMsgpackDecoder();

    /** Deserializes from the given input stream to the given variant. */
    void decode(Input& input, Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Msgpack decoder type that decodes from a std::string. */
using MsgpackStringDecoder = BasicMsgpackDecoder<std::string>;

/// Msgpack decoder type that decodes from a MessageBuffer. */
using MsgpackBufferDecoder = BasicMsgpackDecoder<MessageBuffer>;

/// Msgpack decoder type that decodes from a std::ostream. */
using MsgpackStreamDecoder = BasicMsgpackDecoder<std::istream>;

//------------------------------------------------------------------------------
/** Msgpack format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct Msgpack
{
    template <typename TOutput,
             typename TOutputCategory = OutputCategoryTypeOf<TOutput>>
    using Encoder = BasicMsgpackEncoder<TOutput, TOutputCategory>;

    template <typename TInput,
             typename TInputCategory = InputCategoryTypeOf<TInput>>
    using Decoder = BasicMsgpackDecoder<TInput, TInputCategory>;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::msgpack();}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/msgpack.ipp"
#endif

#endif // CPPWAMP_MSGPACK_HPP
