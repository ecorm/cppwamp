/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2018, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VARIANT_HPP
#define CPPWAMP_VARIANT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of Variant and other closely related
           types/functions. */
//------------------------------------------------------------------------------

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "api.hpp"
#include "blob.hpp"
#include "conversionaccess.hpp"
#include "exceptions.hpp"
#include "null.hpp"
#include "traits.hpp"
#include "variantdefs.hpp"
#include "visitor.hpp"
#include "internal/varianttraits.hpp"
#include "internal/variantvisitors.hpp"

//------------------------------------------------------------------------------
/** Splits the `convert` free function for the given custom type.

When split, the user must provide overloads for the `covertFrom` and
`convertTo` functions. This can be useful when different behavior is
required when converting to/from custom types.

The `convertFrom` function converts from a Variant to the custom type, and
should have the following signature:
```
void convertFrom(FromVariantConverter&, Type&)
```

The `convertTo` function converts to a Variant from a custom type, and
should have the following signature:
```
void convertTo(ToVariantConverter&, const Type&)
```
*/
//------------------------------------------------------------------------------
#define CPPWAMP_CONVERSION_SPLIT_FREE(Type)                     \
inline void convert(::wamp::FromVariantConverter& c, Type& obj) \
{                                                               \
    convertFrom(c, obj);                                        \
}                                                               \
                                                                \
inline void convert(::wamp::ToVariantConverter& c, Type& obj)   \
{                                                               \
    convertTo(c, const_cast<const Type&>(obj));                 \
}

//------------------------------------------------------------------------------
/** Splits the `convert` member function for the given custom type.

When split, the user must provide the `covertFrom` and `convertTo`
member functions. This can be useful when different behavior is
required when converting to/from custom types.

The `convertFrom` member function converts from a Variant to the custom
type, and should have the following signature:
```
void CustomType::convertFrom(FromVariantConverter&)
```

The `convertTo` member function converts to a Variant from a custom type,
and should have the following signature:
```
void CustomType::convertTo(ToVariantConverter&) const
```
*/
//------------------------------------------------------------------------------
#define CPPWAMP_CONVERSION_SPLIT_MEMBER(Type)                   \
inline void convert(::wamp::FromVariantConverter& c, Type& obj) \
{                                                               \
    wamp::ConversionAccess::convertFrom(c, obj);                \
}                                                               \
                                                                \
inline void convert(::wamp::ToVariantConverter& c, Type& obj)   \
{                                                               \
    const auto& constObj = obj;                                 \
    wamp::ConversionAccess::convertTo(c, constObj);             \
}


namespace wamp
{

//------------------------------------------------------------------------------
// Forward declaration
namespace internal {template <TypeId typeId> struct FieldTypeForId;}

//------------------------------------------------------------------------------
/** Discriminated union container that represents a JSON value.

    A Variant behaves similarly to a dynamically-typed Javascript variable.
    Their underlying type can change at runtime, depending on the actual
    values assigned to them. Variants play a central role in CppWAMP, as they
    are used to represent dynamic data exchanged with a WAMP peer.

    Variants can hold any of the following value types:
    - @ref Null : represents an empty or missing value
    - @ref Bool : true or false
    - **Numbers**: as integer (@ref Int, @ref UInt),
                   or floating point (@ref Real)
    - @ref String : only UTF-8 encoded strings are currently supported
    - @ref Blob : binary data as an array of bytes
    - @ref Array : dynamically-sized lists of variants
    - @ref Object : dictionaries having string keys and variant values

    Array and object variants are recursive composites: their element values
    are also variants which can themselves be arrays or objects.

    The Args struct is a value type that bundles one or more variants into
    positional and/or keyword arguments, to be exchanged with a WAMP peer.

    @see Args */
//------------------------------------------------------------------------------
class CPPWAMP_API Variant
{
private:
    template <typename TField, typename Enable = void>
    using ArgTraits = internal::ArgTraits<TField>;

public:
    /// @name Metafunctions
    /// @{
    /** Obtains the bound type associated with a particular @ref TypeId. */
    template <TypeId typeId>
    using BoundTypeForId = typename internal::FieldTypeForId<typeId>::Type;

