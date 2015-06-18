/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VARIANT_HPP
#define CPPWAMP_VARIANT_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of Variant and other closely related
    types/functions. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include "null.hpp"
#include "traits.hpp"
#include "internal/varianttraitsfwd.hpp"

namespace wamp
{

namespace error
{

//------------------------------------------------------------------------------
/** Exception type thrown when accessing a Variant as an invalid type. */
//------------------------------------------------------------------------------
struct Access : public std::runtime_error
{
    explicit Access(const std::string& from, const std::string& to);
};

//------------------------------------------------------------------------------
/** Exception type thrown when converting a Variant to an invalid type. */
//------------------------------------------------------------------------------
struct Conversion : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

} // namespace error


//------------------------------------------------------------------------------
/** Integer ID used to indicate the current dynamic type of a `Variant`. */
//------------------------------------------------------------------------------
enum class TypeId : uint8_t
{
    null,       ///< For Variant::Null
    boolean,    ///< For Variant::Bool
    integer,    ///< For Variant::Int
    uint,       ///< For Variant::UInt
    real,       ///< For Variant::Real
    string,     ///< For Variant::String
    array,      ///< For Variant::Array
    object      ///< For Variant::Object
};

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
    - @ref Array : dynamically-sized lists of variants
    - @ref Object : dictionaries having string keys and variant values

    Array and object variants are recursive composites: their element values
    are also variants which can themselves be arrays or objects.

    The Args struct is a value type that bundles one or more variants into
    positional and/or keyword arguments, to be exchanged with a WAMP peer.

    @see Args */
//------------------------------------------------------------------------------
class Variant
{
private:
    template <typename TField, typename Enable = void>
    using ArgTraits = internal::ArgTraits<TField>;

public:
    /// @name Metafunctions
    /// @{
    /** Metafunction used to obtain the bound type associated with a
        particular @ref TypeId. */
    template <TypeId typeId>
    using BoundTypeForId = typename internal::FieldTypeForId<typeId>::Type;

    /** Metafunction used for enabling overloads for valid argument types. */
    template <typename T>
    using EnableIfValidArg = EnableIf<ArgTraits<ValueTypeOf<T>>::isValid>;

    /** Metafunction used for disabling overloads for valid argument types. */
    template <typename T>
    using DisableIfValidArg = DisableIf<ArgTraits<ValueTypeOf<T>>::isValid ||
                                        isSameType<T, Variant>()>;

    /** Metafunction used for enabling overloads for Variant arguments. */
    template <typename T>
    using EnableIfVariantArg = EnableIf<isSameType<T, Variant>()>;

    /// @}

    /// @name Bound Types
    /// @{
    using Null   = ::wamp::Null;                ///< Represents an empty value
    using Bool   = bool;                        ///< Boolean type
    using Int    = std::int64_t;                ///< Signed integer type
    using UInt   = std::uint64_t;               ///< Unsigned integer type
    using Real   = double;                      ///< Floating-point number type
    using String = std::string;                 ///< String type
    using Array  = std::vector<Variant>;        ///< Dynamic array of variants
    using Object = std::map<String, Variant>;   ///< Dictionary of variants
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
    template <typename T, EnableIfValidArg<T> = 0>
    Variant(T value);

    /** Converting constructor taking an initial Array value. */
    Variant(Array array);

    /** Converting constructor taking a `std::vector` of initial Array
        values. */
    template <typename T> Variant(std::vector<T> vec);

    /** Converting constructor taking an initial Object value. */
    Variant(Object object);

    /** Converting constructor taking a `std::map` of key-value pairs. */
    template <typename T> Variant(std::map<String, T> map);

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

    /** Accesses an object value by key. */
    Variant& operator[](const String& key);

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
    template <typename T> Variant& operator=(T value);

    /** Assigns an array variant to a variant. */
    Variant& operator=(Array array);

    /** Assigns a `std::vector` to a variant. */
    template <typename T> Variant& operator=(std::vector<T> vec);

    /** Assigns an object variant to a variant. */
    Variant& operator=(Object object);

    /** Assigns a `std::map` to a variant. */
    template <typename T> Variant& operator=(std::map<String, T> map);

    /** Swaps two variants. */
    void swap(Variant& other) noexcept;

    /// @}

private:
    template <typename TField>
    using FieldTraits = internal::FieldTraits<TField>;

    template <typename TField>
    using Access = internal::Access<TField>;

    template <typename... TFields> struct TypeMask;

    template <TypeId... typeIds> struct TypeIdMask;

    template <typename T> struct Tag {};

    // Visitors
    class TypeName;
    class Construct;
    class MoveConstruct;
    class MoveAssign;
    class Destruct;
    class Swap;
    class LessThan;
    class ConvertibleTo;
    class ConvertTo;
    class ElementCount;

    template <typename TValue, typename TArg> void constructAs(TArg&& value);

    template <typename TField> void destructAs();

    void destruct();

    template <typename T, EnableIfValidArg<ValueTypeOf<T>> = 0>
    static Variant convertFrom(T&& value);

    template <typename T, DisableIfValidArg<ValueTypeOf<T>> = 0>
    static Variant convertFrom(const T& value);

    template <typename T, EnableIfVariantArg<ValueTypeOf<T>> = 0>
    static Variant convertFrom(const T& variant);

    template <typename T, DisableIfValidArg<T> = 0>
    static Variant convertFrom(const std::vector<T>& vec);

    template <typename T, DisableIfValidArg<T> = 0>
    static Variant convertFrom(const std::map<String, T>& map);

    template <typename T, EnableIfValidArg<T> = 0>
    void convertTo(T& value) const;

