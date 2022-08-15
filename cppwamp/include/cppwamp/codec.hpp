/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CODEC_HPP
#define CPPWAMP_CODEC_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains essential definitions for wamp::Variant codecs. */
//------------------------------------------------------------------------------

#include <istream>
#include <functional>
#include <memory>
#include <ostream>
#include "api.hpp"
#include "config.hpp"
#include "messagebuffer.hpp"
#include "traits.hpp"
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** IDs used by rawsocket transports to negotiate the serializer.
    As described in section Advanced Profile / Other Advanced Features /
    Alternative Transports / RawSocket Transport of the WAMP spec.
    Additional non-standard serializers are listed in
    https://github.com/crossbario/autobahn-python/blob/master/autobahn/wamp/serializer.py. */
//------------------------------------------------------------------------------
struct CPPWAMP_API KnownCodecIds
{
    static constexpr int json() {return 1;}
    static constexpr int msgpack() {return 2;}
    static constexpr int cbor() {return 3;}
};


//******************************************************************************
// Output Sinks
//******************************************************************************

//------------------------------------------------------------------------------
/** This primary template is specialized for each type that can be the
    output of a codec encoding operation. */
//------------------------------------------------------------------------------
template <typename TOutput, typename TEnabled = void>
class OutputSink
{};

//------------------------------------------------------------------------------
/** Output sink specialization referencing a std::string. */
//------------------------------------------------------------------------------
template <>
class OutputSink<std::string>
{
public:
    using Output = std::string;
    OutputSink(Output& s) : output_(&s) {}
    Output& output() const {return *output_;}

private:
    Output* output_;
};

//------------------------------------------------------------------------------
/** Output sink specialization referencing a wamp::MessageBuffer. */
//------------------------------------------------------------------------------
template <>
class OutputSink<MessageBuffer>
{
public:
    using Output = MessageBuffer;
    OutputSink(Output& b) : output_(&b) {}
    Output& output() const {return *output_;}

private:
    Output* output_;
};

//------------------------------------------------------------------------------
/** Output sink specialization referencing a std::ostream. */
//------------------------------------------------------------------------------
template <>
class OutputSink<std::ostream>
{
public:
    using Output = std::ostream;
    OutputSink(Output& b) : output_(&b) {}
    Output& output() const {return *output_;}

private:
    Output* output_;
};

//------------------------------------------------------------------------------
using StringSink = OutputSink<std::string>;   ///< Output sink for strings.
using BufferSink = OutputSink<MessageBuffer>; ///< Output sink for message buffers.
using StreamSink = OutputSink<std::ostream>;  ///< Output sink for streams.

//------------------------------------------------------------------------------
template <typename TOutput, typename TEnabled = void>
struct OutputTraits {};

template <>
struct OutputTraits<std::string> { using Sink = StringSink; };

template <>
struct OutputTraits<MessageBuffer> { using Sink = BufferSink; };

template <typename S>
struct OutputTraits<S, EnableIf<std::is_base_of<std::ostream, S>::value, void>>
{
    using Sink = StreamSink;
};

//------------------------------------------------------------------------------
/** Yields the OutputSink specialization associated with the given output type. */
//------------------------------------------------------------------------------
template <typename TOutput>
using SinkTypeFor = typename OutputTraits<ValueTypeOf<TOutput>>::Sink;


//******************************************************************************
// Encoder
//******************************************************************************

//------------------------------------------------------------------------------
/** Primary template specialized for serialization format tag and sink combinations . */
//------------------------------------------------------------------------------
template <typename TFormat, typename TSink, typename TEnable = void>
class SinkEncoder {};

//------------------------------------------------------------------------------
/** Yields the encoder type needed to encode a Variant to the given output type
    using the given format.
    @tparam F Encoder format type tag (e.g. Json)
    @tparam O Output type (e.g. std::string)
    @see wamp::Decoder
    @see wamp::encode */
//------------------------------------------------------------------------------
template <typename F, typename O>
using EncoderFor = SinkEncoder<F, SinkTypeFor<O>>;

//------------------------------------------------------------------------------
/** Encodes the given variant to the given byte container or stream.
    By design, the output is not cleared before encoding.
    The encoder is instantiated once and then discarded.
    @tparam TFormat The serialization format tag (e.g. Json)
    @tparam TOutput The output type (deduced)
    @see wamp::decode
    @see wamp::Encoder */
//------------------------------------------------------------------------------
template <typename TFormat, typename TOutput>
void encode(const Variant& variant, TOutput&& output)
{
    EncoderFor<TFormat, TOutput> encoder;
    encoder.encode(variant, std::forward<TOutput>(output));
}

//******************************************************************************
// Input Sources
//******************************************************************************

//------------------------------------------------------------------------------
/** This primary template is specialized for each type that can be the
    input of a codec decoding operation. */
