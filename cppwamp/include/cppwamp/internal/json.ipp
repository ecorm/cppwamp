/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../json.hpp"
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>
#include <jsoncons/json_encoder.hpp>
#include <jsoncons/json_parser.hpp>
#include "../traits.hpp"
#include "jsonencoding.hpp"
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TInput>
class JsonDecoderImpl
{
public:
    JsonDecoderImpl() : parser_(jsoncons::strict_json_parsing{}) {}

    std::error_code decode(const TInput& input, Variant& variant)
    {
        parser_.reinitialize();
        parser_.update(reinterpret_cast<const char*>(input.data()),
                       input.size());
        visitor_.reset();
        std::error_code ec;
        parser_.finish_parse(visitor_, ec);

        if (!ec)
        {
            // jsoncons::basic_json_parser does not treat an input with no
            // tokens as an error.
            if (visitor_.empty())
                ec = make_error_code(DecodingErrc::emptyInput);
            else
                variant = std::move(visitor_).variant();
        }
        parser_.reset();
        visitor_.reset();

        return ec;
    }

private:
    using Parser = jsoncons::basic_json_parser<char>;
    using Visitor = internal::VariantJsonDecodingVisitor;
    Parser parser_;
    Visitor visitor_;
};

//------------------------------------------------------------------------------
template <>
class JsonDecoderImpl<std::istream> : public JsonDecoderImpl<std::string>
{
public:
    std::error_code decode(std::istream& in, Variant& variant)
    {
        bytes_.clear();
        char buffer[4096];
        while (in.read(buffer, sizeof(buffer)))
            bytes_.append(buffer, sizeof(buffer));
        bytes_.append(buffer, in.gcount());
        auto ec = Base::decode(bytes_, variant);
        bytes_.clear();
        return ec;
    }

private:
    using Base = JsonDecoderImpl<std::string>;

    std::string bytes_;
};

} // namespace internal


//******************************************************************************
// JSON encoder
//******************************************************************************

//------------------------------------------------------------------------------
template <typename TSink>
class SinkEncoder<Json, TSink>::Impl
{
public:
    template <typename TSinkable>
    void encode(const Variant& variant, TSinkable& output)
    {
        encoderImpl_.encode(variant, output);
    }

private:
    using Output = typename TSink::Output;
    using ImplSink = Conditional<isSameType<TSink, StreamSink>(),
                                 jsoncons::stream_sink<char>,
                                 jsoncons::string_sink<Output>>;

    internal::JsonEncoderImpl<ImplSink> encoderImpl_;
};

//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Json, TSink>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Json, TSink>::~SinkEncoder() {}

//------------------------------------------------------------------------------
template <typename TSink>
void SinkEncoder<Json, TSink>::encode(const Variant& variant, Sink sink)
{
    impl_->encode(variant, sink.output());
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SinkEncoder<Json, StringSink>;
template class SinkEncoder<Json, BufferSink>;
template class SinkEncoder<Json, StreamSink>;
#endif


//******************************************************************************
// JSON decoder
//******************************************************************************

//------------------------------------------------------------------------------
template <typename TSource>
class SourceDecoder<Json, TSource>::Impl
{
public:
    std::error_code decode(Source source, Variant& variant)
    {
        return decoderImpl_.decode(source.input(), variant);
    }

private:
    internal::JsonDecoderImpl<typename TSource::Input> decoderImpl_;
};

//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Json, TSource>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Json, TSource>::~SourceDecoder() {}

//------------------------------------------------------------------------------
template <typename TSource>
std::error_code SourceDecoder<Json, TSource>::decode(Source source,
                                                     Variant& variant)
{
    return impl_->decode(source.input(), variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SourceDecoder<Json, StringSource>;
template class SourceDecoder<Json, BufferSource>;
template class SourceDecoder<Json, StreamSource>;
#endif

} // namespace wamp
