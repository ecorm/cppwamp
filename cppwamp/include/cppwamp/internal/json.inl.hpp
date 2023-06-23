/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../json.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>
#include <jsoncons/json_encoder.hpp>
#include <jsoncons/json_parser.hpp>
#include "../traits.hpp"
#include "jsonencoding.hpp"
#include "variantdecoding.hpp"

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
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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
        std::array<char, bufferSize_> buffer = {};
        while (in.read(buffer.data(), bufferSize_))
            bytes_.append(buffer.data(), bufferSize_);
        bytes_.append(buffer.data(), in.gcount());
        auto ec = Base::decode(bytes_, variant);
        bytes_.clear();
        return ec;
    }

private:
    using Base = JsonDecoderImpl<std::string>;

    static constexpr unsigned bufferSize_ = 4096;
    std::string bytes_;
};

} // namespace internal


//******************************************************************************
// JSON encoder
//******************************************************************************

//------------------------------------------------------------------------------
template <typename S>
class SinkEncoder<Json, S>::Impl
{
public:
    template <typename Sable>
    void encode(const Variant& variant, Sable& output)
    {
        encoderImpl_.encode(variant, output);
    }

private:
    using Output = typename S::Output;
    using ImplSink = Conditional<isSameType<S, StreamSink>(),
                                 jsoncons::stream_sink<char>,
                                 jsoncons::string_sink<Output>>;

    internal::JsonEncoderImpl<ImplSink> encoderImpl_;
};

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Json, S>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Json, S>::SinkEncoder(SinkEncoder&&) noexcept = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Json, S>::~SinkEncoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Json, S>& SinkEncoder<Json, S>::operator=(SinkEncoder&&) noexcept
    = default;

//------------------------------------------------------------------------------
template <typename S>
void SinkEncoder<Json, S>::encode(const Variant& variant, Sink sink)
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
template <typename S>
class SourceDecoder<Json, S>::Impl
{
public:
    std::error_code decode(Source source, Variant& variant)
    {
        return decoderImpl_.decode(source.input(), variant);
    }

private:
    internal::JsonDecoderImpl<typename S::Input> decoderImpl_;
};

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Json, S>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Json, S>::SourceDecoder(SourceDecoder&&) noexcept = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Json, S>::~SourceDecoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Json, S>&
SourceDecoder<Json, S>::operator=(SourceDecoder&&) noexcept = default;

//------------------------------------------------------------------------------
template <typename S>
std::error_code SourceDecoder<Json, S>::decode(Source source, Variant& variant)
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