    /** Indicates that the given argument type is convertible
        to a bound type. */
    template <typename T>
    static constexpr bool isValidArg() noexcept
    {
        return ArgTraits<ValueTypeOf<T>>::isValid;
    }

    /* Indicates that the given argument type is not convertible
       to a bound type. */
    template <typename T>
    static constexpr bool isInvalidArg() noexcept
    {
        return !ArgTraits<ValueTypeOf<T>>::isValid &&
               !isSameType<T, Variant>();
    }

    /* Indicates that the given argument type is a Variant. */
    template <typename T>
    static constexpr bool isVariantArg() noexcept
    {
        return isSameType<ValueTypeOf<T>, Variant>();
    }

    /// @}

    /// @name Bound Types
    /// @{
    using Null   = wamp::Null;   ///< Represents an empty value
    using Bool   = wamp::Bool;   ///< Boolean type
    using Int    = wamp::Int;    ///< Signed integer type
    using UInt   = wamp::UInt;   ///< Unsigned integer type
    using Real   = wamp::Real;   ///< Floating-point number type
    using String = wamp::String; ///< String type
    using Blob   = wamp::Blob;   ///< Binary data as an array of bytes
    using Array  = wamp::Array;  ///< Dynamic array of variants
    using Object = wamp::Object; ///< Dictionary of variants
    /// @}

    /** Integer type used to access array elements. */
    using SizeType = Array::size_type;

    /** Character type used by string variants. */
    using CharType = String::value_type;

    /// @name Construction
    /// @{

    /** Constructs a variant from a custom type. */
    template <typename TValue>
    static Variant from(TValue&& value);

    /** Constructs a null variant. */
    Variant() noexcept;

    /** Copy constructor. */
    Variant(const Variant& other);

    /** Move constructor. */
    Variant(Variant&& other) noexcept;

    /** Converting constructor taking an initial value. */
    template <typename T, CPPWAMP_NEEDS(Variant::isValidArg<T>()) = 0>
    Variant(T&& value);

    /** Converting constructor taking an initial Array value. */
    Variant(Array array);

    /** Converting constructor taking a `std::vector` of initial Array
        values. */
    template <typename T,
             CPPWAMP_NEEDS(Variant::isValidArg<std::vector<T>>()) = 0>
    Variant(std::vector<T> vec);

    /** Converting constructor taking an initial Object value. */
    Variant(Object object);

    /** Converting constructor taking a `std::map` of key-value pairs. */
    template <typename T,
              CPPWAMP_NEEDS((Variant::isValidArg<std::map<String, T>>())) = 0>
    Variant(std::map<String, T> map);

    /** Destructor. */
    ~Variant();

    /// @}

    /// @name Observers
    /// @{

    /** Returns the id of the variant's current dynamic type. */
    TypeId typeId() const;

    /** Returns `false` iff the variant is currently null. */
    explicit operator bool() const;

    /** Returns `true` iff the variant's current dynamic type matches the
        given `TBound` type parameter. */
    template <typename TBound> bool is() const;

    /** Returns `true` iff the variant's current dynamic type matches the
        given `id` template parameter. */
    template <TypeId id> bool is() const;

    /** Converts the variant's bound value to the given type. */
    template <typename T> T to() const;

    /** Converts the variant's bound value to the given type, and assigns the
        result to the given `value` reference. */
    template <typename T> void to(T& value) const;

    /** Obtains the variant's value converted to the given type, or the given
        fallback value if the variant is null. */
    template <typename T>
    ValueTypeOf<T> valueOr(T&& fallback) const;

    /** Returns the number of elements contained by the variant. */
    SizeType size() const;

    /// @}

    /// @name Access
    /// @{

    /** Returns a reference to the variant's bound value. */
    template <typename TBound> TBound& as();

    /** Returns a constant reference to the variant's bound value. */
    template <typename TBound> const TBound& as() const;

    /** Returns a reference to the variant's bound value. */
    template <TypeId id> BoundTypeForId<id>& as();

    /** Returns a constant reference to the variant's bound value. */
    template <TypeId id> const BoundTypeForId<id>& as() const;

    /** Accesses an array element by index. */
    Variant& operator[](SizeType index);

    /** Accesses a constant array element by index. */
    const Variant& operator[](SizeType index) const;

