/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../msgpack.hpp"
#include <jsoncons_ext/msgpack/msgpack_encoder.hpp>
#include <jsoncons_ext/msgpack/msgpack_parser.hpp>
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename S>
class SinkEncoder<Msgpack, S>::Impl
{
public:
    void encode(const Variant& variant, S sink)
    {
        encoder_.encode(variant, sink);
    }

private:
    struct Config
    {
        using Sink = S;

        template <typename TUnderlyingEncoderSink>
        using EncoderType =
            jsoncons::msgpack::basic_msgpack_encoder<TUnderlyingEncoderSink>;
    };

    internal::GenericEncoder<Config> encoder_;
};

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Msgpack, S>::SinkEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Msgpack, S>::SinkEncoder(SinkEncoder&&) = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Msgpack, S>::~SinkEncoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SinkEncoder<Msgpack, S>&
SinkEncoder<Msgpack, S>::operator=(SinkEncoder&&) = default;

//------------------------------------------------------------------------------
template <typename S>
void SinkEncoder<Msgpack, S>::encode(const Variant& variant, Sink sink)
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
template <typename S>
class SourceDecoder<Msgpack, S>::Impl
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
        using Source = S;

        template <typename TImplSource>
        using Parser = jsoncons::msgpack::basic_msgpack_parser<TImplSource>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Msgpack, S>::SourceDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Msgpack, S>::SourceDecoder(SourceDecoder&&) = default;

//------------------------------------------------------------------------------
// Avoids incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Msgpack, S>::~SourceDecoder() = default;

//------------------------------------------------------------------------------
template <typename S>
SourceDecoder<Msgpack, S>&
SourceDecoder<Msgpack, S>::operator=(SourceDecoder&&) = default;

//------------------------------------------------------------------------------
template <typename S>
std::error_code SourceDecoder<Msgpack, S>::decode(Source source,
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
