/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MSGPACK_HPP
#define CPPWAMP_MSGPACK_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the MSGPACK codec. */
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
/** %Msgpack format tag type.
    Meets the requirements of the @ref CodecFormat concept. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Msgpack
{
    /** Default contructor. */
    constexpr Msgpack() = default;

    /** Obtains a numeric identifier associated with this codec. */
    static constexpr int id() {return KnownCodecIds::msgpack();}
};

/** Instance of the Msgpack tag. */
constexpr CPPWAMP_INLINE Msgpack msgpack;

//------------------------------------------------------------------------------
/** %Msgpack encoder.
    This class uses [jsoncons][1] to serialize MSGPACK payloads from Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecEncoder concept.

    @tparam TSink The output sink type in which to encode. */
//------------------------------------------------------------------------------
template <typename TSink>
class CPPWAMP_API SinkEncoder<Msgpack, TSink>
{
public:
    using Sink = TSink;
    using Output = typename Sink::Output;

    /** Default constructor. */
    SinkEncoder();

    /** Move constructor. */
    SinkEncoder(SinkEncoder&&);

    /** Destructor. */
    ~SinkEncoder();

    /** Move assignment. */
    SinkEncoder& operator=(SinkEncoder&&);

    /** Serializes from the given variant to the given output sink
        (it does not first clear the output, by design). */
    void encode(const Variant& variant, Sink sink);

    /** @name Noncopyable */
    /// @{
    SinkEncoder(const SinkEncoder&) = delete;
    SinkEncoder& operator=(const SinkEncoder&) = delete;
    /// @}

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Yields the %Msgpack encoder type for the given output sink type.
template <typename TSink>
using MsgpackEncoder = SinkEncoder<Msgpack, TSink>;

/// %Msgpack encoder type that encodes into a std::string. */
using MsgpackStringEncoder = MsgpackEncoder<StringSink>;

/// %Msgpack encoder type that encodes into a MessageBuffer. */
using MsgpackBufferEncoder = MsgpackEncoder<BufferSink>;

/// %Msgpack encoder type that encodes into a std::ostream. */
using MsgpackStreamEncoder = MsgpackEncoder<StreamSink>;


//------------------------------------------------------------------------------
/** %Msgpack decoder.
    This class uses [jsoncons][1] to deserialize MSGPACK payloads into Variant
    instances.
    [1]: https://github.com/danielaparker/jsoncons

    Meets the requirements of the @ref CodecDecoder concept.

    @tparam TSource The input source type from which to decode. */
//------------------------------------------------------------------------------
template <typename TSource>
class CPPWAMP_API SourceDecoder<Msgpack, TSource>
{
public:
    using Source = TSource;
    using Input = typename Source::Input;

    /** Default constructor. */
    SourceDecoder();

    /** Move constructor. */
    SourceDecoder(SourceDecoder&&);

    /** Destructor. */
    ~SourceDecoder();

    /** Move assignment. */
    SourceDecoder& operator=(SourceDecoder&&);

    /** Deserializes from the given input source to the given variant. */
    CPPWAMP_NODISCARD std::error_code decode(Source source, Variant& variant);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Yields the %Msgpack decoder type for the given input source type.
template <typename TSource>
using MsgpackDecoder = SourceDecoder<Msgpack, TSource>;

/// %Msgpack decoder type that decodes from a std::string. */
using MsgpackStringDecoder = MsgpackDecoder<StringSource>;

/// %Msgpack decoder type that decodes from a MessageBuffer. */
using MsgpackBufferDecoder = MsgpackDecoder<BufferSource>;

/// %Msgpack decoder type that decodes from a std::istream. */
using MsgpackStreamDecoder = MsgpackDecoder<StreamSource>;

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/msgpack.inl.hpp"
#endif

#endif // CPPWAMP_MSGPACK_HPP
