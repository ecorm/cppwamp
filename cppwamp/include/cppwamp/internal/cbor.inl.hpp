/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../codecs/cbor.hpp"
#include <jsoncons_ext/cbor/cbor_encoder.hpp>
#include <jsoncons_ext/cbor/cbor_options.hpp>
#include <jsoncons_ext/cbor/cbor_parser.hpp>
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE CborOptions cborWithMaxDepth(unsigned maxDepth)
{
    using Depth = decltype(std::declval<jsoncons::cbor::cbor_options>()
                               .max_nesting_depth());
    using Limits = std::numeric_limits<Depth>;
    CPPWAMP_LOGIC_CHECK(maxDepth < Limits::max(),
                        "maxDepth exceeds limit of underlying option");
    jsoncons::cbor::cbor_options opts;
    opts.max_nesting_depth(static_cast<int>(maxDepth));
    return CborOptions{std::move(opts)};
}


//------------------------------------------------------------------------------
template <typename S>
class SinkEncoder<Cbor, S>::Impl
{
public:
    Impl() = default;

    explicit Impl(const CborOptions& options) : encoder_(options) {}

    void encode(const Variant& variant, S sink)
    {
        encoder_.encode(variant, sink);
    }

private:
    struct Config
    {
        using Sink = S;
        using Options = jsoncons::cbor::cbor_options;

        template <typename TUnderlyingEncoderSink>
        using EncoderType =
            jsoncons::cbor::basic_cbor_encoder<TUnderlyingEncoderSink>;
    };

    internal::GenericEncoder<Config> encoder_;
};

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Cbor, S>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Cbor, S>::SinkEncoder(const Options& options)
    : impl_(new Impl(options))
{}

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Cbor, S>::SinkEncoder(SinkEncoder&&) noexcept = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Cbor, S>::~SinkEncoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Cbor, S>& SinkEncoder<Cbor, S>::operator=(SinkEncoder&&) noexcept
    = default;

//------------------------------------------------------------------------------
template <typename S>
void SinkEncoder<Cbor, S>::encode(const Variant& variant, Sink sink)
{
    impl_->encode(variant, sink);
}


//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SinkEncoder<Cbor, StringSink>;
template class SinkEncoder<Cbor, BufferSink>;
template class SinkEncoder<Cbor, StreamSink>;
#endif

//------------------------------------------------------------------------------
template <typename S>
class SourceDecoder<Cbor, S>::Impl
{
public:
    Impl() : decoder_("Cbor") {}

    explicit Impl(const CborOptions& options) : decoder_("Cbor", options) {}

    std::error_code decode(Source source, Variant& variant)
    {
        return decoder_.decode(source.input(), variant);
    }

private:
    struct Config
    {
        using Source = S;
        using Options = jsoncons::cbor::cbor_options;

        template <typename TImplSource>
        using Parser = jsoncons::cbor::basic_cbor_parser<TImplSource>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Cbor, S>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Cbor, S>::SourceDecoder(const Options& options)
    : impl_(new Impl(options))
{}

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Cbor, S>::SourceDecoder(SourceDecoder&&) noexcept = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Cbor, S>::~SourceDecoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Cbor, S>&
SourceDecoder<Cbor, S>::operator=(SourceDecoder&&) noexcept = default;

//------------------------------------------------------------------------------
template <typename S>
std::error_code SourceDecoder<Cbor, S>::decode(Source source, Variant& variant)
{
    return impl_->decode(source, variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SourceDecoder<Cbor, StringSource>;
template class SourceDecoder<Cbor, BufferSource>;
template class SourceDecoder<Cbor, StreamSource>;
#endif

} // namespace wamp
