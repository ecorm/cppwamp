/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTTRAITS_HPP
#define CPPWAMP_INTERNAL_VARIANTTRAITS_HPP

#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "../blob.hpp"
#include "../null.hpp"
#include "../traits.hpp"
#include "../variantdefs.hpp"

namespace wamp
{

// Forward declaration
class Variant;

namespace internal
{

//------------------------------------------------------------------------------
using ArrayType  = std::vector<Variant>;
using ObjectType = std::map<String, Variant>;

//------------------------------------------------------------------------------
template <TypeId typeId> struct FieldTypeForId {};
template <> struct FieldTypeForId<TypeId::null>    {using Type = Null;};
template <> struct FieldTypeForId<TypeId::boolean> {using Type = Bool;};
template <> struct FieldTypeForId<TypeId::integer> {using Type = Int;};
template <> struct FieldTypeForId<TypeId::uint>    {using Type = UInt;};
template <> struct FieldTypeForId<TypeId::real>    {using Type = Real;};
template <> struct FieldTypeForId<TypeId::string>  {using Type = String;};
template <> struct FieldTypeForId<TypeId::blob>    {using Type = Blob;};
template <> struct FieldTypeForId<TypeId::array>   {using Type = ArrayType;};
template <> struct FieldTypeForId<TypeId::object>  {using Type = ObjectType;};


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

template <> struct FieldTraits<ArrayType>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::array;
    static String typeName()        {return "Array";}
};

template <> struct FieldTraits<ObjectType>
{
    static constexpr bool isValid   = true;
    static constexpr TypeId typeId  = TypeId::object;
    static String typeName()        {return "Object";}
};

//------------------------------------------------------------------------------
template <typename TField, typename Enable = int>
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
struct ArgTraits<TField, Needs<isBoolLike<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Bool";}
    using FieldType                 = Bool;
};

template <typename TField>
struct ArgTraits<TField, Needs<isSignedInteger<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[signed integer]";}
    using FieldType                 = Int;
};

template <typename TField>
struct ArgTraits<TField, Needs<isUnsignedInteger<TField>()>>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[unsigned integer]";}
    using FieldType                 = UInt;
};

template <typename TField>
struct ArgTraits<TField, Needs<std::is_floating_point<TField>::value>>
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

template <> struct ArgTraits<String::value_type*>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <> struct ArgTraits<const String::value_type*>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <size_t N> struct ArgTraits<String::value_type[N]>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "[character array]";}
    using FieldType                 = String;
};

template <size_t N>
struct ArgTraits<const String::value_type[N]>
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

template <> struct ArgTraits<ArrayType>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Array";}
    using FieldType                 = ArrayType;
};

template <typename TElem>
struct ArgTraits<std::vector<TElem>, Needs<!isSameType<TElem,Variant>()>>
{
    static constexpr bool isValid   = ArgTraits<TElem>::isValid;
    static String typeName()        {return "std::vector<" +
                                     ArgTraits<TElem>::typeName() + '>';}
    using FieldType                 = ArrayType;
};

template <> struct ArgTraits<ObjectType>
{
    static constexpr bool isValid   = true;
    static String typeName()        {return "Object";}
    using FieldType                 = ObjectType;
};

template <typename TValue>
struct ArgTraits<std::map<String, TValue>,
                 Needs<!isSameType<TValue, Variant>()>>
{
    static constexpr bool isValid   = ArgTraits<TValue>::isValid;
    static String typeName()        {return "std::map<String, "
                                     + ArgTraits<TValue>::typeName() + '>';}
    using FieldType                 = ObjectType;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTTRAITS_HPP
