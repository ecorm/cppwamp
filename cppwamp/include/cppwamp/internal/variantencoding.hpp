/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTENCODING_HPP
#define CPPWAMP_INTERNAL_VARIANTENCODING_HPP

#include <deque>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/sink.hpp>
#include "../codec.hpp"
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct GenericEncoderContext
{
    struct ArrayTag {};
    struct ObjectTag {};

    explicit GenericEncoderContext(const Variant* var) : variant_(var) {}

    explicit GenericEncoderContext(const Variant* var, ArrayTag)
        : variant_(var)
    {
        iterator_.forArray = asArray().begin();
    }

    explicit GenericEncoderContext(const Variant* var, ObjectTag)
        : variant_(var)
    {
        iterator_.forObject = asObject().begin();
    }

    template <typename E>
    const Variant* next(E& encoder)
    {
        const Variant* result = nullptr;

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
    using Array = std::vector<Variant>;
    using Object = std::map<String, Variant>;

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

    const Variant* variant_ = nullptr;
    ArrayOrObjectIterator iterator_;
};

//------------------------------------------------------------------------------
template <typename TSink>
struct GenericEncoderSinkTraits {};

template <>
struct GenericEncoderSinkTraits<StringSink>
{
    using Sink = jsoncons::string_sink<std::string>;
    using StubArg = std::string;
};

template <>
struct GenericEncoderSinkTraits<BufferSink>
{
    using Sink = jsoncons::string_sink<MessageBuffer>;
    using StubArg = MessageBuffer;
};

template <>
struct GenericEncoderSinkTraits<StreamSink>
{
    using Sink = jsoncons::stream_sink<char>;
    using StubArg = std::nullptr_t;
};

//------------------------------------------------------------------------------
template <typename TConfig>
class GenericEncoder
{
private:
    using SinkTraits = internal::GenericEncoderSinkTraits<typename TConfig::Sink>;
    using Options = typename TConfig::Options;

public:
    using Sink = typename TConfig::Sink;

    GenericEncoder() :
        stub_(typename SinkTraits::StubArg{}),
        encoder_(stub_)
    {}

    template <typename O>
    explicit GenericEncoder(const O& codecOptions) :
        stub_(typename SinkTraits::StubArg{}),
        encoder_(stub_, codecOptions.template as<Options>())
    {}

    void encode(const Variant& v, Sink sink)
    {
        const Variant* variant = &v;
        encoder_.reset(sink.output());
        stack_.clear();

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
                encoder_.string_value(str);
                break;
            }

            case TypeId::blob:
            {
                const auto& bytes =
                    variant->template as<TypeId::blob>().bytes();
                encoder_.byte_string_value(bytes);
                break;
            }

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
    using Output = typename Sink::Output;
    using EncoderSink = typename SinkTraits::Sink;
    using Encoder = typename TConfig::template EncoderType<EncoderSink>;
    using Context = GenericEncoderContext;

    Output stub_;
    Encoder encoder_;
    std::deque<Context> stack_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTENCODING_HPP