    /** Accesses an array element by index. */
    Variant& at(SizeType index);

    /** Accesses a constant array element by index. */
    const Variant& at(SizeType index) const;

    /** Accesses an object value by key. */
    Variant& operator[](const String& key);

    /** Accesses an object value by key. */
    Variant& at(const String& key);

    /** Accesses a constant object value by key. */
    const Variant& at(const String& key) const;

    /// @}

    /// @name Comparison
    /// @{

    /** Compares two variants for equality. */
    bool operator==(const Variant& other) const;

    /** Compares two variants for inequality. */
    bool operator!=(const Variant& other) const;

    /** Less-than comparison between two variants. */
    bool operator<(const Variant& other) const;

    /// @}

    /// @name Modifiers
    /// @{

    /** Assigns a variant onto another variant. */
    Variant& operator=(const Variant& other);

    /** Move-assigns from one variant into another. */
    Variant& operator=(Variant&& other) noexcept;

    /** Assigns a value to a variant. */
    template <typename T, CPPWAMP_NEEDS((Variant::isValidArg<T>())) = 0>
    Variant& operator=(T&& value);

    /** Assigns an array variant to a variant. */
    Variant& operator=(Array array);

    /** Assigns a `std::vector` to a variant. */
    template <typename T,
              CPPWAMP_NEEDS((Variant::isValidArg<std::vector<T>>())) = 0>
    Variant& operator=(std::vector<T> vec);

    /** Assigns an object variant to a variant. */
    Variant& operator=(Object object);

    /** Assigns a `std::map` to a variant. */
    template <typename T,
              CPPWAMP_NEEDS((Variant::isValidArg<std::map<String, T>>())) = 0>
    Variant& operator=(std::map<String, T> map);

    /** Swaps two variants. */
    void swap(Variant& other) noexcept;

    /// @}

private:
    class CPPWAMP_HIDDEN Construct;
    class CPPWAMP_HIDDEN MoveConstruct;
    class CPPWAMP_HIDDEN MoveAssign;
    class CPPWAMP_HIDDEN Destruct;
    class CPPWAMP_HIDDEN Swap;
    class CPPWAMP_HIDDEN ElementCount;
    class CPPWAMP_HIDDEN LessThan;
    class CPPWAMP_HIDDEN Output;

    template <typename TField>
    using FieldTraits = internal::FieldTraits<TField>;

    template <typename TField>
    using Access = internal::Access<TField>;

    template <typename... TFields> struct CPPWAMP_HIDDEN TypeMask;

    template <TypeId... typeIds> struct CPPWAMP_HIDDEN TypeIdMask;

    template <typename TValue, typename TArg>
    CPPWAMP_HIDDEN void constructAs(TArg&& value);

    template <typename TField>
    CPPWAMP_HIDDEN void destructAs();

    // Can't be hidden; invoked from inline operator=(T)
    void destruct();

    template <typename T, Needs<Variant::isValidArg<T>()> = 0>
    CPPWAMP_HIDDEN static Variant convertFrom(T&& value);

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN static Variant convertFrom(const T& value);

    template <typename T, Needs<Variant::isVariantArg<T>()> = 0>
    CPPWAMP_HIDDEN static Variant convertFrom(const T& variant);

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN static Variant convertFrom(const std::vector<T>& vec);

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN static Variant convertFrom(const std::map<String, T>& map);

    template <typename T, Needs<Variant::isValidArg<T>()> = 0>
    CPPWAMP_HIDDEN void convertTo(T& value) const;

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN void convertTo(T& value) const;

    template <typename T, Needs<Variant::isVariantArg<T>()> = 0>
    CPPWAMP_HIDDEN void convertTo(T& variant) const;

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN void convertTo(std::vector<T>& vec) const;

    template <typename T, Needs<Variant::isInvalidArg<T>()> = 0>
    CPPWAMP_HIDDEN void convertTo(std::map<String, T>& map) const;

    template <typename TField, typename V>
    CPPWAMP_HIDDEN static TField& get(V&& variant);

    union Field
    {
        Field();
        ~Field();
        Null    nullValue;
        Bool    boolean;
        Int     integer;
        UInt    uint;
        Real    real;
        String  string;
        Blob*   blob;
        Array*  array;
        Object* object;
    } field_;

