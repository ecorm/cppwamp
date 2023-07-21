/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTDECODING_HPP
#define CPPWAMP_INTERNAL_VARIANTDECODING_HPP

#include <cassert>
#include <cstdint>
#include <deque>
#include <limits>
#include <utility>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/config/version.hpp>
#include <jsoncons/item_event_visitor.hpp>
#include <jsoncons/ser_context.hpp>
#include <jsoncons/source.hpp>
#include <jsoncons/tag_type.hpp>
#include "../codec.hpp"
#include "../variant.hpp"
#include "base64.hpp"

#define CPPWAMP_JSONCONS_VERSION \
    (JSONCONS_VERSION_MAJOR * 10000000u + \
     JSONCONS_VERSION_MINOR * 1000u + \
     JSONCONS_VERSION_PATCH)

#define CPPWAMP_JSONCONS_CURSOR_PROVIDES_SIZE (CPPWAMP_JSONCONS_VERSION > 168007)

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TVisitor>
class VariantDecodingVisitorBase : public TVisitor
{
public:
    using string_view_type = typename TVisitor::string_view_type;

    void reset()
    {
        contextStack_.clear();
        key_.clear();
        variant_ = Variant();
        hasRoot_ = false;
    }

    const Variant& variant() const & {return variant_;}

    Variant&& variant() && {return std::move(variant_);}

    bool empty() const {return !hasRoot_;}

protected:
    using Tag = jsoncons::semantic_tag;
    using Where = jsoncons::ser_context;

    template <typename T>
    std::error_code put(T&& value, const Where& where, bool isComposite = false)
    {
        if (contextStack_.empty())
            return addRoot(std::forward<T>(value), isComposite);

        switch (context().variant().typeId())
        {
        case TypeId::array:
            return addArrayElement(std::forward<T>(value), isComposite);
        case TypeId::object:
            return addObjectElement(std::forward<T>(value), isComposite, where);
        default:
            assert(false);
            break;
        }

        return {};
    }

    void putKey(String&& key)
    {
        key_ = std::move(key);
        context().setKeyIsDone();
    }

    void putStringOrKey(String&& str, const Where& where)
    {
        if (!contextStack_.empty() && context().expectsKey())
            putKey(std::move(str));
        else
            put(std::move(str), where);
    }

    void endComposite()
    {
        assert(!contextStack_.empty());
        contextStack_.pop_back();
    }

private:
    using Base = TVisitor;
    using ByteStringView = jsoncons::byte_string_view;

    class Context
    {
    public:
        Context() = default;

        explicit Context(Variant& v) : variant_(&v) {}

        Variant& variant() {return *variant_;}

        bool expectsKey() const
        {
            return variant_->is<Object>() && !keyIsDone_;
        }

        bool keyIsDone() const {return keyIsDone_;}

        void setKeyIsDone(bool done = true) {keyIsDone_ = done;}

    private:
        Variant* variant_ = nullptr;
        bool keyIsDone_ = false;
    };

    static std::string makeErrorMessage(std::string what, const Where& where)
    {
        what += " at column ";
        what += std::to_string(where.column());
        what += ", line ";
        what += std::to_string(where.line());
        what += ", position ";
        what += std::to_string(where.position());
        return what;
    }

    template <typename T>
    std::error_code putInteger(T integer, const Where& where)
    {
        return put(static_cast<Variant::Int>(integer), where);
    }

    template <typename T>
    std::error_code addRoot(T&& value, bool isComposite)
    {
        variant_ = std::forward<T>(value);
        hasRoot_ = true;
        if (isComposite)
            contextStack_.push_back(Context{variant_});
        return {};
    }

    template <typename T>
    std::error_code addArrayElement(T&& value, bool isComposite)
    {
        auto& array = context().variant().template as<TypeId::array>();
        array.push_back(std::forward<T>(value));
        if (isComposite)
            contextStack_.push_back(Context{array.back()});
        return {};
    }

    template <typename T>
    std::error_code addObjectElement(T&& value, bool isComposite, const Where&)
    {
        auto& ctx = context();
        if (!ctx.keyIsDone())
            return make_error_code(DecodingErrc::expectedStringKey);

        Variant* newElement = nullptr;
        auto& object = ctx.variant().template as<TypeId::object>();
        auto found = object.find(key_);
        if (found == object.end())
        {
            auto result = object.emplace(std::move(key_),
                                         std::forward<T>(value));
            newElement = &(result.first->second);
        }
        else
        {
            found->second = std::forward<T>(value);
            newElement = &(found->second);
        }
        ctx.setKeyIsDone(false);
        if (isComposite)
            contextStack_.push_back(Context{*newElement});

        return {};
    }

    Context& context()
    {
        assert(!contextStack_.empty());
        return contextStack_.back();
    }


    // visit overrides common to jsoncons::json_visitor
    // and jsoncons::item_event_visitor

    void visit_flush() override {}

    bool visit_begin_object(Tag, const Where& where, std::error_code&) override
    {
        put(Object{}, where, true);
        return true;
    }

