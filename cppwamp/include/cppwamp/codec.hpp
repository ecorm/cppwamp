/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CODEC_HPP
#define CPPWAMP_CODEC_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains essential definitions for wamp::Variant codecs. */
//------------------------------------------------------------------------------

#include <istream>
#include <ostream>
#include "api.hpp"
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
// Encoder
//******************************************************************************

/** Output category for containers of bytes which provide `push_back` and
    `insert` member functions. */
struct ByteContainerOutputCategory {};

/** Output category for output streams of bytes. */
struct StreamOutputCategory {};

/** Type used to indicate output category detection failed. */
struct UnknownOutputCategory {};

/** Traits class that determines the category type that best matches the
    given output type. */
template <typename O, typename Enabled = void>
struct OutputCategory
{
    using type = UnknownOutputCategory;
};

template <typename O>
struct OutputCategory<O, EnableIf<std::is_base_of<std::ostream, O>::value,
                                  void>>
{
    using type = StreamOutputCategory;
};

template <typename O>
struct OutputCategory<O, EnableIf<!std::is_base_of<std::ostream, O>::value,
                                  void>>
{
    using type = ByteContainerOutputCategory;
};

/// Yields the category type that best matches the given output type.
template <typename O>
using OutputCategoryTypeOf = typename OutputCategory<ValueTypeOf<O>>::type;

//------------------------------------------------------------------------------
/** Yields the encoder type needed to encode a Variant to the given output type
    and output category
    @tparam F Encoder format type tag (e.g. Json)
    @tparam O Output type (e.g. std::string)
    @tparam C Output category (deduced)
    @see wamp::Decoder
    @see wamp::encode */
//------------------------------------------------------------------------------
template <typename F, typename O, typename C = OutputCategoryTypeOf<O>>
using Encoder = typename F::template Encoder<ValueTypeOf<O>, C>;

//------------------------------------------------------------------------------
/** Encodes the given variant to the given byte container or stream.
    By design, the output is not cleared before encoding.
    The encoder is instantiated once and then discarded.
    @tparam TFormat The codec format tag (e.g. Json)
    @tparam TOutput The output type (deduced)
    @see wamp::decode
    @see wamp::Encoder */
//------------------------------------------------------------------------------
template <typename TFormat, typename TOutput>
void encode(const Variant& variant, TOutput&& output)
{
    Encoder<TFormat, TOutput> encoder;
    encoder.encode(variant, std::forward<TOutput>(output));
}


//******************************************************************************
// Decoder
//******************************************************************************

/** Input category for contiguous byte arrays which provide `data` and `size`
    member functions. */
struct ByteArrayInputCategory {};

/** Input category for input streams of bytes. */
struct StreamInputCategory {};

/** Type used to indicate input category detection failed. */
struct UnknownInputCategory {};

/** Traits class that determines the category type that best matches the
    given input type. */
template <typename I, typename Enabled = void>
struct InputCategory
{
    using type = UnknownOutputCategory;
};

template <typename I>
struct InputCategory<I, EnableIf<std::is_base_of<std::istream, I>::value,
                                 void>>
{
    using type = StreamInputCategory;
};

template <typename I>
struct InputCategory<I, EnableIf<!std::is_base_of<std::istream, I>::value,
                                 void>>
{
    using type = ByteArrayInputCategory;
};

/// Yields the category type that best matches the given input type.
template <typename I>
using InputCategoryTypeOf = typename InputCategory<ValueTypeOf<I>>::type;

//------------------------------------------------------------------------------
/** Yields the decoder type needed to decode a Variant from the given input type
    and input category
    @tparam F Encoder format type tag (e.g. Json)
    @tparam I Input type (e.g. std::string)
    @tparam I Input category (deduced)
    @see wamp::Encoder
    @see wamp::decode */
//------------------------------------------------------------------------------
template <typename F, typename I, typename C = InputCategoryTypeOf<I>>
using Decoder = typename F::template Decoder<ValueTypeOf<I>, C>;

//------------------------------------------------------------------------------
/** Decodes from the given byte sequence or stream to the given variant.
    The decoder is instantiated once and then discarded.
    @tparam TFormat The codec format tag (e.g. Json)
    @tparam TInput The input type (deduced)
    @throws error::Decode if there is an error while parsing the input.
    @see wamp::encode
    @see wamp::Decoder */
//------------------------------------------------------------------------------
template <typename TFormat, typename TInput>
void decode(TInput&& input, Variant& variant)
{
    Decoder<TFormat, TInput> decoder;
    decoder.decode(std::forward<TInput>(input), variant);
}

} // namespace wamp

#endif // CPPWAMP_CODEC_HPP