    TypeId typeId_;
};


//------------------------------------------------------------------------------
/// @name Non-member Modifiers
//------------------------------------------------------------------------------
/// @{

/** Swaps two variant objects.
    @see Variant::swap
    @relates Variant */
CPPWAMP_API void swap(Variant& v, Variant& w) noexcept;

/// @}

//------------------------------------------------------------------------------
/// @name Non-member Observers
//------------------------------------------------------------------------------
/// @{

/** Returns `true` iff the variant's current dynamic type is numeric.
    The numeric bound types are:
    - Variant::Int
    - Variant::UInt
    - Variant::Real
    @note Variant::Bool is not considered a numeric type.
    @relates Variant */
CPPWAMP_API bool isNumber(const Variant& v);

/** Returns `true` iff the variant's current dynamic type is scalar.
    The scalar bound types are:
    - Variant::Bool
    - Variant::Int
    - Variant::UInt
    - Variant::Real
    @relates Variant */
CPPWAMP_API bool isScalar(const Variant& v);

/** Returns a textual representation of the variant's current dynamic type.
This function is intended for diagnostic purposes. Equivalent to:
```
Variant::typeNameOf<CurrentBoundType>()
```
@relates Variant */
CPPWAMP_API Variant::String typeNameOf(const Variant& v);

/** Returns a textual representation of a bound type.
    This function is intended for diagnostic purposes.
    @tparam TBound The bound type from which a textual representation
                   will be returned.
    @relates Variant */
template <typename TBound>
CPPWAMP_API Variant::String typeNameOf();

/// @}

//------------------------------------------------------------------------------
/// @name Non-member Output
//------------------------------------------------------------------------------
/// @{
/** Outputs the given Array to the given output stream. @relates Variant */
CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Array& a);

/** Outputs the given Object to the given output stream. @relates Variant */
CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Object& o);

/** Outputs the given Variant to the given output stream. @relates Variant */
CPPWAMP_API std::ostream& operator<<(std::ostream& out, const Variant& v);

/** Outputs the given Array to a new string. @relates Variant */
CPPWAMP_API std::string toString(const Array& a);

/** Outputs the given Object to a new string. @relates Variant */
CPPWAMP_API std::string toString(const Object& o);

/** Outputs the given Variant to a new string. @relates Variant */
CPPWAMP_API std::string toString(const Variant& v);
/// @}


//------------------------------------------------------------------------------
/// @name Non-member Comparison
//------------------------------------------------------------------------------
/// @{

/** Compares a variant with a non-variant value for equality.
    The comparison is performed according to the following matrix:
| Value, Variant->      | Null  | Bool  | Int   | UInt  | Real  | String | Blob  | Array | Object |
|-----------------------|-------|-------|-------|-------|-------|--------|-------|-------|--------|
| Null                  | true  | false | false | false | false | false  | false | false | false  |
| Bool                  | false | L==R  | false | false | false | false  | false | false | false  |
| _integer type_        | false | false | L==R  | L==R  | L==R  | false  | false | false | false  |
| _floating point type_ | false | false | L==R  | L==R  | L==R  | false  | false | false | false  |
| String                | false | false | false | false | false | L==R   | false | false | false  |
| Blob                  | false | false | false | false | false | false  | L==R  | false | false  |
| Array                 | false | false | false | false | false | false  | false | L==R  | false  |
| std::vector<T>        | false | false | false | false | false | false  | false | L==R  | false  |
| Object                | false | false | false | false | false | false  | false | false | L==R   |
| std::map<String,T>    | false | false | false | false | false | false  | false | false | L==R   |
    @relates Variant */
template <typename T>
CPPWAMP_API bool operator==(const Variant& variant, const T& value);