//------------------------------------------------------------------------------
template <typename TInput, typename TEnabled = void>
class InputSource
{};

//------------------------------------------------------------------------------
/** Input source specialization referencing a std::string. */
//------------------------------------------------------------------------------
template <>
class InputSource<std::string>
{
public:
    using Input = std::string;
    InputSource(const Input& s) : input_(&s) {}
    const Input& input() const {return *input_;}

private:
    const Input* input_;
};

//------------------------------------------------------------------------------
/** Input source specialization referencing a wamp::MessageBuffer. */
//------------------------------------------------------------------------------
template <>
class InputSource<MessageBuffer>
{
public:
    using Input = MessageBuffer;
    InputSource(const Input& b) : input_(&b) {}
    const Input& input() const {return *input_;}

private:
    const Input* input_;
};

//------------------------------------------------------------------------------
/** Input source specialization referencing a std::ostream. */
//------------------------------------------------------------------------------
template <>
class InputSource<std::istream>
{
public:
    using Input = std::istream;
    InputSource(Input& b) : input_(&b) {}
    Input& input() const {return *input_;}

private:
    Input* input_;
};

//------------------------------------------------------------------------------
using StringSource = InputSource<std::string>;   ///< Input source for strings.
using BufferSource = InputSource<MessageBuffer>; ///< Input source for message buffers.
using StreamSource = InputSource<std::istream>;  ///< Input source for streams.

//------------------------------------------------------------------------------
template <typename TInput, typename TEnabled = void>
struct InputTraits {};

template <>
struct InputTraits<std::string> { using Source = StringSource; };

template <>
struct InputTraits<MessageBuffer> { using Source = BufferSource; };

template <typename S>
struct InputTraits<S, EnableIf<std::is_base_of<std::istream, S>::value, void>>
{
    using Source = StreamSource;
};

//------------------------------------------------------------------------------
/** Yields the InputSource specialization associated with the given input type. */
//------------------------------------------------------------------------------
template <typename TInput>
using SourceTypeFor = typename InputTraits<ValueTypeOf<TInput>>::Source;


//******************************************************************************
// Decoder
//******************************************************************************

//------------------------------------------------------------------------------
/** Primary template specialized for serialization format tag and source combinations . */
//------------------------------------------------------------------------------
template <typename TFormat, typename TSource, typename TEnable = void>
class SourceDecoder {};

//------------------------------------------------------------------------------
/** Yields the decoder type needed to decode a Variant from the given input type
    using the given format.
    @tparam F Encoder format type tag (e.g. Json)
    @tparam I Input type (e.g. std::string)
    @see wamp::Encoder
    @see wamp::decode */
//------------------------------------------------------------------------------
template <typename F, typename I>
using DecoderFor = SourceDecoder<F, SourceTypeFor<I>>;

//------------------------------------------------------------------------------
/** Decodes from the given byte sequence or stream to the given variant.
    The decoder is instantiated once and then discarded.
    @tparam TFormat The serialization format tag (e.g. Json)
    @tparam TInput The input type (deduced)
    @return a std::error_code indicating success or failure
    @see wamp::encode
    @see wamp::Decoder */
//------------------------------------------------------------------------------
template <typename TFormat, typename TInput>
CPPWAMP_NODISCARD std::error_code decode(TInput&& input, Variant& variant)
{
    DecoderFor<TFormat, TInput> decoder;
    return decoder.decode(std::forward<TInput>(input), variant);
}


//******************************************************************************
// Codec and AnyCodec
//******************************************************************************

//------------------------------------------------------------------------------
/** Combines an encoder and a decoder for the same serialization format.
    @tparam TFormat Serialization format tag (e.g. Json).
    @tparam TSink Output sink type, a specialization of OutputSink.
    @tparam TSource Input source type, a specialization of InputSource. */
//------------------------------------------------------------------------------
template <typename TFormat, typename TSink, typename TSource>
class CPPWAMP_API Codec
{
public:
    using Format = TFormat; ///< The encoding/decoding format (e.g. Json).
    using Sink = TSink;     ///< Output sink type in which to encode.
    using Source = TSource; ///< Input source type from which to decode.

    /** Encodes the given variant to the given output sink. */
    void encode(const Variant& variant, Sink sink)
    {
        encoder_.encode(variant, sink);
    };

    /** Decodes a variant from the given input source. */
    CPPWAMP_NODISCARD std::error_code decode(Source source, Variant& variant)
    {
        return decoder_.decode(source, variant);
    }

private:
    SinkEncoder<Format, Sink> encoder_;
    SourceDecoder<Format, Source> decoder_;
};

//------------------------------------------------------------------------------
/** Abstract base class for polymorphic codecs.
    @tparam TSink Output sink type, a specialization of OutputSink.
    @tparam TSource Input source type, a specialization of InputSource. */
