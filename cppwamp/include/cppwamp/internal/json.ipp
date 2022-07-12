/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../json.hpp"
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>
#include <jsoncons/json_encoder.hpp>
#include <jsoncons/json_parser.hpp>
#include "../api.hpp"
#include "base64.hpp"
#include "jsonencoding.hpp"
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class JsonDecoderImpl
{
public:
    JsonDecoderImpl() : parser_(jsoncons::strict_json_parsing{}) {}

    void decode(const void* data, std::size_t length, Variant& variant)
    {
        parser_.reinitialize();
        parser_.update(static_cast<const char*>(data), length);
        visitor_.reset();
        try
        {
            parser_.finish_parse(visitor_);
        }
        catch (const jsoncons::ser_error& e)
        {
            parser_.reset();
            visitor_.reset();
            throw error::Decode(std::string("JSON parsing failure: ") +
                                e.what());
        }

        bool empty = visitor_.empty();
        if (!empty)
            variant = std::move(visitor_).variant();
        parser_.reset();
        visitor_.reset();

        // jsoncons::basic_json_parser does not throw for an input
        // with no tokens
        if (empty)
            throw error::Decode(std::string("JSON parsing failure: no tokens"));
    }

private:
    using Parser = jsoncons::basic_json_parser<char>;
    using Visitor = internal::VariantJsonDecodingVisitor;
    Parser parser_;
    Visitor visitor_;
};

} // namespace internal


//******************************************************************************
// JSON encoder
//******************************************************************************

//------------------------------------------------------------------------------
template <typename O, typename C>
class BasicJsonEncoder<O, C>::Impl
{
public:
    template <typename TSinkable>
    void encode(const Variant& variant, TSinkable& output)
    {
        encoderImpl_.encode(variant, output);
    }

private:
    using Sink = typename std::conditional<
            isSameType<C, StreamOutputCategory>(),
            jsoncons::stream_sink<char>,
            jsoncons::string_sink<O>>::type;

    internal::JsonEncoderImpl<Sink> encoderImpl_;
};

//------------------------------------------------------------------------------
template <typename O, typename C>
BasicJsonEncoder<O, C>::BasicJsonEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename O, typename C>
BasicJsonEncoder<O, C>::~BasicJsonEncoder() {}

//------------------------------------------------------------------------------
template <typename O, typename C>
void BasicJsonEncoder<O, C>::encode(const Variant& variant, O& output)
{
    impl_->encode(variant, output);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicJsonEncoder<std::string, ByteContainerOutputCategory>;
template class BasicJsonEncoder<MessageBuffer, ByteContainerOutputCategory>;
template class BasicJsonEncoder<std::ostream, StreamOutputCategory>;
#endif


//******************************************************************************
// JSON decoder
//******************************************************************************

//------------------------------------------------------------------------------
template <typename I, typename C>
class BasicJsonDecoder<I, C>::Impl
{
public:
    void decode(const I& input, Variant& variant)
    {
        decoderImpl_.decode(input.data(), input.size(), variant);
    }

private:
    internal::JsonDecoderImpl decoderImpl_;
};

//------------------------------------------------------------------------------
template <typename I, typename C>
BasicJsonDecoder<I, C>::BasicJsonDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I, typename C>
BasicJsonDecoder<I, C>::~BasicJsonDecoder() {}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the input. */
//------------------------------------------------------------------------------
template <typename I, typename C>
void BasicJsonDecoder<I, C>::decode(const I& input, Variant& variant)
{
    impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
template <typename I>
class BasicJsonDecoder<I, StreamInputCategory>::Impl
{
public:
    void decode(I& input, Variant& variant)
    {
        bytes_.clear();
        char buffer[4096];
        while (input.read(buffer, sizeof(buffer)))
            bytes_.append(buffer, sizeof(buffer));
        bytes_.append(buffer, input.gcount());
        decoderImpl_.decode(bytes_.data(), bytes_.size(), variant);
        bytes_.clear();
    }

private:
    internal::JsonDecoderImpl decoderImpl_;
    std::string bytes_;
};

//------------------------------------------------------------------------------
template <typename I>
BasicJsonDecoder<I, StreamInputCategory>::BasicJsonDecoder()
    : impl_(new Impl)
{}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I>
BasicJsonDecoder<I, StreamInputCategory>::~BasicJsonDecoder() {}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the input. */
//------------------------------------------------------------------------------
template <typename I>
void BasicJsonDecoder<I, StreamInputCategory>::decode(I& input, Variant& variant)
{
    impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicJsonDecoder<std::string, ByteArrayInputCategory>;
template class BasicJsonDecoder<MessageBuffer, ByteArrayInputCategory>;
template class BasicJsonDecoder<std::istream, StreamInputCategory>;
#endif

} // namespace wamp
