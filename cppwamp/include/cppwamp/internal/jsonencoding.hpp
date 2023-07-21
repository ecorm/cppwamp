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
struct JsonEncoderContext
{
    struct ArrayTag {};
    struct ObjectTag {};

    explicit JsonEncoderContext(const TVariant* var) : variant_(var) {}

    explicit JsonEncoderContext(const TVariant* var, ArrayTag)
        : variant_(var)
    {
        iterator_.forArray = asArray().begin();
    }

    explicit JsonEncoderContext(const TVariant* var, ObjectTag)
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
    using Sink = TSink;

    JsonEncoderImpl() : encoder_(Proxy{}) {}

    template <typename O>
    explicit JsonEncoderImpl(const O& options)
        : encoder_(Proxy{}, options.template as<jsoncons::json_options>())
    {}

    template <typename TOutput>
    void encode(const TVariant& v, TOutput& output)
    {
        // JsonSinkProxy is needed because shared access to the underlying sink
        // by JsonVariantEncoder is required for WAMP's special handling of
        // Base64-encoded blobs.
        Sink outputSink{output};
        Proxy sink(outputSink);
        encoder_.reset(Proxy{sink});
        stack_.clear();
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
                encodeBlob(variant->template as<TypeId::blob>(), sink);
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

private:
    using Proxy = internal::JsonSinkProxy<Sink>;
    using EncoderType = jsoncons::basic_compact_json_encoder<char, Proxy>;
    using Context = JsonEncoderContext<TVariant>;

    void encodeBlob(const Blob& b, Proxy& sink)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
        static constexpr char prefix[] = "\"\\u0000";
        if (!stack_.empty() && stack_.back().needsArraySeparator())
            sink.push_back(',');
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        sink.append(prefix, sizeof(prefix) - 1);
        Base64::encode(b.data().data(), b.data().size(), sink);
        sink.push_back('\"');
    }

    EncoderType encoder_;
    std::deque<Context> stack_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_JSONENCODING_HPP