    template <typename T, DisableIfValidArg<T> = 0>
    void convertTo(T& value) const;

    template <typename T, EnableIfVariantArg<T> = 0>
    void convertTo(T& variant) const;

    template <typename T, DisableIfValidArg<T> = 0>
    void convertTo(std::vector<T>& vec) const;

    template <typename T, DisableIfValidArg<T> = 0>
    void convertTo(std::map<String, T>& map) const;

    template <typename TField, typename V> static TField& get(V&& variant);

    static std::ostream& output(std::ostream& out, const Array& array);

    static std::ostream& output(std::ostream& out, const Object& object);

    static std::ostream& output(std::ostream& out, const Variant& variant);

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
        Array*  array;
        Object* object;
    } field_;

    TypeId typeId_;

    /** Outputs the given Array to the given output stream. */
    friend std::ostream& operator<<(std::ostream& out, const Array& a)
        {return output(out, a);}

    /** Outputs the given Object to the given output stream. */
    friend std::ostream& operator<<(std::ostream& out, const Object& o)
        {return output(out, o);}

    /** Outputs the given Variant to the given output stream. */
    friend std::ostream& operator<<(std::ostream& out, const Variant& v)
        {return output(out, v);}
};


//------------------------------------------------------------------------------
/// @name Non-member Modifiers
//------------------------------------------------------------------------------
/// @{

/** Swaps two variant objects.
    @see Variant::swap
    @relates Variant */
void swap(Variant& v, Variant& w) noexcept;

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
bool isNumber(const Variant& v);

/** Returns `true` iff the variant's current dynamic type is scalar.
    The scalar bound types are:
    - Variant::Bool
    - Variant::Int
    - Variant::UInt
    - Variant::Real
    @relates Variant */
bool isScalar(const Variant& v);

/** Returns a textual representation of the variant's current dynamic type.
    This function is intended for diagnostic purposes. Equivalent to:
    ~~~
    Variant::typeNameOf<CurrentBoundType>()
    ~~~
    @relates Variant */
Variant::String typeNameOf(const Variant& v);

/** Returns a textual representation of a bound type.
    This function is intended for diagnostic purposes.
    @tparam TBound The bound type from which a textual representation
                   will be returned.
    @relates Variant */
template <typename TBound>
Variant::String typeNameOf();

/// @}


//------------------------------------------------------------------------------
/// @name Non-member Comparison
//------------------------------------------------------------------------------
/// @{

/** Compares a variant with a non-variant value for equality.
    The comparison is performed according to the following matrix:
| Value, Variant->      | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|-----------------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null                  | true  | false | false | false | false | false  | false | false  |
| Bool                  | false | L==R  | false | false | false | false  | false | false  |
| _integer type_        | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| _floating point type_ | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| String                | false | false | false | false | false | L==R   | false | false  |
| Array                 | false | false | false | false | false | false  | L==R  | false  |
| std::vector<T>        | false | false | false | false | false | false  | L==R  | false  |
| Object                | false | false | false | false | false | false  | false | L==R   |
| std::map<String,T>    | false | false | false | false | false | false  | false | L==R   |
    @relates Variant */
template <typename T>
bool operator==(const Variant& variant, const T& value);

/** Compares a non-variant value with a variant for equality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
bool operator==(const T& value, const Variant& variant);

/** Compares a variant with a null-terminated constant character array
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
bool operator==(const Variant& variant, const Variant::CharType* str);

/** Compares a null-terminated constant character array with a variant
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
bool operator==(const Variant::CharType* str, const Variant& variant);

/** Compares a variant with a null-terminated mutable character array
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
bool operator==(const Variant& variant, Variant::CharType* str);

/** Compares a null-terminated mutable character array with a variant
    for equality.
    @returns `true` iff `variant.is<String>() && variant.as<String>() == str`
    @relates Variant */
bool operator==(Variant::CharType* str, const Variant& variant);

/** Compares a variant with a non-variant value for inequality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
bool operator!=(const Variant& variant, const T& value);

/** Compares a non-variant value with a variant for inequality.
    @see bool operator==(const Variant& variant, const T& value)
    @relates Variant */
template <typename T>
bool operator!=(const T& value, const Variant& variant);

/** Compares a variant with a null-terminated constant character array
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
bool operator!=(const Variant& variant, const Variant::CharType* str);

/** Compares a null-terminated constant character array with a variant
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
bool operator!=(const Variant::CharType* str, const Variant& variant);

/** Compares a variant with a null-terminated mutable character array
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
bool operator!=(const Variant& variant, Variant::CharType* str);

/** Compares a null-terminated mutable character array with a variant
    for inequality.
    @returns `true` iff `!variant.is<String>() || variant.as<String>() != str`
    @relates Variant */
bool operator!=(Variant::CharType* str, const Variant& variant);

/// @}


//------------------------------------------------------------------------------
/** @name Variant bound types redeclared in namespace for convenience: */
//------------------------------------------------------------------------------
/// @{
using Bool   = Variant::Bool;   ///< Alias for Variant::Bool
using Int    = Variant::Int;    ///< Alias for Variant::Int
using UInt   = Variant::UInt;   ///< Alias for Variant::UInt
using Real   = Variant::Real;   ///< Alias for Variant::Real
using String = Variant::String; ///< Alias for Variant::String
using Array  = Variant::Array;  ///< Alias for Variant::Array
using Object = Variant::Object; ///< Alias for Variant::Object
/// @}


} // namespace wamp

#include "internal/variant.ipp"

#endif // CPPWAMP_VARIANT_HPP