/** Compares a non-variant value with a variant for equality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
CPPWAMP_API bool operator==(const T& value, const Variant& variant);

/** Compares a variant with a null-terminated constant character array
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
CPPWAMP_API bool operator==(const Variant& variant,
                            const Variant::CharType* str);

/** Compares a null-terminated constant character array with a variant
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
CPPWAMP_API bool operator==(const Variant::CharType* str,
                            const Variant& variant);

/** Compares a variant with a null-terminated mutable character array
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
CPPWAMP_API bool operator==(const Variant& variant, Variant::CharType* str);

/** Compares a null-terminated mutable character array with a variant
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
CPPWAMP_API bool operator==(Variant::CharType* str, const Variant& variant);

/** Compares a variant with a non-variant value for inequality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
bool operator!=(const Variant& variant, const T& value);

/** Compares a non-variant value with a variant for inequality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
CPPWAMP_API bool operator!=(const T& value, const Variant& variant);

/** Compares a variant with a null-terminated constant character array
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
CPPWAMP_API bool operator!=(const Variant& variant,
                            const Variant::CharType* str);

/** Compares a null-terminated constant character array with a variant
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
CPPWAMP_API bool operator!=(const Variant::CharType* str,
                            const Variant& variant);

/** Compares a variant with a null-terminated mutable character array
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
CPPWAMP_API bool operator!=(const Variant& variant, Variant::CharType* str);

/** Compares a null-terminated mutable character array with a variant
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
CPPWAMP_API bool operator!=(Variant::CharType* str, const Variant& variant);

/// @}


//------------------------------------------------------------------------------
/** Wrapper around a destination Variant, used for conversions.
    This wrapper provides a convenient, uniform syntax for inserting values
    into a destination variant. */
//------------------------------------------------------------------------------
class CPPWAMP_API ToVariantConverter
{
public:
    /// Integer type used to represent the size of array variants.
    using SizeType = size_t;

    /// String type used to represent an object variant key.
    using String = std::string;

    /** Indicates that this converter is used to convert **to** a variant. */
    static constexpr bool convertingToVariant = true;

    /** Constructor taking a variant reference. */
    explicit ToVariantConverter(Variant& var);

    /** Makes the variant become an Array variant. */
    ToVariantConverter& size(SizeType n);

    /** Assigns a value to the variant. */
    template <typename T>
    ToVariantConverter& operator()(T&& value);

    /** Appends an array element to the variant. */
    template <typename T>
    ToVariantConverter& operator[](T&& value);

    /** Appends an object member to the variant. */
    template <typename T>
    ToVariantConverter& operator()(String key, T&& value);

    /** Appends an object member to the variant. */
    template <typename T, typename U>
    ToVariantConverter& operator()(String key, T&& value, U&& ignored);

    /** Returns a reference to the wrapped variant. */
    Variant& variant();

private:
    Variant& var_;
};

//------------------------------------------------------------------------------
/** Wrapper around a source Variant, used for conversions.
    This wrapper provides a convenient, uniform syntax for retrieving values
    from a source variant. */
//------------------------------------------------------------------------------
class CPPWAMP_API FromVariantConverter
{
public:
    /// Integer type used to represent the size of array variants.
    using SizeType = size_t;

    /// String type used to represent an object variant key.
    using String = std::string;

    /** Indicates that this converter is used to convert **from** a variant. */
    static constexpr bool convertingToVariant = false;

    /** Constructor taking a constant variant reference. */
    explicit FromVariantConverter(const Variant& var);

    /** Obtains the current size of the variant. */
    SizeType size() const;

    /** Obtains the current size of the variant. */
    FromVariantConverter& size(SizeType& n);

    /** Retrieves a non-composite value from the variant. */
    template <typename T>
    FromVariantConverter& operator()(T& value);

    /** Retrieves the next element from an Array variant. */
    template <typename T>
    FromVariantConverter& operator[](T& value);

    /** Retrieves a member from an Object variant. */
    template <typename T>
    FromVariantConverter& operator()(const String& key, T& value);

    /** Retrieves a member from an Object variant, with a fallback value
        if the member is not found. */
    template <typename T, typename U>
    FromVariantConverter& operator()(const String& key, T& value, U&& fallback);

    /** Returns a constant reference to the wrapped variant. */
    const Variant& variant() const;

private:
    const Variant& var_;
    SizeType index_ = 0;
};


//------------------------------------------------------------------------------
/** General function for converting custom types to/from Variant.
    You must overload this function for custom types that you want to be
    convertible to/from Variant. Alternatively, you may also:
    - provide a `convert` member function within your custom type,
    - use @ref CPPWAMP_CONVERSION_SPLIT_FREE and provide split `convertTo`
      and `convertFrom` free functions, or,
    - use @ref CPPWAMP_CONVERSION_SPLIT_MEMBER and provide split `convertTo`
      and `convertFrom` member functions. */
