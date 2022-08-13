/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../msgpack.hpp"
#include <jsoncons_ext/msgpack/msgpack_encoder.hpp>
#include <jsoncons_ext/msgpack/msgpack_parser.hpp>
#include "../api.hpp"
#include "variantdecoding.hpp"
#include "variantencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename O, typename C>
class BasicMsgpackEncoder<O, C>::Impl
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
    using Encoder = jsoncons::msgpack::basic_msgpack_encoder<Sink>;

    Output stub_;
    Encoder encoder_;
};

//------------------------------------------------------------------------------
template <typename O, typename C>
BasicMsgpackEncoder<O, C>::BasicMsgpackEncoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename O, typename C>
BasicMsgpackEncoder<O, C>::~BasicMsgpackEncoder() {}

//------------------------------------------------------------------------------
template <typename O, typename C>
void BasicMsgpackEncoder<O, C>::encode(const Variant& variant, O& output)
{
    impl_->encode(variant, output);
}

//------------------------------------------------------------------------------
template <typename O>
class BasicMsgpackEncoder<O, StreamOutputCategory>::Impl
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
    using Encoder = jsoncons::msgpack::basic_msgpack_encoder<Sink>;

    std::ostream outputStub_;
    Encoder encoder_;
};

//------------------------------------------------------------------------------
template <typename O>
BasicMsgpackEncoder<O, StreamOutputCategory>::BasicMsgpackEncoder()
    : impl_(new Impl)
{}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename O>
BasicMsgpackEncoder<O, StreamOutputCategory>::~BasicMsgpackEncoder() {}

//------------------------------------------------------------------------------
template <typename O>
void BasicMsgpackEncoder<O, StreamOutputCategory>::encode(
    const Variant& variant, O& output)
{
    impl_->encode(variant, output);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicMsgpackEncoder<std::string, ByteContainerOutputCategory>;
template class BasicMsgpackEncoder<MessageBuffer, ByteContainerOutputCategory>;
template class BasicMsgpackEncoder<std::ostream, StreamOutputCategory>;
#endif

//------------------------------------------------------------------------------
template <typename I, typename C>
class BasicMsgpackDecoder<I, C>::Impl
{
public:
    Impl() : decoder_("Msgpack") {}

    std::error_code decode(const I& input, Variant& variant)
    {
        return decoder_.decode(input, variant);
    }

private:
    struct Config
    {
        using Input = I;
        using Source = jsoncons::bytes_source;
        using Parser = jsoncons::msgpack::basic_msgpack_parser<Source>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename I, typename C>
BasicMsgpackDecoder<I, C>::BasicMsgpackDecoder() : impl_(new Impl) {}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I, typename C>
BasicMsgpackDecoder<I, C>::~BasicMsgpackDecoder() {}

//------------------------------------------------------------------------------
template <typename I, typename C>
std::error_code BasicMsgpackDecoder<I, C>::decode(const I& input,
                                                  Variant& variant)
{
    return impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
template <typename I>
class BasicMsgpackDecoder<I, StreamInputCategory>::Impl
{
public:
    Impl() : decoder_("Msgpack", nullptr) {}

    std::error_code decode(I& input, Variant& variant)
    {
        return decoder_.decode(input, variant);
    }

private:
    struct Config
    {
        using Input = std::istream;
        using Source = jsoncons::stream_source<uint8_t>;
        using Parser = jsoncons::msgpack::basic_msgpack_parser<Source>;
    };

    internal::GenericDecoder<Config> decoder_;
};

//------------------------------------------------------------------------------
template <typename I>
BasicMsgpackDecoder<I, StreamInputCategory>::BasicMsgpackDecoder()
    : impl_(new Impl)
{}

//------------------------------------------------------------------------------
// Avoids incomplete type errors.
//------------------------------------------------------------------------------
template <typename I>
BasicMsgpackDecoder<I, StreamInputCategory>::~BasicMsgpackDecoder() {}

//------------------------------------------------------------------------------
template <typename I>
std::error_code
BasicMsgpackDecoder<I, StreamInputCategory>::decode(I& input, Variant& variant)
{
    return impl_->decode(input, variant);
}

//------------------------------------------------------------------------------
// Explicit template instantiations
//------------------------------------------------------------------------------
#ifdef CPPWAMP_COMPILED_LIB
template class BasicMsgpackDecoder<std::string, ByteArrayInputCategory>;
template class BasicMsgpackDecoder<MessageBuffer, ByteArrayInputCategory>;
template class BasicMsgpackDecoder<std::istream, StreamInputCategory>;
#endif

} // namespace wamp