//------------------------------------------------------------------------------
template <typename TSink, typename TSource>
class CPPWAMP_API PolymorphicCodecInterface
{
public:
    using Sink = TSink;     ///< Output sink type in which to encode.
    using Source = TSource; ///< Input source type from which to decode.

    /** Destructor. */
    virtual ~PolymorphicCodecInterface() {}

    /** Encodes the given variant to the given output sink. */
    virtual void encode(const Variant& variant, Sink sink) = 0;

    /** Decodes a variant from the given input source. */
    CPPWAMP_NODISCARD virtual std::error_code decode(Source source,
                                                     Variant& variant) = 0;
};

//------------------------------------------------------------------------------
/** Polymorphic codec.
    @tparam TFormat Serialization format tag (e.g. Json).
    @tparam TSink Output sink type, a specialization of OutputSink.
    @tparam TSource Input source type, a specialization of InputSource. */
//------------------------------------------------------------------------------
template <typename TFormat, typename TSink, typename TSource>
class CPPWAMP_API PolymorphicCodec
    : public PolymorphicCodecInterface<TSink, TSource>
{
public:
    using Format = TFormat; ///< The encoding/decoding format (e.g. Json).
    using Sink = TSink;     ///< Output sink type in which to encode.
    using Source = TSource; ///< Input source type from which to decode.

    /** Encodes the given variant to the given output sink. */
    void encode(const Variant& variant, Sink sink) override
    {
        codec_.encode(variant, sink);
    };

    /** Decodes a variant from the given input source. */
    CPPWAMP_NODISCARD std::error_code decode(Source source,
                                             Variant& variant) override
    {
        return codec_.decode(source, variant);
    }

private:
    Codec<Format, Sink, Source> codec_;
};

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic codec.
    @tparam TSink Output sink type, a specialization of OutputSink.
    @tparam TSource Input source type, a specialization of InputSource. */
//------------------------------------------------------------------------------
template <typename TSink, typename TSource>
class CPPWAMP_API AnyCodec
{
public:
    using Sink = TSink;     ///< Output sink type in which to encode.
    using Source = TSource; ///< Input source type from which to decode.

    /** Constructor taking a serialization format tag (e.g. wamp::json). */
    template <typename TFormat>
    AnyCodec(TFormat)
        : codec_(std::make_shared<PolymorphicCodec<TFormat, Sink, Source>>())
    {}

    /** Encodes the given variant to the given output sink. */
    void encode(const Variant& variant, Sink sink)
    {
        codec_->encode(variant, sink);
    };

    /** Decodes a variant from the given input source. */
    CPPWAMP_NODISCARD std::error_code decode(Source source, Variant& variant)
    {
        return codec_->decode(source, variant);
    }

private:
    using Interface = PolymorphicCodecInterface<Sink, Source>;
    std::shared_ptr<Interface> codec_;
};

/// Type-erased codec for string sources/sinks.
using AnyStringCodec = AnyCodec<StringSink, StringSource>;

/// Type-erased codec for buffer sources/sinks.
using AnyBufferCodec = AnyCodec<BufferSink, BufferSource>;

/// Type-erased codec for stream sources/sinks.
using AnyStreamCodec = AnyCodec<StreamSink, StreamSource>;

//------------------------------------------------------------------------------
/** Builds a codec on demand.
    @tparam TSink Output sink type, a specialization of OutputSink.
    @tparam TSource Input source type, a specialization of InputSource. */
//------------------------------------------------------------------------------
template <typename TSink, typename TSource>
class CPPWAMP_API CodecBuilder
{
public:
    /** AnyCodec specialization to be built. */
    using AnyCodecType = AnyCodec<TSink, TSource>;

    /** Constructor taking a serialization format tag. */
    template <typename TFormat>
    explicit CodecBuilder(TFormat)
        : builder_([]() -> AnyCodecType {return AnyCodecType(TFormat{});}),
          id_(TFormat::id())
    {}

    int id() const {return id_;}

    /** Builds and returns a codec for the serialzation format that was given
        during construction. */
    AnyCodecType operator()() const {return builder_();}

private:
    std::function<AnyCodecType ()> builder_;
    int id_;
};

/// Builds a type-erased codec for string sources/sinks.
using StringCodecBuilder = CodecBuilder<StringSink, StringSource>;

/// Builds a type-erased codec for buffer sources/sinks.
using BufferCodecBuilder = CodecBuilder<BufferSink, BufferSource>;

/// Builds a type-erased codec for stream sources/sinks.
using StreamCodecBuilder = CodecBuilder<StreamSink, StreamSource>;

} // namespace wamp

#endif // CPPWAMP_CODEC_HPP