//------------------------------------------------------------------------------
template <typename TConverter, typename TValue,
          Needs<!std::is_enum<TValue>::value> = 0>
CPPWAMP_API inline void convert(TConverter& c, TValue& val)
{
    // Fall back to intrusive conversion if 'convert' was not specialized
    // for the given type.
    ConversionAccess::convert(c, val);
}

//------------------------------------------------------------------------------
/** Converts an integer variant to an enumerator. */
//------------------------------------------------------------------------------
template <typename TEnum, Needs<std::is_enum<TEnum>::value> = 0>
CPPWAMP_API inline void convert(const FromVariantConverter& c, TEnum& e)
{
    using U = typename std::underlying_type<TEnum>::type;
    auto n = c.variant().to<U>();
    e = static_cast<TEnum>(n);
}

//------------------------------------------------------------------------------
/** Converts an enumerator to an integer variant. */
//------------------------------------------------------------------------------
template <typename TEnum, Needs<std::is_enum<TEnum>::value> = 0>
CPPWAMP_API inline void convert(ToVariantConverter& c, const TEnum& e)
{
    using U = typename std::underlying_type<TEnum>::type;
    c.variant() = static_cast<U>(e);
}


//******************************************************************************
// Variant template member function implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename TValue>
Variant Variant::from(TValue&& value)
{
    return convertFrom(std::forward<TValue>(value));
}

//------------------------------------------------------------------------------
/** @details
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @see Variant::from
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `*this == value` */
//------------------------------------------------------------------------------
template <typename T, CPPWAMP_NEEDS(Variant::isValidArg<T>())>
Variant::Variant(T&& value)
{
    using FieldType = typename ArgTraits<ValueTypeOf<T>>::FieldType;
    typeId_ = FieldTraits<FieldType>::typeId;
    constructAs<FieldType>(std::forward<T>(value));
}

//------------------------------------------------------------------------------
/** @details
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `this->is<Array>() == true`
    @post `*this == vec` */
