/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_JSONENCODING_HPP
#define CPPWAMP_INTERNAL_JSONENCODING_HPP

#include <deque>
#include <jsoncons/json_encoder.hpp>
#include "../blob.hpp"
#include "../variantdefs.hpp"
#include "base64.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TVariant>
struct JsonVariantEncoderContext
{
    struct ArrayTag {};
    struct ObjectTag {};

    explicit JsonVariantEncoderContext(const TVariant* var) : variant_(var) {}

    explicit JsonVariantEncoderContext(const TVariant* var, ArrayTag)
        : variant_(var)
    {
        iterator_.forArray = asArray().begin();
    }

    explicit JsonVariantEncoderContext(const TVariant* var, ObjectTag)
        : variant_(var)
    {
        iterator_.forObject = asObject().begin();
    }

    bool needsArraySeparator() const
    {
        return isArray() && (iterator_.forArray != asArray().begin());
    }

    template <typename E>
    const TVariant* next(E& encoder)
    {
        const TVariant* result = nullptr;

        if (isObject())
        {
            if (iterator_.forObject == asObject().end())
            {
                encoder.end_object();
                return nullptr;
            }

            const auto& key = iterator_.forObject->first;
            encoder.key({key.data(), key.size()});
            result = &(iterator_.forObject->second);
            ++iterator_.forObject;
        }
        else if (isArray())
        {
            if (iterator_.forArray == asArray().end())
            {
                encoder.end_array();
                return nullptr;
            }

            result = &(*iterator_.forArray);
            ++iterator_.forArray;
        }
        else
        {
            result = variant_;
        }

        return result;
    }

private:
    using Array = std::vector<TVariant>;
    using Object = std::map<String, TVariant>;

    union ArrayOrObjectIterator
    {
        ArrayOrObjectIterator() {}
        typename Array::const_iterator forArray;
        typename Object::const_iterator forObject;
    };

    bool isArray() const {return variant_->typeId() == TypeId::array;}

    bool isObject() const {return variant_->typeId() == TypeId::object;}

    const Array& asArray() const
    {
        return variant_->template as<TypeId::array>();
    }

    const Object& asObject() const
    {
        return variant_->template as<TypeId::object>();
    }

    const TVariant* variant_ = nullptr;
    ArrayOrObjectIterator iterator_;
};

//------------------------------------------------------------------------------
template <typename TVariant, typename TEncoder>
class JsonVariantEncoder
{
public:
    using Sink = typename TEncoder::sink_type;
    using Result = void;

    JsonVariantEncoder() : encoder_(Sink{}) {}

    template <typename O>
    JsonVariantEncoder(O&& options)
        : encoder_(Sink{}, std::forward<O>(options))
    {}

    void reset(Sink sink)
    {
        sink_ = sink;
        encoder_.reset(std::move(sink));
        stack_.clear();
    }

    void encode(const TVariant& v)
    {
        assert(stack_.empty());
        const TVariant* variant = &v;

        while (true)
        {
            if (!stack_.empty())
            {
                variant = stack_.back().next(encoder_);
                if (variant == nullptr)
                {
                    stack_.pop_back();
                    if (stack_.empty())
                        break;
                    continue;
                }
            }

            switch (variant->typeId())
            {
            case TypeId::null:
                encoder_.null_value();
                break;

            case TypeId::boolean:
                encoder_.bool_value(variant->template as<TypeId::boolean>());
                break;

            case TypeId::integer:
                encoder_.int64_value(variant->template as<TypeId::integer>());
                break;

            case TypeId::uint:
                encoder_.uint64_value(variant->template as<TypeId::uint>());
                break;

            case TypeId::real:
                encoder_.double_value(variant->template as<TypeId::real>());
                break;

            case TypeId::string:
            {
                const auto& str = variant->template as<TypeId::string>();
                encoder_.string_value({str.data(), str.size()});
                break;
            }

            case TypeId::blob:
                encodeBlob(variant->template as<TypeId::blob>());
                break;

            case TypeId::array:
            {
                const auto& array = variant->template as<TypeId::array>();
                encoder_.begin_array(array.size());
                stack_.emplace_back(Context{variant,
                                            typename Context::ArrayTag{}});
                break;
            }

            case TypeId::object:
            {
                const auto& object = variant->template as<TypeId::object>();
                encoder_.begin_object(object.size());
                stack_.emplace_back(Context{variant,
                                            typename Context::ObjectTag{}});
                break;
            }
            }

            if (stack_.empty())
                break;
        }
    }

    void encodeBlob(const Blob& b)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
        static constexpr char prefix[] = "\"\\u0000";
        if (!stack_.empty() && stack_.back().needsArraySeparator())
            sink_.push_back(',');
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto data = reinterpret_cast<const typename Sink::value_type*>(prefix);
        sink_.append(data, sizeof(prefix) - 1);
        Base64::encode(b.data().data(), b.data().size(), sink_);
        sink_.push_back('\"');
    }

private:
    using Context = JsonVariantEncoderContext<TVariant>;

    Sink sink_;
    TEncoder encoder_;
    std::deque<Context> stack_;
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
template <typename TSink, typename TVariant>
class JsonEncoderImpl
{
public:
    JsonEncoderImpl() = default;

    template <typename O>
    explicit JsonEncoderImpl(const O& options)
        : encoder_(options.template as<jsoncons::json_options>())
    {}

    template <typename TOutput>
    void encode(const TVariant& variant, TOutput& output)
    {
        // JsonSinkProxy is needed because shared access to the underlying sink
        // by JsonVariantEncoder is required for WAMP's special handling of
        // Base64-encoded blobs.
        TSink sink(output);
        encoder_.reset(Proxy{sink});
        encoder_.encode(variant);
    }

private:
    using Proxy = internal::JsonSinkProxy<TSink>;
    using UnderlyingEncoder = jsoncons::basic_compact_json_encoder<char, Proxy>;
    using EncoderType = internal::JsonVariantEncoder<TVariant,
                                                     UnderlyingEncoder>;
    EncoderType encoder_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_JSONENCODING_HPP
