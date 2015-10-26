/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTBUILDER_HPP
#define CPPWAMP_INTERNAL_VARIANTBUILDER_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stack>
#include <utility>
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class VariantBuilder
{
public:
    using SizeType = unsigned;

    explicit VariantBuilder(Variant& variant)
        : v_(variant)
    {
        v_ = null;
        stack_.push(&v_);
    }

    bool Null()             {return put(null);}
    bool Bool(bool b)       {return put(b);}
    bool Int(int n)         {return putInteger(n);}
    bool Uint(unsigned n)   {return putInteger(n);}
    bool Int64(int64_t n)   {return putInteger(n);}
    bool Double(double x)   {return put(x);}

    bool Uint64(uint64_t n)
    {
        if (n <= std::numeric_limits<Variant::Int>::max())
            return putInteger(n);
        else
            return put(n);
    }

    bool String(const char* str, SizeType length, bool /*copy*/)
        {return put(VString(str, length));}

    bool Bin(const char* data, SizeType length)
    {
        Blob::Data bytes;
        bytes.reserve(length);
        std::copy(data, data + length, std::back_inserter(bytes));
        return put(Blob(std::move(bytes)));
    }

    bool Bin(Blob::Data&& data)
    {
        return put(Blob(std::move(data)));
    }

    bool StartObject()
    {
        Object object;
        auto ptr = put(std::move(object));
        if (ptr)
            stack_.push(ptr);
        return ptr;
    }

    bool Key(const char* str, SizeType length, bool /*copy*/)
    {
        key_ = VString(str, length);
        return true;
    }

    bool EndObject(SizeType memberCount)
    {
        assert( top().template as<TypeId::object>().size() == memberCount );
        stack_.pop();
        return true;
    }

    bool StartArray(SizeType elementCount = 0)
    {
        Array array;
        if (elementCount != 0)
            array.reserve(elementCount);
        auto ptr = put(std::move(array));
        if (ptr)
            stack_.push(ptr);
        return ptr;
    }

    bool EndArray(SizeType elementCount)
    {
        assert( top().template as<TypeId::array>().size() == elementCount );
        stack_.pop();
        return true;
    }

private:
    using VString = typename Variant::String;

    template <typename T> Variant* put(T&& value)
    {
        auto& v = top();
        Variant* ptr = &v;
        switch (v.typeId())
        {
            case TypeId::null:
                v = std::forward<T>(value);
                break;

            case TypeId::array:
            {
                auto& array = v.template as<TypeId::array>();
                array.push_back(std::forward<T>(value));
                ptr = &array.back();
                break;
            }

            case TypeId::object:
            {
                auto& object = v.template as<TypeId::object>();
                auto result = object.emplace(std::move(key_),
                                             std::forward<T>(value));
                ptr = result.second ? &(result.first->second) : nullptr;
                break;
            }

            default:
                assert(false);
        }
        return ptr;
    }

    template <typename T> Variant* putInteger(T integer)
    {
        return put(static_cast<Variant::Int>(integer));
    }

    Variant& top() {return *stack_.top();}

    Variant& v_;
    std::stack<Variant*> stack_;
    VString key_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTBUILDER_HPP
