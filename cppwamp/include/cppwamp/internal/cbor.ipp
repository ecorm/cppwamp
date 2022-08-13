/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../cbor.hpp"
#include <jsoncons_ext/cbor/cbor_encoder.hpp>
#include <jsoncons_ext/cbor/cbor_parser.hpp>
#include "../api.hpp"
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename O, typename C>
class BasicCborEncoder<O, C>::Impl
{
public:
    using Output = O;

    Impl() : encoder_(stub_) {}

    template <typename TSinkable>
    void encode(const Variant& variant, TSinkable&& output)
    {
        encoder_.reset(std::forward<TSinkable>(output));
        wamp::apply(internal::VariantEncodingVisitor<Encoder>(encoder_),
                    variant);
    }

private:
    using Sink = jsoncons::string_sink<Output>;
    using Encoder = jsoncons::cbor::basic_cbor_encoder<Sink>;

    Output stub_;
    Encoder encoder_;
};

//------------------------------------------------------------------------------
template <typename O, typename C>
BasicCborEncoder<O, C>::BasicCborEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename O, typename C>
BasicCborEncoder<O, C>::~BasicCborEncoder() {}

//------------------------------------------------------------------------------
template <typename O, typename C>
void BasicCborEncoder<O, C>::encode(const Variant& variant, O& output)
{
    impl_->encode(variant, output);
}

//------------------------------------------------------------------------------
template <typename O>
class BasicCborEncoder<O, StreamOutputCategory>::Impl
{
public:
    using Output = O;

    Impl() : outputStub_(nullptr), encoder_(outputStub_) {}

    template <typename TSinkable>
    void encode(const Variant& variant, TSinkable&& output)
    {
        encoder_.reset(std::forward<TSinkable>(output));
        wamp::apply(internal::VariantEncodingVisitor<Encoder>(encoder_),
                    variant);
    }

private:
    using Sink = jsoncons::binary_stream_sink;
    using Encoder = jsoncons::cbor::basic_cbor_encoder<Sink>;

    std::ostream outputStub_;
    Encoder encoder_;
};

//------------------------------------------------------------------------------
template <typename O>
BasicCborEncoder<O, StreamOutputCategory>::BasicCborEncoder()
    : impl_(new Impl)
{}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename O>
BasicCborEncoder<O, StreamOutputCategory>::~BasicCborEncoder() {}

//------------------------------------------------------------------------------
template <typename O>
void BasicCborEncoder<O, StreamOutputCategory>::encode(const Variant& variant,
                                                       O& output)
{
    impl_->encode(variant, output);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicCborEncoder<std::string, ByteContainerOutputCategory>;
template class BasicCborEncoder<MessageBuffer, ByteContainerOutputCategory>;
template class BasicCborEncoder<std::ostream, StreamOutputCategory>;
#endif

//------------------------------------------------------------------------------
template <typename I, typename C>
class BasicCborDecoder<I, C>::Impl
{
public:
    Impl() : decoder_("Cbor") {}

    std::error_code decode(const I& input, Variant& variant)
    {
        return decoder_.decode(input, variant);
    }

private:
    struct Config
    {
        using Input = I;
        using Source = jsoncons::bytes_source;
        using Parser = jsoncons::cbor::basic_cbor_parser<Source>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename I, typename C>
BasicCborDecoder<I, C>::BasicCborDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I, typename C>
BasicCborDecoder<I, C>::~BasicCborDecoder() {}

//------------------------------------------------------------------------------
template <typename I, typename C>
std::error_code BasicCborDecoder<I, C>::decode(const I& input, Variant& variant)
{
    return impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
template <typename I>
class BasicCborDecoder<I, StreamInputCategory>::Impl
{
public:
    Impl() : decoder_("Cbor", nullptr) {}

    std::error_code decode(I& input, Variant& variant)
    {
        return decoder_.decode(input, variant);
    }

private:
    struct Config
    {
        using Input = std::istream;
        using Source = jsoncons::stream_source<uint8_t>;
        using Parser = jsoncons::cbor::basic_cbor_parser<Source>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename I>
BasicCborDecoder<I, StreamInputCategory>::BasicCborDecoder()
    : impl_(new Impl)
{}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I>
BasicCborDecoder<I, StreamInputCategory>::~BasicCborDecoder() {}

//------------------------------------------------------------------------------
template <typename I>
std::error_code
BasicCborDecoder<I, StreamInputCategory>::decode(I& input, Variant& variant)
{
    return impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicCborDecoder<std::string, ByteArrayInputCategory>;
template class BasicCborDecoder<MessageBuffer, ByteArrayInputCategory>;
template class BasicCborDecoder<std::istream, StreamInputCategory>;
#endif

} // namespace wamp
