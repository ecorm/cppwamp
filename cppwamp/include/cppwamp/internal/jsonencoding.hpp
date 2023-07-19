/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_JSONENCODING_HPP
#define CPPWAMP_INTERNAL_JSONENCODING_HPP

#include <stack>
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
          encoder_(&encoder)
    {
        enter();
    }

    void operator()(Null)
    {
        encoder_->null_value();
        next();
    }

    void operator()(Bool b)
    {
        encoder_->bool_value(b);
        next();
    }

    void operator()(Int n)
    {
        encoder_->int64_value(n);
        next();
    }

    void operator()(UInt n)
    {
        encoder_->uint64_value(n);
        next();
    }

    void operator()(Real x)
    {
        encoder_->double_value(x);
        next();
    }

    void operator()(const String& s)
    {
        encoder_->string_value({s.data(), s.size()});
        next();
    }

    void operator()(const Blob& b)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
        static constexpr char prefix[] = "\"\\u0000";
        if (context_.back().needsArraySeparator())
            sink_.push_back(',');
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto data = reinterpret_cast<const typename Sink::value_type*>(prefix);
        sink_.append(data, sizeof(prefix) - 1);
        Base64::encode(b.data().data(), b.data().size(), sink_);
        sink_.push_back('\"');
        next();
    }

    template <typename TVariant>
    void operator()(const std::vector<TVariant>& array)
    {
        // TODO: Use stack container instead of recursion
        enter(true);
        encoder_->begin_array(array.size());
        for (const auto& v: array)
            wamp::apply(*this, v);
        encoder_->end_array();
        leave();
        next();
    }

    template <typename TVariant>
    void operator()(const std::map<String, TVariant>& object)
    {
        // TODO: Use stack container instead of recursion
        enter();
        encoder_->begin_object(object.size());
        for (const auto& kv: object)
        {
            const auto& key = kv.first;
            encoder_->key({key.data(), key.size()});
            wamp::apply(*this, kv.second);
        }
        encoder_->end_object();
        leave();
        next();
    }

private:
    struct Context
    {
        explicit Context(bool isArray = false)
            : isArray(isArray),
              isPopulated(false)
        {}

        bool needsArraySeparator() const {return isArray && isPopulated;}

        void populate() {isPopulated = true;}

        bool isArray : 1;
        bool isPopulated : 1;
    };

    void next() {context_.back().populate();}

    void enter(bool isArray = false) {context_.push_back(Context{isArray});}

    void leave()
    {
        assert(!context_.empty());
        context_.pop_back();
    }

    Sink sink_;
    TEncoder* encoder_ = nullptr;
    std::vector<Context> context_;
};

//------------------------------------------------------------------------------
template <typename TVariant, typename TEncoder>
class JsonVariantEncoder
{
public:
    using Sink = typename TEncoder::sink_type;
    using Result = void;

    explicit JsonVariantEncoder(Sink sink, TEncoder& encoder)
        : sink_(sink),
          encoder_(&encoder)
    {}

    void encode(const TVariant& v)
    {
        stack_.emplace(Context{&v});

        while (stack_.empty())
        {
            auto& context = stack_.top();
            bool done = false;
            const TVariant* variant = nullptr;

            if (context.isObject())
            {
                const auto& key = context.iterator.forObject->first;
                encoder_->key({key.data(), key.size()});
                variant = &(context.iterator.forObject->second);
            }
            else if (context.isArray())
            {
                variant = &(*context.iterator.forArray);
            }
            else
            {
                variant = context.variant;
            }

            switch (variant->typeId())
            {
            case TypeId::null:
                encoder_->null_value();
                done = context.advance();
                break;

            case TypeId::boolean:
                encoder_->bool_value(variant->template as<TypeId::boolean>());
                done = context.advance();
                break;

            case TypeId::integer:
                encoder_->int64_value(variant->template as<TypeId::integer>());
                done = context.advance();
                break;

            case TypeId::uint:
                encoder_->uint64_value(variant->template as<TypeId::uint>());
                done = context.advance();
                break;

            case TypeId::real:
                encoder_->double_value(variant->template as<TypeId::real>());
                done = context.advance();
                break;

            case TypeId::string:
            {
                const auto& str = variant->template as<TypeId::string>();
                encoder_->string_value({str.data(), str.size()});
                done = context.advance();
                break;
            }

            case TypeId::blob:
                encodeBlob(variant->template as<TypeId::blob>());
                done = context.advance();
                break;

            case TypeId::array:
            {
                const auto& array = variant->template as<TypeId::array>();
                encoder_->begin_array(array.size());
                stack_.emplace(Context{variant});
                break;
            }

            case TypeId::object:
            {
                const auto& object = variant->template as<TypeId::object>();
                encoder_->begin_object(object.size());
                stack_.emplace(Context{variant});
                break;
            }
            }

            while (!stack_.empty() && done)
            {
                stack_.pop();
                auto& context = stack_.top();
                done = context.advance();
                if (done)
                {
                    if (context.isArray())
                        encoder_->end_array();
                    if (context.isObject())
                        encoder_->end_object();
                }
            }
        }
    }