//------------------------------------------------------------------------------
template <typename T, CPPWAMP_NEEDS(Variant::isValidArg<std::vector<T>>())>
Variant::Variant(std::vector<T> vec)
    : typeId_(TypeId::array)
{
    Array array;
    array.reserve(vec.size());
    std::move(vec.begin(), vec.end(), std::back_inserter(array));
    constructAs<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @details
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `this->is<Object>() == true`
    @post `*this == map` */
//------------------------------------------------------------------------------
template <typename T,
         CPPWAMP_NEEDS((Variant::isValidArg<std::map<String, T>>()))>
Variant::Variant(std::map<String, T> map)
    : typeId_(TypeId::object)
{
    Object object;
    std::move(map.begin(), map.end(), std::inserter(object, object.begin()));
    constructAs<Object>(std::move(object));
}

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
/** @details
    The variant's dynamic type will change to accomodate the assigned
    value.
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `*this == value` */
//------------------------------------------------------------------------------
template <typename T, CPPWAMP_NEEDS((Variant::isValidArg<T>()))>
Variant& Variant::operator=(T&& value)
{
    using FieldType = typename ArgTraits<ValueTypeOf<T>>::FieldType;
    if (is<FieldType>())
        as<FieldType>() = std::forward<T>(value);
    else
    {
        destruct();
        constructAs<FieldType>(std::forward<T>(value));
        typeId_ = FieldTraits<FieldType>::typeId;
    }
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `this->typeId() == TypeId::array`
    @post `*this == vec` */
//------------------------------------------------------------------------------
template <typename T,
         CPPWAMP_NEEDS((Variant::isValidArg<std::vector<T>>()))>
Variant& Variant::operator=(std::vector<T> vec)
{
    Array array;
    array.reserve(vec.size());
    std::move(vec.begin(), vec.end(), std::back_inserter(array));
    return *this = std::move(array);
}

//------------------------------------------------------------------------------
/** @details
    Only participates in overload resolution when
    `Variant::isValidArg<T>() == true`.
    @tparam T (Deduced) Value type which must be convertible to a bound type.
    @post `this->typeId() == TypeId::object`
    @post `*this == map` */
//------------------------------------------------------------------------------
template <typename T,
         CPPWAMP_NEEDS((Variant::isValidArg<std::map<String, T>>()))>
Variant& Variant::operator=(std::map<String,T> map)
{
    Object object(map.cbegin(), map.cend());
    return *this = std::move(object);
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
template <typename T, Needs<Variant::isValidArg<T>()>>
Variant Variant::convertFrom(T&& value)
{
    return Variant(std::forward<T>(value));
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isInvalidArg<T>()>>
Variant Variant::convertFrom(const T& value)
{
    Variant v;
    ToVariantConverter conv(v);
    convert(conv, const_cast<T&>(value));
    return v;
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isVariantArg<T>()>>
Variant Variant::convertFrom(const T& variant)
{
    return variant;
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isInvalidArg<T>()>>
Variant Variant::convertFrom(const std::vector<T>& vec)
{
    Variant::Array array;
    for (const auto& elem: vec)
        array.emplace_back(Variant::convertFrom(elem));
    return Variant(std::move(array));
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isInvalidArg<T>()>>
Variant Variant::convertFrom(const std::map<String, T>& map)
{
    Variant::Object object;
    for (const auto& kv: map)
        object.emplace(kv.first, Variant::convertFrom(kv.second));
    return Variant(std::move(object));
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isValidArg<T>()>>
void Variant::convertTo(T& value) const
{
    applyWithOperand(internal::VariantConvertTo<Variant>(), *this, value);
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isInvalidArg<T>()>>
void Variant::convertTo(T& value) const
{
    FromVariantConverter conv(*this);
    convert(conv, value);
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isVariantArg<T>()>>
void Variant::convertTo(T& variant) const
{
    variant = *this;
}

//------------------------------------------------------------------------------
template <typename T, Needs<Variant::isInvalidArg<T>()>>
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
template <typename T, Needs<Variant::isInvalidArg<T>()>>
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
    return applyWithOperand(internal::VariantEquivalentTo<Variant>(), variant,
                            value);
}

//------------------------------------------------------------------------------
template <typename T>
bool operator==(const T& value, const Variant& variant)
{
    return variant == value;
}

//------------------------------------------------------------------------------
template <typename T>
bool operator!=(const Variant& variant, const T& value)
{
    static_assert(internal::ArgTraits<T>::isValid,
                  "Invalid value type");
    return applyWithOperand(internal::VariantNotEquivalentTo<Variant>(),
                            variant, value);
}

//------------------------------------------------------------------------------
template <typename T>
bool operator!=(const T& value, const Variant& variant)
{
    return variant != value;
}


//******************************************************************************
// ToVariantConverter implementation
//******************************************************************************

//------------------------------------------------------------------------------
inline ToVariantConverter::ToVariantConverter(Variant& var) : var_(var) {}

//------------------------------------------------------------------------------
/** @details
    The array will `reserve` space for `n` elements.
    @post `this->variant().is<Array> == true`
    @post `this->variant().as<Array>.capacity() >= n` */
//------------------------------------------------------------------------------
inline ToVariantConverter& ToVariantConverter::size(SizeType n)
{
    Array array;
    array.reserve(n);
    var_ = std::move(array);
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The given value is converted to a Variant via Variant::from before
    being assigned. */
//------------------------------------------------------------------------------
template <typename T>
ToVariantConverter& ToVariantConverter::operator()(T&& value)
{
    var_ = Variant::from(std::forward<T>(value));
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    If the destination Variant is not already an Array, it will be transformed
    into an array and all previously stored values will be cleared.
    @post `this->variant().is<Array> == true` */
//------------------------------------------------------------------------------
template <typename T>
ToVariantConverter& ToVariantConverter::operator[](T&& value)
{
    if (!var_.is<Array>())
        var_ = Array();
    auto& array = var_.as<Array>();
    array.emplace_back(Variant::from(std::forward<T>(value)));
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    If the destination Variant is not already an Object, it will be transformed
    into an object and all previously stored values will be cleared.
    @post `this->variant().is<Object> == true` */
//------------------------------------------------------------------------------
template <typename T>
ToVariantConverter& ToVariantConverter::operator()(String key, T&& value)
{
    if (!var_.is<Object>())
        var_ = Object();
    auto& object = var_.as<Object>();
    object.emplace( std::move(key),
                   Variant::from(std::forward<T>(value)) );
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    If the destination Variant is not already an Object, it will be transformed
    into an object and all previously stored values will be cleared.
    @post `this->variant().is<Object> == true` */
//------------------------------------------------------------------------------
template <typename T, typename U>
ToVariantConverter& ToVariantConverter::operator()(String key, T&& value, U&&)
{
    return operator()(std::move(key), std::forward<T>(value));
}

inline Variant& ToVariantConverter::variant() {return var_;}


//******************************************************************************
// FromVariantConverter implementation
//******************************************************************************

//------------------------------------------------------------------------------
inline FromVariantConverter::FromVariantConverter(const Variant& var)
    : var_(var)
{}

//------------------------------------------------------------------------------
/** @details
    Returns this->variant()->size().
    @see Variant::size */
//------------------------------------------------------------------------------
inline FromVariantConverter::SizeType FromVariantConverter::size() const
{
    return var_.size();
}

//------------------------------------------------------------------------------
/** @details
    Returns this->variant()->size().
    @see Variant::size */
//------------------------------------------------------------------------------
inline FromVariantConverter& FromVariantConverter::size(SizeType& n)
{
    n = var_.size();
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The variant's value is converted to the destination type via
    Variant::to.
    @pre The variant is convertible to the destination type.
    @throws error::Conversion if the variant is not convertible to the
            destination type. */
//------------------------------------------------------------------------------
template <typename T>
FromVariantConverter& FromVariantConverter::operator()(T& value)
{
    var_.to(value);
    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The element is converted to the destination type via Variant::to.
    @pre The variant is an array.
    @pre There are elements left in the variant array.
    @pre The element is convertible to the destination type.
    @throws error::Conversion if the variant is not an array.
    @throws error::Conversion if there are no elements left in the
            variant array.
    @throws error::Conversion if the element is not convertible to the
            destination type. */
//------------------------------------------------------------------------------
template <typename T>
FromVariantConverter& FromVariantConverter::operator[](T& value)
{
    try
    {
        var_.at(index_).to(value);
        ++index_;
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for array index " << index_;
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << " as array";
        throw error::Conversion(oss.str());
    }
    catch (const std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Cannot extract more than " << index_
            << " elements from the array";
        throw error::Conversion(oss.str());
    }

    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The member is converted to the destination type via Variant::to.
    @pre The variant is an object.
    @pre There exists a member with the given key.
    @pre The member is convertible to the destination type.
    @throws error::Conversion if the variant is not an object.
    @throws error::Conversion if there doesn't exist a member with the
            given key.
    @throws error::Conversion if the member is not convertible to the
            destination type. */
//------------------------------------------------------------------------------
template <typename T>
FromVariantConverter& FromVariantConverter::operator()(const String& key,
                                                       T& value)
{
    try
    {
        var_.at(key).to(value);
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for object member \"" << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << " as object using key \"" << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Key \"" << key
            << "\" not found in object";
        throw error::Conversion(oss.str());
    }

    return *this;
}

//------------------------------------------------------------------------------
/** @details
    The member is converted to the destination type via Variant::to.
    @pre The variant is an object.
    @pre The member, if it exists, is convertible to the destination type.
    @throws error::Conversion if the variant is not an object.
    @throws error::Conversion if the existing member is not convertible to the
            destination type. */
//------------------------------------------------------------------------------
template <typename T, typename U>
FromVariantConverter& FromVariantConverter::operator()(const String& key,
                                                       T& value, U&& fallback)
{
    try
    {
        auto& obj = var_.as<Object>();
        auto kv = obj.find(key);
        if (kv != obj.end())
            kv->second.to(value);
        else
            value = std::forward<U>(fallback);
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for object member \""  << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << " as object using key \"" << key << '"';
        throw error::Conversion(oss.str());
    }

    return *this;
}

//------------------------------------------------------------------------------
inline const Variant& FromVariantConverter::variant() const {return var_;}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
    #include "internal/variant.ipp"
#endif

#endif // CPPWAMP_VARIANT_HPP
