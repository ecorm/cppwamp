/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTTRAITS_HPP
#define CPPWAMP_INTERNAL_VARIANTTRAITS_HPP

#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "../traits.hpp"
#include "varianttraitsfwd.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <TypeId typeId> struct FieldTypeForId {};
template <> struct FieldTypeForId<TypeId::null>    {using Type = Null;};
template <> struct FieldTypeForId<TypeId::boolean> {using Type = Bool;};
template <> struct FieldTypeForId<TypeId::integer> {using Type = Int;};
template <> struct FieldTypeForId<TypeId::uint>    {using Type = UInt;};
template <> struct FieldTypeForId<TypeId::real>    {using Type = Real;};
template <> struct FieldTypeForId<TypeId::string>  {using Type = String;};
template <> struct FieldTypeForId<TypeId::blob>    {using Type = Blob;};
template <> struct FieldTypeForId<TypeId::array>   {using Type = Array;};
template <> struct FieldTypeForId<TypeId::object>  {using Type = Object;};


//------------------------------------------------------------------------------
template <typename TField>
struct FieldTraits
{
    static constexpr bool isValid   = false;
    static String typeName()        {return "<unknown>";}
};

template <> struct FieldTraits<Null>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::null;
    static String typeName()        {return "Null";}
};

template <> struct FieldTraits<Bool>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::boolean;
    static String typeName()        {return "Bool";}
};

template <> struct FieldTraits<Int>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::integer;
    static String typeName()        {return "Int";}
};

template <> struct FieldTraits<UInt>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::uint;
    static String typeName()        {return "UInt";}
};

template <> struct FieldTraits<Real>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::real;
    static String typeName()        {return "Real";}
};

template <> struct FieldTraits<String>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::string;
    static String typeName()        {return "String";}
};

template <> struct FieldTraits<Blob>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::blob;
    static String typeName()        {return "Blob";}
};

template <> struct FieldTraits<Array>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::array;
    static String typeName()        {return "Array";}
};

template <> struct FieldTraits<Object>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::object;
    static String typeName()        {return "Object";}
};

//------------------------------------------------------------------------------
template <typename TField, typename Enable>
struct ArgTraits
{
    static constexpr bool isValid   = false;
    static String typeName()        {return "[unknown]";}
};

template <> struct ArgTraits<Null>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Null";}
    using FieldType                 = Null;
};

template <typename TField>
struct ArgTraits<TField, EnableIf<isBool<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Bool";}
    using FieldType                 = Bool;
};

template <typename TField>
struct ArgTraits<TField, EnableIf<isSignedInteger<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[signed integer]";}
    using FieldType                 = Int;
};

template <typename TField>
struct ArgTraits<TField, EnableIf<isUnsignedInteger<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[unsigned integer]";}
    using FieldType                 = UInt;
};

template <typename TField>
struct ArgTraits<TField, EnableIf<std::is_floating_point<TField>::value>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[floating point]";}
    using FieldType                 = Real;
};

template <> struct ArgTraits<String>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "String";}
    using FieldType                 = String;
};

template <> struct ArgTraits<Variant::CharType*>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <> struct ArgTraits<const Variant::CharType*>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <size_t N> struct ArgTraits<Variant::CharType[N]>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <size_t N>
struct ArgTraits<const Variant::CharType[N]>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <> struct ArgTraits<Blob>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Blob";}
    using FieldType                 = Blob;
};

template <> struct ArgTraits<Array>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Array";}
    using FieldType                 = Array;
};

template <typename TElem>
struct ArgTraits<std::vector<TElem>, DisableIf<isSameType<TElem,Variant>()>>
{
    static constexpr bool isValid   = ArgTraits<TElem>::isValid;
    static String typeName()        {return "std::vector<" +
                                     ArgTraits<TElem>::typeName() + '>';}
    using FieldType                 = Array;
};

template <> struct ArgTraits<Object>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Object";}
    using FieldType                 = Object;
};

template <typename TValue>
struct ArgTraits<std::map<String, TValue>,
                 DisableIf<isSameType<TValue, Variant>()>>
{
    static constexpr bool isValid   = ArgTraits<TValue>::isValid;
    static String typeName()        {return "std::map<String, "
                                     + ArgTraits<TValue>::typeName() + '>';}
    using FieldType                 = Object;
};


//------------------------------------------------------------------------------
template <typename TField> struct Access
{
    template <typename U> static void construct(U&& value, void* field)
        {get(field) = value;}

    static void destruct(void*) {}

    static TField& get(void* field)
        {return *static_cast<TField*>(field);}

    static const TField& get(const void* field)
        {return *static_cast<const TField*>(field);}
};

template <> struct Access<String>
{
    template <typename U> static void construct(U&& value, void* field)
        {new (field) String(std::forward<U>(value));}

    static void destruct(void* field) {get(field).~String();}

    static String& get(void* field)
        {return *static_cast<String*>(field);}

    static const String& get(const void* field)
        {return *static_cast<const String*>(field);}
};

template <> struct Access<Blob>
{
    template <typename U> static void construct(U&& value, void* field)
        {new (field) Blob(std::forward<U>(value));}

    static void destruct(void* field) {get(field).~Blob();}

    static Blob& get(void* field)
        {return *static_cast<Blob*>(field);}

    static const Blob& get(const void* field)
        {return *static_cast<const Blob*>(field);}
};

template <> struct Access<Array>
{
    template <typename U> static void construct(U&& value, void* field)
        {ptr(field) = new Array(std::forward<U>(value));}

    static void destruct(void* field)
    {
        Array*& a = ptr(field);
        delete a;
        a = nullptr;
    }

    static Array& get(void* field) {return *ptr(field);}

    static const Array& get(const void* field)
    {
        const Array* a = Access<const Array*>::get(field);
        return *a;
    }

    static Array*& ptr(void* field) {return Access<Array*>::get(field);}
};

template <> struct Access<Object>
{
    template <typename U> static void construct(U&& value, void* field)
    {
        ptr(field) = new Object(std::forward<U>(value));
    }

    static void destruct(void* field)
    {
        Object*& o = ptr(field);
        delete o;
        o = nullptr;
    }

    static Object& get(void* field) {return *ptr(field);}

    static const Object& get(const void* field)
    {
        const Object* o = Access<const Object*>::get(field);
        return *o;
    }

    static Object*& ptr(void* field) {return Access<Object*>::get(field);}
};


} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTTRAITS_HPP
