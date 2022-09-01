/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../msgpack.hpp"
#include <jsoncons_ext/msgpack/msgpack_encoder.hpp>
#include <jsoncons_ext/msgpack/msgpack_parser.hpp>
#include "../api.hpp"
#include "../traits.hpp"
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename TSink>
class SinkEncoder<Msgpack, TSink>::Impl
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
            jsoncons::msgpack::basic_msgpack_encoder<TUnderlyingEncoderSink>;
    };

    internal::GenericEncoder<Config> encoder_;
};

//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Msgpack, TSink>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSink>
SinkEncoder<Msgpack, TSink>::~SinkEncoder() {}

//------------------------------------------------------------------------------
template <typename TSink>
void SinkEncoder<Msgpack, TSink>::encode(const Variant& variant, Sink sink)
{
    impl_->encode(variant, sink);
}


//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SinkEncoder<Msgpack, StringSink>;
template class SinkEncoder<Msgpack, BufferSink>;
template class SinkEncoder<Msgpack, StreamSink>;
#endif

//------------------------------------------------------------------------------
template <typename TSource>
class SourceDecoder<Msgpack, TSource>::Impl
{
public:
    Impl() : decoder_("Msgpack") {}

    std::error_code decode(Source source, Variant& variant)
    {
        return decoder_.decode(source.input(), variant);
    }

private:
    struct Config
    {
        using Source = TSource;

        template <typename TImplSource>
        using Parser = jsoncons::msgpack::basic_msgpack_parser<TImplSource>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Msgpack, TSource>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename TSource>
SourceDecoder<Msgpack, TSource>::~SourceDecoder() {}

//------------------------------------------------------------------------------
template <typename TSource>
std::error_code SourceDecoder<Msgpack, TSource>::decode(Source source,
                                                        Variant& variant)
{
    return impl_->decode(source, variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class SourceDecoder<Msgpack, StringSource>;
template class SourceDecoder<Msgpack, BufferSource>;
template class SourceDecoder<Msgpack, StreamSource>;
#endif

} // namespace wamp
