/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../cbor.hpp"
#include <jsoncons_ext/cbor/cbor_encoder.hpp>
#include <jsoncons_ext/cbor/cbor_parser.hpp>
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename TSink>
class SinkEncoder<Cbor, TSink>::Impl
{
public:
    void encode(const Variant& variant, TSink sink)
    {
        encoder_.encode(variant, sink);
    }

private:
    struct Config
    {
        using Sink = TSink;

        template <typename TUnderlyingEncoderSink>
        using EncoderType =
            jsoncons::cbor::basic_cbor_encoder<TUnderlyingEncoderSink>;
    };

    internal::GenericEncoder<Config> encoder_;
};

//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Cbor, TSink>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Cbor, TSink>::~SinkEncoder() {}

//------------------------------------------------------------------------------
template <typename TSink>
void SinkEncoder<Cbor, TSink>::encode(const Variant& variant, Sink sink)
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
template <typename TSource>
class SourceDecoder<Cbor, TSource>::Impl
{
public:
    Impl() : decoder_("Cbor") {}

    std::error_code decode(Source source, Variant& variant)
    {
        return decoder_.decode(source.input(), variant);
    }

private:
    struct Config
    {
        using Source = TSource;

        template <typename TImplSource>
        using Parser = jsoncons::cbor::basic_cbor_parser<TImplSource>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Cbor, TSource>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Cbor, TSource>::~SourceDecoder() {}

//------------------------------------------------------------------------------
template <typename TSource>
std::error_code SourceDecoder<Cbor, TSource>::decode(Source source,
                                                     Variant& variant)
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