    bool visit_end_object(const Where&, std::error_code&) override
    {
        endComposite();
        return true;
    }

    bool visit_begin_array(Tag, const Where& where, std::error_code&) override
    {
        put(Array{}, where, true);
        return true;
    }

    bool visit_begin_array(std::size_t length, Tag, const Where& where,
                           std::error_code&) override
    {
        Array a;
        if (length > 0)
            a.reserve(length);
        put(std::move(a), where, true);
        return true;
    }

    bool visit_end_array(const Where&, std::error_code&) override
    {
        endComposite();
        return true;
    }

    bool visit_null(Tag, const Where& where, std::error_code& ec) override
    {
        ec = put(null, where);
        return !ec;
    }

    bool visit_bool(bool value, Tag, const Where& where,
                    std::error_code& ec) override
    {
        ec = put(value, where);
        return !ec;
    }

    bool visit_byte_string(const ByteStringView& bsv, Tag, const Where& where,
                           std::error_code& ec) override
    {
        ec = put(Blob(Blob::Bytes(bsv.begin(), bsv.end())), where);
        return !ec;
    }

    bool visit_uint64(uint64_t n, Tag, const Where& where,
                      std::error_code& ec) override
    {
        if (n <= std::numeric_limits<Variant::Int>::max())
            ec = putInteger(n, where);
        else
            ec = put(n, where);
        return !ec;
    }

    bool visit_int64(int64_t n, Tag, const Where& where,
                     std::error_code& ec) override
    {
        ec = putInteger(n, where);
        return !ec;
    }

    bool visit_double(double x, Tag, const Where& where,
                      std::error_code& ec) override
    {
        ec = put(x, where);
        return !ec;
    }

    std::deque<Context> contextStack_;
    String key_;
    Variant variant_;
    bool hasRoot_ = false;
};

//------------------------------------------------------------------------------
class VariantJsonDecodingVisitor :
    public VariantDecodingVisitorBase<jsoncons::json_visitor>
{
private:
    bool visit_key(const string_view_type& name, const Where&,
                   std::error_code&) override
    {
        putKey(String(name.data(), name.size()));
        return true;
    }

    bool visit_string(const string_view_type& sv, Tag, const Where& where,
                      std::error_code& ec) override
    {
        if ( !sv.empty() && (sv[0] == '\0') )
        {
            Blob::Bytes bytes;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            ec = Base64::decode(sv.data() + 1, sv.size() - 1, bytes);
            if (!ec)
                put(Blob(std::move(bytes)), where);
        }
        else
            put(String(sv.data(), sv.size()), where);
        return true;
    }
};

//------------------------------------------------------------------------------
class VariantDecodingVisitor :
    public VariantDecodingVisitorBase<jsoncons::item_event_visitor>
{
private:
    bool visit_string(const string_view_type& sv, Tag, const Where& where,
                      std::error_code&) override
    {
        putStringOrKey(String(sv.data(), sv.size()), where);
        return true;
    }
};

//------------------------------------------------------------------------------
template <typename TSource>
struct GenericDecoderSourceTraits {};

template <>
struct GenericDecoderSourceTraits<StringSource>
{
    using Source = jsoncons::bytes_source;
    using StubArg = std::string;
};

template <>
struct GenericDecoderSourceTraits<BufferSource>
{
    using Source = jsoncons::bytes_source;
    using StubArg = MessageBuffer;
};

template <>
struct GenericDecoderSourceTraits<StreamSource>
{
    using Source = jsoncons::stream_source<uint8_t>;
    using StubArg = std::nullptr_t;
};

//------------------------------------------------------------------------------
template <typename TConfig>
class GenericDecoder
{
private:
    using SourceTraits = GenericDecoderSourceTraits<typename TConfig::Source>;
    using Options = typename TConfig::Options;

public:
    explicit GenericDecoder(std::string codecName)
        : inputStub_(typename SourceTraits::StubArg{}),
          parser_(inputStub_),
          codecName_(std::move(codecName))
    {}

    template <typename O>
    GenericDecoder(std::string codecName, const O& codecOptions)
        : inputStub_(typename SourceTraits::StubArg{}),
        parser_(inputStub_, codecOptions.template as<Options>()),
          codecName_(std::move(codecName))
    {}

    template <typename TSourceable>
    std::error_code decode(TSourceable&& input, Variant& variant)
    {
        ParserSource source(std::forward<TSourceable>(input));
        parser_.reset(std::move(source));
        visitor_.reset();
        std::error_code ec;
        parser_.parse(visitor_, ec);
        if (!ec)
            variant = std::move(visitor_).variant();
        reset();
        return ec;
    }

private:
    using ParserSource = typename SourceTraits::Source;
    using Source = typename TConfig::Source;
    using Input = typename Source::Input;
    using Parser = typename TConfig::template Parser<ParserSource>;
    using Visitor = internal::VariantDecodingVisitor;

    void reset()
    {
        parser_.reset();
        visitor_.reset();
    }

    Input inputStub_;
    Parser parser_;
    Visitor visitor_;
    std::string codecName_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTDECODING_HPP