    void encodeBlob(const Blob& b)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
        static constexpr char prefix[] = "\"\\u0000";
        if (!stack_.empty() && stack_.top().needsArraySeparator())
            sink_.push_back(',');
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto data = reinterpret_cast<const typename Sink::value_type*>(prefix);
        sink_.append(data, sizeof(prefix) - 1);
        Base64::encode(b.data().data(), b.data().size(), sink_);
        sink_.push_back('\"');
    }

private:
    struct Context
    {
        using Array = std::vector<TVariant>;
        using Object = std::map<String, TVariant>;

        union ArrayOrObjectIterator
        {
            ArrayOrObjectIterator() {}

            typename Array::const_iterator forArray;
            typename Object::const_iterator forObject;
        };

        explicit Context(const TVariant* var)
            : variant(var)
        {
            if (isArray())
                iterator.forArray = asArray().begin();
            if (isObject())
                iterator.forObject = asObject().begin();
        }

        bool advance()
        {
            if (isArray())
            {
                ++iterator.forArray;
                return iterator.forArray == asArray().end();
            }

            if (isObject())
            {
                ++iterator.forObject;
                return iterator.forObject == asObject().end();
            }

            return true;
        }

        bool isArray() const {return variant->typeId() == TypeId::array;}

        bool isObject() const {return variant->typeId() == TypeId::object;}

        bool needsArraySeparator() const
        {
            return isArray() && (iterator.forArray != asArray().begin());
        }

        const Array& asArray() const
        {
            return variant->template as<TypeId::array>();
        }

        const Object& asObject() const
        {
            return variant->template as<TypeId::object>();
        }

        const TVariant* variant = nullptr;
        ArrayOrObjectIterator iterator;
    };

    Sink sink_;
    TEncoder* encoder_ = nullptr;
    std::stack<Context> stack_;
};

//------------------------------------------------------------------------------
template <typename TSink>
class JsonSinkProxy
{
public:
    using value_type = char;

    JsonSinkProxy() = default;

    explicit JsonSinkProxy(TSink& out) : sink_(&out) {}

    void append(const value_type* data, size_t size)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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

    template <typename O>
    explicit JsonEncoderImpl(const O& options)
        : encoder_(Proxy{}, options.template as<jsoncons::json_options>())
    {}

    template <typename TVariant, typename TOutput>
    void encode(const TVariant& variant, TOutput& output)
    {
        // JsonSinkProxy is needed because shared access to the underlying sink
        // by JsonVariantEncodingVisitor is required for WAMP's special
        // handling of Base64-encoded blobs.
        TSink sink(output);
        encoder_.reset(Proxy{sink});
//        internal::JsonVariantEncodingVisitor<Encoder> visitor(Proxy{sink},
//                                                              encoder_);
//        wamp::apply(visitor, variant);
        using JsonVariantEncoderType = internal::JsonVariantEncoder<TVariant,
                                                                    Encoder>;
        JsonVariantEncoderType variantEncoder{Proxy{sink}, encoder_};
        variantEncoder.encode(variant);
    }

private:
    using Proxy = internal::JsonSinkProxy<TSink>;
    using Encoder = jsoncons::basic_compact_json_encoder<char, Proxy>;
    Encoder encoder_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_JSONENCODING_HPP
