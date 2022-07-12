/*------------------------------------------------------------------------------
               Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTDECODING_HPP
#define CPPWAMP_INTERNAL_VARIANTDECODING_HPP

#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/config/version.hpp>
#include <jsoncons/json_visitor.hpp>
#include <jsoncons/json_visitor2.hpp>
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
    void put(T&& value, const Where& where, bool isComposite = false)
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

        Context(Variant& v)
            : variant_(&v)
        {}

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
    void putInteger(T integer, const Where& where)
    {
        put(static_cast<Variant::Int>(integer), where);
    }

    template <typename T>
    void addRoot(T&& value, bool isComposite)
    {
        variant_ = std::forward<T>(value);
        hasRoot_ = true;
        if (isComposite)
            contextStack_.push_back(variant_);
    }

    template <typename T>
    void addArrayElement(T&& value, bool isComposite)
    {
        auto& array = context().variant().template as<TypeId::array>();
        array.push_back(std::forward<T>(value));
        if (isComposite)
            contextStack_.push_back(array.back());
    }

    template <typename T>
    void addObjectElement(T&& value, bool isComposite, const Where& where)
    {
        auto& ctx = context();
        if (!ctx.keyIsDone())
            throw error::Decode(makeErrorMessage("Expected string key", where));

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
            contextStack_.push_back(*newElement);
    }

    Context& context()
    {
        assert(!contextStack_.empty());
        return contextStack_.back();
    }


    // visit overrides common to jsoncons::json_visitor
    // and jsoncons::json_visitor2

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

    bool visit_null(Tag, const Where& where, std::error_code&) override
    {
        put(null, where);
        return true;
    }

    bool visit_bool(bool value, Tag, const Where& where,
                    std::error_code&) override
    {
        put(value, where);
        return true;
    }

    bool visit_byte_string(const ByteStringView& bsv, Tag, const Where& where,
                           std::error_code&) override
    {
        put(Blob(Blob::Data(bsv.begin(), bsv.end())), where);
        return true;
    }

    bool visit_uint64(uint64_t n, Tag, const Where& where,
                      std::error_code&) override
    {
        if (n <= std::numeric_limits<Variant::Int>::max())
            putInteger(n, where);
        else
            put(n, where);
        return true;
    }

    bool visit_int64(int64_t n, Tag, const Where& where,
                     std::error_code&) override
    {
        putInteger(n, where);
        return true;
    }

    bool visit_double(double x, Tag, const Where& where,
                      std::error_code&) override
    {
        put(x, where);
        return true;
    }

    std::vector<Context> contextStack_;
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
                      std::error_code&) override
    {
        if ( (sv.size() > 0) && (sv[0] == '\0') )
        {
            Blob::Data bytes;
            Base64::decode(sv.data() + 1, sv.size() - 1, bytes);
            put(Blob(std::move(bytes)), where);
        }
        else
            put(String(sv.data(), sv.size()), where);
        return true;
    }
};

//------------------------------------------------------------------------------
class VariantDecodingVisitor :
    public VariantDecodingVisitorBase<jsoncons::json_visitor2>
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
template <typename TConfig>
class GenericDecoder
{
public:
    template <typename... TArgs>
    explicit GenericDecoder(std::string codecName, TArgs&&... inputStubArgs)
        : inputStub_(std::forward<TArgs>(inputStubArgs)...),
          parser_(inputStub_),
          codecName_(std::move(codecName))
    {}

    template <typename TSourceable>
    void decode(TSourceable&& input, Variant& variant)
    {
        Source source(std::forward<TSourceable>(input));
        parser_.reset(std::move(source));
        visitor_.reset();
        std::error_code ec;
        parser_.parse(visitor_, ec);

        if (ec)
        {
            std::string msg = codecName_;
            msg += " parsing failure at position ";
            msg += std::to_string(parser_.column());
            msg += ": ";
            msg += ec.message();
            reset();
            throw error::Decode(msg);
        }

        variant = std::move(visitor_).variant();
        reset();
    }

private:
    using Input = typename TConfig::Input;
    using Source = typename TConfig::Source;
    using Parser = typename TConfig::Parser;
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
