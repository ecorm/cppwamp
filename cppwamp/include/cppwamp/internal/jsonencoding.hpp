/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_JSONENCODING_HPP
#define CPPWAMP_INTERNAL_JSONENCODING_HPP

#include <jsoncons/json_encoder.hpp>
#include "../blob.hpp"
#include "../null.hpp"
#include "../variantdefs.hpp"
#include "../visitor.hpp"
#include "base64.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEncoder>
class JsonVariantEncodingVisitor
{
public:
    using Sink = typename TEncoder::sink_type;
    using Result = void;

    explicit JsonVariantEncodingVisitor(Sink sink, TEncoder& encoder)
        : sink_(sink),
          encoder_(encoder)
    {
        enter();
    }

    void operator()(Null)
    {
        encoder_.null_value();
        next();
    }

    void operator()(Bool b)
    {
        encoder_.bool_value(b);
        next();
    }

    void operator()(Int n)
    {
        encoder_.int64_value(n);
        next();
    }

    void operator()(UInt n)
    {
        encoder_.uint64_value(n);
        next();
    }

    void operator()(Real x)
    {
        encoder_.double_value(x);
        next();
    }

    void operator()(const String& s)
    {
        encoder_.string_value({s.data(), s.size()});
        next();
    }

    void operator()(const Blob& b)
    {
        static constexpr char prefix[] = "\"\\u0000";
        if (context_.back().needsArraySeparator())
            sink_.push_back(',');
        auto data = reinterpret_cast<const typename Sink::value_type*>(prefix);
        sink_.append(data, sizeof(prefix) - 1);
        Base64::encode(b.data().data(), b.data().size(), sink_);
        sink_.push_back('\"');
        next();
    }

    template <typename TVariant>
    void operator()(const std::vector<TVariant>& array)
    {
        enter(true);
        encoder_.begin_array(array.size());
        for (const auto& v: array)
            wamp::apply(*this, v);
        encoder_.end_array();
        leave();
        next();
    }

    template <typename TVariant>
    void operator()(const std::map<String, TVariant>& object)
    {
        enter();
        encoder_.begin_object(object.size());
        for (const auto& kv: object)
        {
            const auto& key = kv.first;
            encoder_.key({key.data(), key.size()});
            wamp::apply(*this, kv.second);
        }
        encoder_.end_object();
        leave();
        next();
    }

private:
    struct Context
    {
        Context(bool isArray = false) : isArray(isArray), isPopulated(false) {}

        bool needsArraySeparator() const {return isArray && isPopulated;}

        void populate() {isPopulated = true;}

        bool isArray : 1;
        bool isPopulated : 1;
    };

    void next() {context_.back().populate();}

    void enter(bool isArray = false) {context_.push_back(isArray);}

    void leave()
    {
        assert(!context_.empty());
        context_.pop_back();
    }

    Sink sink_;
    TEncoder& encoder_;
    std::vector<Context> context_;
};

//------------------------------------------------------------------------------
template <typename TSink>
class JsonSinkProxy
{
public:
    using value_type = char;

    JsonSinkProxy() = default;

    JsonSinkProxy(TSink& out) : sink_(&out) {}

    void append(const value_type* data, size_t size)
    {
        auto ptr = reinterpret_cast<const SinkByte*>(data);
        sink_->append(ptr, size);
    }

    void push_back(value_type byte)
    {
        // Emulate std::bit_cast
        SinkByte value;
        std::memcpy(&value, &byte, sizeof(byte));
        sink_->push_back(value);
    }

    void flush() {}

private:
    using SinkByte = typename TSink::value_type;

    TSink* sink_ = nullptr;
};

//------------------------------------------------------------------------------
// This implementation needs to be taken out of the json.ipp module to
// avoid a circular dependency with the variant.ipp module.
//------------------------------------------------------------------------------
template <typename TSink>
class JsonEncoderImpl
{
public:
    JsonEncoderImpl() : encoder_(Proxy{}) {}

    template <typename TVariant, typename TOutput>
    void encode(const TVariant& variant, TOutput& output)
    {
        // JsonSinkProxy is needed because shared access to the underlying sink
        // by JsonVariantEncodingVisitor is required for WAMP's special
        // handling of Base64-encoded blobs.
        TSink sink(output);
        Proxy proxy(sink);
        encoder_.reset(std::move(proxy));
        internal::JsonVariantEncodingVisitor<Encoder> visitor(proxy, encoder_);
        wamp::apply(visitor, variant);
    }

private:
    using Proxy = internal::JsonSinkProxy<TSink>;
    using Encoder = jsoncons::basic_compact_json_encoder<char, Proxy>;
    Encoder encoder_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_JSONENCODING_HPP
