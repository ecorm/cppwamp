/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

#include "../conversion.hpp"
#include "varianttraits.hpp"
#include "variantvisitors.hpp"

namespace wamp
{

namespace error
{

//------------------------------------------------------------------------------
inline BadType::BadType(const std::string& what)
    : std::runtime_error(what)
{}

//------------------------------------------------------------------------------
inline Access::Access(const std::string& from, const std::string& to)
    : BadType("wamp::error::Access: "
        "Attemping to access field type " + from + " as " + to)
{}

//------------------------------------------------------------------------------
inline Conversion::Conversion(const std::string& what)
    : BadType("wamp::error::Conversion: " + what)
{}

} // namespace error

//------------------------------------------------------------------------------
template <typename TValue>
Variant Variant::from(TValue&& value)
{
    return convertFrom(std::forward<TValue>(value));
}

//------------------------------------------------------------------------------
inline Variant::Variant() noexcept : typeId_(TypeId::null) {}

//------------------------------------------------------------------------------
inline Variant::Variant(const Variant& other)
    : typeId_(other.typeId_)
{
    apply(Construct(*this), other);
}

//------------------------------------------------------------------------------
inline Variant::Variant(Variant&& other) noexcept
    : typeId_(other.typeId_)
{
    apply(MoveConstruct(*this), other);
    other = null;
}

//------------------------------------------------------------------------------
/** @details
    The @ref EnableIfValidArg metafunction is used to disable this function for
    invalid value types. The second template parameter should not specified.
    @see Variant::from
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `*this == value` */
//------------------------------------------------------------------------------
template <typename T, Variant::EnableIfValidArg<T>>
Variant::Variant(T value)
{
    static_assert(ArgTraits<T>::isValid, "Invalid argument type");
    using FieldType = typename ArgTraits<T>::FieldType;
    typeId_ = FieldTraits<FieldType>::typeId;
    constructAs<FieldType>(std::move(value));
}

//------------------------------------------------------------------------------
/** @post `this->is<Array>() == true`
    @post `*this == array` */
//------------------------------------------------------------------------------
inline Variant::Variant(Array array)
    : typeId_(TypeId::array)
{
    constructAs<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @pre The vector elements must be convertible to a bound type
         (checked at compile time).
    @post `this->is<Array>() == true`
    @post `*this == vec` */
//------------------------------------------------------------------------------
template <typename T> Variant::Variant(std::vector<T> vec)
    : typeId_(TypeId::array)
{
    static_assert(ArgTraits<T>::isValid, "Invalid vector element type");
    Array array;
    array.reserve(vec.size());
    std::move(vec.begin(), vec.end(), std::back_inserter(array));
    constructAs<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @post `this->is<Object>() == true`
    @post `*this == object` */
//------------------------------------------------------------------------------
inline Variant::Variant(Object object)
    : typeId_(TypeId::object)
{
    constructAs<Object>(std::move(object));
}

//------------------------------------------------------------------------------
/** @post `this->is<Object>() == true`
    @post `*this == map`
    @pre The map values must be convertible to bound types
         (checked at compile time). */
//------------------------------------------------------------------------------
template <typename T> Variant::Variant(std::map<String, T> map)
    : typeId_(TypeId::object)
{
    static_assert(ArgTraits<T>::isValid, "Invalid map value type");
    Object object;
    std::move(map.begin(), map.end(), std::inserter(object, object.begin()));
    constructAs<Object>(std::move(object));
}

//------------------------------------------------------------------------------
inline Variant::~Variant() {*this = null;}

//------------------------------------------------------------------------------
inline TypeId Variant::typeId() const {return typeId_;}

//------------------------------------------------------------------------------
inline Variant::operator bool() const {return typeId_ != TypeId::null;}

//------------------------------------------------------------------------------
template <typename TBound> bool Variant::is() const
{
    static_assert(FieldTraits<TBound>::isValid, "Invalid field type");
    return typeId_ == FieldTraits<TBound>::typeId;
}

//------------------------------------------------------------------------------
template <TypeId id> bool Variant::is() const
{
    return is<BoundTypeForId<id>>();
}

//------------------------------------------------------------------------------
/** @details
    The variant is deemed convertible to the target type according to the
    following table:
| Target,  Variant->    | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|-----------------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null                  | true  | false | false | false | false | false  | false | false  |
| Bool                  | false | true  | true  | true  | true  | false  | false | false  |
| _integer type_        | false | true  | true  | true  | true  | false  | false | false  |
| _floating point type_ | false | true  | true  | true  | true  | false  | false | false  |
| String                | false | false | false | false | false | true   | false | false  |
| Array                 | false | false | false | false | false | false  | true  | false  |
| std::vector<T>        | false | false | false | false | false | false  | maybe | false  |
| Object                | false | false | false | false | false | false  | false | true   |
| std::map<String,T>    | false | false | false | false | false | false  | false | maybe  |

    An `Array` is convertible to `std::vector<T>` iff all `Array` elements are
    convertible to `T`.

    An `Object` is convertible to `std::map<String,T>` iff all Object values
    are convertible to `T`.

    @tparam T The target type to convert to. Must be default-constructable.
            If T's default constructor is private, then T must grant friendship
            to wamp::ConversionAccess.
    @return The converted value.
    @pre The variant is convertible to the destination type.
    @throws error::Conversion if the variant is not convertible to
            the destination type. */
//------------------------------------------------------------------------------
template <typename T> T Variant::to() const
{
    T result(std::move(ConversionAccess::defaultConstruct<T>()));
    convertTo(result);
    return result;
}

//------------------------------------------------------------------------------
/** @tparam T The target type to convert to.
    @pre The variant is convertible to the destination type.
    @throws error::Conversion if the variant is not convertible to
            the destination type. */
//------------------------------------------------------------------------------
template <typename T> void Variant::to(T& value) const
{
    convertTo(value);
}

//------------------------------------------------------------------------------
/** @tparam T The target type of the result.
    @pre The variant is null, or is convertible to the destination type.
    @throws error::Conversion if the variant is not null and is not convertible
            to the destination type. */
//------------------------------------------------------------------------------
template <typename T>
ValueTypeOf<T> Variant::valueOr(T&& fallback) const
{
    if (!*this)
        return std::forward<T>(fallback);
    else
        return this->to< ValueTypeOf<T> >();
}

//------------------------------------------------------------------------------
/** @returns `0` if the variant is null
    @returns `1` if the variant is a boolean, number, or string
    @returns The number of elements if the variant is an array
    @returns The number of members if the variant is an object */
//------------------------------------------------------------------------------
inline Variant::SizeType Variant::size() const
{
    return apply(ElementCount(), *this);
}

//------------------------------------------------------------------------------
/** @tparam TBound The bound type under which to interpret the variant.
    @pre `TBound` must match the variant's current bound type.
    @throws error::Access if `TBound` does not match the variant's
            current bound type. */
//------------------------------------------------------------------------------
template <typename TBound> TBound& Variant::as()
{
    return get<TBound>(*this);
}

//------------------------------------------------------------------------------
/** @tparam TBound The bound type under which to interpret the variant.
    @pre `TBound` must match the variant's current bound type.
    @throws error::Access if `TBound` does not match the variant's
            current bound type. */
//------------------------------------------------------------------------------
template <typename TBound> const TBound& Variant::as() const
{
    return get<const TBound>(*this);
}

//------------------------------------------------------------------------------
/** @tparam id The bound type id under which to interpret the variant.
    @pre `this->typeId() == id`
    @throws error::Access if `this->typeId() != id`. */
//------------------------------------------------------------------------------
template <TypeId id>
Variant::BoundTypeForId<id>& Variant::as()
{
    return as<BoundTypeForId<id>>();
}

//------------------------------------------------------------------------------
/** @tparam id The bound type id under which to interpret the variant.
    @pre `this->typeId() == id`
    @throws error::Access if `this->typeId() != id`. */
//------------------------------------------------------------------------------
template <TypeId id>
const Variant::BoundTypeForId<id>& Variant::as() const
{
    return as<BoundTypeForId<id>>();
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
inline Variant& Variant::operator[](SizeType index)
{
    return at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
inline const Variant& Variant::operator[](SizeType index) const
{
    return at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
inline Variant& Variant::at(SizeType index)
{
    return as<Array>().at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
inline const Variant& Variant::at(SizeType index) const
{
    return as<Array>().at(index);
}

//------------------------------------------------------------------------------
/** @details
    If there is no element under the given key, a null variant will be
    automatically inserted under that key before the reference is returned.
    @pre `this->is<Object>() == true`
    @throws error::Access if `this->is<Object>() == false` */
//------------------------------------------------------------------------------
inline Variant& Variant::operator[](const String& key)
{
    return as<Object>()[key];
}

//------------------------------------------------------------------------------
/** @pre `this->is<Object>() == true`
    @pre `this->as<Object>().count(key) > 0`
    @throws error::Access if `this->is<Object>() == false`
    @throws std::out_of_range if the key does not exist. */
//------------------------------------------------------------------------------
inline Variant& Variant::at(const String& key)
{
    return as<Object>().at(key);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Object>() == true`
    @pre `this->as<Object>().count(key) > 0`
    @throws error::Access if `this->is<Object>() == false`
    @throws std::out_of_range if the key does not exist. */
//------------------------------------------------------------------------------
inline const Variant& Variant::at(const String& key) const
{
    return as<Object>().at(key);
}

//------------------------------------------------------------------------------
/** @details
    The comparison is analogous to the Javascript `===` operator, and is
    performed according to the following matrix:

| LHS, RHS-> | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null       | true  | false | false | false | false | false  | false | false  |
| Bool       | false | L==R  | false | false | false | false  | false | false  |
| Int        | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| UInt       | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| Real       | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| String     | false | false | false | false | false | L==R   | false | false  |
| Array      | false | false | false | false | false | false  | L==R  | false  |
| Object     | false | false | false | false | false | false  | false | L==R   | */
//------------------------------------------------------------------------------
inline bool Variant::operator==(const Variant& other) const
{
    return apply(internal::EquivalentTo(), *this, other);
}

//------------------------------------------------------------------------------
/** @details
    The result is equivalant to `!(lhs == rhs)`
    @see Variant::operator== */
//------------------------------------------------------------------------------
inline bool Variant::operator!=(const Variant& other) const
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
/** @details
    This operator is provided to allow the use of variants in associative
    containers. The comparison is performed according to the following
    matrix:

| LHS, RHS-> | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null       | false | false | false | false | false | false  | false | false  |
| Bool       | false | L<R   | false | false | false | false  | false | false  |
| Int        | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| UInt       | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| Real       | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| String     | false | false | false | false | false | L<R    | false | false  |
| Array      | false | false | false | false | false | false  | L<R   | false  |
| Object     | false | false | false | false | false | false  | false | L<R    | */
//------------------------------------------------------------------------------
inline bool Variant::operator<(const Variant &other) const
{
    return apply(LessThan(), *this, other);
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == other.typeId()`
    @post `*this == other` */
//------------------------------------------------------------------------------
inline Variant& Variant::operator=(const Variant& other)
{
    if (&other != this)
        Variant(other).swap(*this);
    return *this;
}

//------------------------------------------------------------------------------
/** @post `other == null` */
//------------------------------------------------------------------------------
inline Variant& Variant::operator=(Variant&& other) noexcept
{
    if (&other != this)
    {
        apply(MoveAssign(*this), *this, other);
        other = null;
    }
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The variant's dynamic type will change to accomodate the assigned
    value.
    @pre The value must be convertible to a bound type
         (checked at compile time).
    @post `*this == value` */
//------------------------------------------------------------------------------
template <typename T> Variant& Variant::operator=(T value)
{
    static_assert(ArgTraits<T>::isValid, "Invalid argument type");

    using FieldType = typename ArgTraits<T>::FieldType;
    if (is<FieldType>())
        as<FieldType>() = std::move(value);
    else
    {
        destruct();
        constructAs<FieldType>(std::move(value));
        typeId_ = FieldTraits<FieldType>::typeId;
    }
    return *this;
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == TypeId::array`
    @post `*this == array` */
//------------------------------------------------------------------------------
inline Variant& Variant::operator=(Array array)
{
    return this->operator=<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @pre The vector elements must be convertible to a bound type
         (checked at compile time).
    @post `this->typeId() == TypeId::array`
    @post `*this == vec` */
//------------------------------------------------------------------------------
template <typename T>
Variant& Variant::operator=(std::vector<T> vec)
{
    static_assert(ArgTraits<T>::isValid, "Invalid vector element type");

    Array array;
    array.reserve(vec.size());
    std::move(vec.begin(), vec.end(), std::back_inserter(array));
    return *this = std::move(array);
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == TypeId::object`
    @post `*this == object` */
//------------------------------------------------------------------------------
inline Variant& Variant::operator=(Object object)
{
    return this->operator=<Object>(std::move(object));
}

//------------------------------------------------------------------------------
/** @pre The map values must be convertible to a bound type
         (checked at compile time).
    @post `this->typeId() == TypeId::object`
    @post `*this == map` */
//------------------------------------------------------------------------------
template <typename T>
Variant& Variant::operator=(std::map<String,T> map)
{
    static_assert(ArgTraits<T>::isValid, "Invalid map value type");
    Object object(map.cbegin(), map.cend());
    return *this = std::move(object);
}

//------------------------------------------------------------------------------
/** @see void swap(Variant& v, Variant& w) noexcept */
//------------------------------------------------------------------------------
inline void Variant::swap(Variant &other) noexcept
{
    if (&other != this)
        apply(Swap(*this, other), *this, other);
}

//------------------------------------------------------------------------------
template <typename TField, typename TArg>
void Variant::constructAs(TArg&& value)
{
    Access<TField>::construct(std::forward<TArg>(value), &field_);
}

//------------------------------------------------------------------------------
template <typename TField> void Variant::destructAs()
{
    Access<TField>::destruct(&field_);
}

//------------------------------------------------------------------------------
inline void Variant::destruct() {apply(Destruct(&field_), *this);}

//------------------------------------------------------------------------------
template <typename T, Variant::EnableIfValidArg<ValueTypeOf<T>>>
Variant Variant::convertFrom(T&& value)
{
    return Variant(std::forward<T>(value));
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<ValueTypeOf<T>>>
Variant Variant::convertFrom(const T& value)
{
    Variant v;
    ToVariantConverter conv(v);
    convert(conv, const_cast<T&>(value));
    return v;
}

//------------------------------------------------------------------------------
template <typename T, Variant::EnableIfVariantArg<ValueTypeOf<T>>>
Variant Variant::convertFrom(const T& variant)
{
    return variant;
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<T>>
Variant Variant::convertFrom(const std::vector<T>& vec)
{
    Variant::Array array;
    for (const auto& elem: vec)
        array.emplace_back(Variant::convertFrom(elem));
    return Variant(std::move(array));
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<T>>
Variant Variant::convertFrom(const std::map<String, T>& map)
{
    Variant::Object object;
    for (const auto& kv: map)
        object.emplace(kv.first, Variant::convertFrom(kv.second));
    return Variant(std::move(object));
}

//------------------------------------------------------------------------------
template <typename T, Variant::EnableIfValidArg<T>>
void Variant::convertTo(T& value) const
{
    applyWithOperand(ConvertTo(), *this, value);
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<T>>
void Variant::convertTo(T& value) const
{
    FromVariantConverter conv(*this);
    convert(conv, value);
}

//------------------------------------------------------------------------------
template <typename T, Variant::EnableIfVariantArg<T>>
void Variant::convertTo(T& variant) const
{
    variant = *this;
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<T>>
void Variant::convertTo(std::vector<T>& vec) const
{
    const auto& array = this->as<Array>();
    for (const auto& elem: array)
    {
        T value;
        elem.convertTo(value);
        vec.emplace_back(std::move(value));
    }
}

//------------------------------------------------------------------------------
template <typename T, Variant::DisableIfValidArg<T>>
void Variant::convertTo(std::map<String, T>& map) const
{
    const auto& object = this->as<Object>();
    for (const auto& kv: object)
    {
        T value;
        kv.second.convertTo(value);
        map.emplace(std::move(kv.first), std::move(value));
    }
}

//------------------------------------------------------------------------------
template <typename TField, typename V> TField& Variant::get(V&& variant)
{
    using FieldType = typename std::remove_const<TField>::type;
    static_assert(FieldTraits<FieldType>::isValid, "Invalid field type");
    if (!variant.template is<FieldType>())
        throw error::Access(wamp::typeNameOf(variant),
                            FieldTraits<FieldType>::typeName());
    return Access<FieldType>::get(&variant.field_);
}

//------------------------------------------------------------------------------
inline std::ostream& Variant::output(std::ostream& out, const Array& array)
{
    internal::Output visitor;
    visitor(array, out);
    return out;
}

//------------------------------------------------------------------------------
inline std::ostream& Variant::output(std::ostream& out, const Object& object)
{
    internal::Output visitor;
    visitor(object, out);
    return out;
}

//------------------------------------------------------------------------------
inline std::ostream& Variant::output(std::ostream& out,
                                      const Variant& variant)
{
    applyWithOperand(internal::Output(), variant, out);
    return out;
}

//------------------------------------------------------------------------------
inline Variant::Field::Field() : nullValue(null) {}

//------------------------------------------------------------------------------
inline Variant::Field::~Field() {}

//------------------------------------------------------------------------------
inline void swap(Variant& v, Variant& w) noexcept {v.swap(w);}

//------------------------------------------------------------------------------
inline bool isNumber(const Variant& v)
{
    using T = TypeId;
    switch(v.typeId())
    {
    case T::integer: case T::uint: case T::real:
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
inline bool isScalar(const Variant& v)
{
    using T = TypeId;
    switch(v.typeId())
    {
    case T::boolean: case T::integer: case T::uint: case T::real:
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
inline Variant::String typeNameOf(const Variant& v)
{
    return apply(internal::TypeName(), v);
}

//------------------------------------------------------------------------------
template <typename TBound>
Variant::String typeNameOf()
{
    return internal::FieldTraits<TBound>::typeName();
}

//------------------------------------------------------------------------------
template <typename T>
bool operator==(const Variant& variant, const T& value)
{
    static_assert(internal::ArgTraits<T>::isValid,
                  "Invalid value type");
    return applyWithOperand(internal::EquivalentTo(), variant, value);
}

//------------------------------------------------------------------------------
template <typename T>
bool operator==(const T& value, const Variant& variant)
{
    return variant == value;
}

//------------------------------------------------------------------------------
inline bool operator==(const Variant& variant, const Variant::CharType* str)
{
    return variant.is<String>() && (variant.as<String>() == str);
}

//------------------------------------------------------------------------------
inline bool operator==(const Variant::CharType* str, const Variant& variant)
{
    return variant == str;
}

//------------------------------------------------------------------------------
inline bool operator==(const Variant& variant, Variant::CharType* str)
{
    return variant == static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
inline bool operator==(Variant::CharType* str, const Variant& variant)
{
    return variant == static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
template <typename T>
bool operator!=(const Variant& variant, const T& value)
{
    static_assert(internal::ArgTraits<T>::isValid,
                  "Invalid value type");
    return applyWithOperand(internal::NotEquivalentTo(), variant, value);
}

//------------------------------------------------------------------------------
template <typename T>
bool operator!=(const T& value, const Variant& variant)
{
    return variant != value;
}

//------------------------------------------------------------------------------
inline bool operator!=(const Variant& variant, const Variant::CharType* str)
{
    return !variant.is<String>() || (variant.as<String>() != str);
}

//------------------------------------------------------------------------------
inline bool operator!=(const Variant::CharType* str, const Variant& variant)
{
    return variant != str;
}

//------------------------------------------------------------------------------
inline bool operator!=(const Variant& variant, Variant::CharType* str)
{
    return variant != static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
inline bool operator!=(Variant::CharType* str, const Variant& variant)
{
    return variant != static_cast<const Variant::CharType*>(str);
}

} // namespace wamp
